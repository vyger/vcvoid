// sequencer — Simple eight step (Metropolis-style) sequencer. Spec:
// manual/circuits/sequencer.md. Each clock advances one step through up to 8
// stages; each stage has pitch/cv/gate/slew/repeat. Multiple sequencer circuits
// chain via `chaintonext` into one longer sequence (the head owns clock/outputs
// and reaches into the following instances' step inputs via peer access).
//
// pitchoutput = slew(stagePitch) * outputscaling + transpose. Per-stage `slew`
// linearly limits pitch moves; `repeat` holds a stage for N clocks; `stages`
// caps the number of used steps; `steps` forces a restart after N clocks;
// gate=0 stages emit no gate and hold the previous pitch. gatelength unpatched
// feeds the clock through 1:1; patched, the gate is that fraction of the clock
// period (period learned from consecutive clock edges).
//
// SPEC-GAPs (manual silent / imprecise; literal readings): reset arms a restart
// consumed by the next clock (euklid convention); slew uses the linear
// "seconds per 1 V" mapping of the slew circuit (exact curve unverified);
// slew/scaling/transpose order is slew(raw) then *scaling +transpose; a gate=0
// stage also holds the cv output (manual only pins pitch); before the period is
// known a patched gatelength falls back to clock passthrough; `stages` caps at
// the number of used step inputs (highest patched index).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>
#include <string>
#include <vector>

namespace droid {

class Sequencer : public Circuit {
public:
    void tick(EngineState& s) override {
        // --- build the (possibly chained) flat stage list ------------------
        std::vector<StageRef> stages;
        Circuit* c = this;
        for (;;) {
            int used = usedStages(c, s);
            for (int j = 0; j < used; j++) stages.push_back({c, j});
            // chaintonext (dynamic) extends into the next sequencer circuit.
            if (c->in("chaintonext").value(s) < kGateHighThreshold) break;
            Circuit* nxt = c->nextPeer();
            if (!nxt || !nxt->def || std::string(nxt->def->name) != "sequencer") break;
            c = nxt;
        }
        int N = (int)stages.size();
        if (N < 1) N = 1;

        // --- clock / reset transport ---------------------------------------
        float clockVal = in("clock").value(s);
        bool clockHigh = clockVal >= kGateHighThreshold;
        bool clockEdge = clockGate_.risingEdge(clockVal);
        if (resetGate_.risingEdge(in("reset").value(s))) resetPending_ = true;

        long stepsLimit = std::lround(in("steps").value(s));
        if (stepsLimit < 0) stepsLimit = 0;

        if (clockEdge) {
            if (in("clock").connected()) {                 // learn clock period
                if (haveClock_) period_ = double(s.tick - lastClock_);
                lastClock_ = s.tick; haveClock_ = true;
            }
            if (!started_ || resetPending_) {
                stageIdx_ = 0; pulse_ = 1; clocks_ = 1;
                started_ = true; resetPending_ = false;
            } else {
                clocks_++;
                if (stepsLimit > 0 && clocks_ > stepsLimit) {
                    stageIdx_ = 0; pulse_ = 1; clocks_ = 1;
                } else {
                    int r = repeatAt(stages, stageIdx_ % N, s);
                    if (++pulse_ > r) { stageIdx_ = (stageIdx_ + 1) % N; pulse_ = 1; }
                }
            }
        }
        if (stageIdx_ >= N) stageIdx_ = stageIdx_ % N;

        // --- current stage values ------------------------------------------
        StageRef cur = stages[stageIdx_ < N ? stageIdx_ : 0];
        bool gateOn = cur.c->in("gate", cur.j + 1).value(s) >= kGateHighThreshold;
        if (gateOn) {                                       // muted stage holds
            pitchTarget_ = cur.c->in("pitch", cur.j + 1).value(s);
            cvHeld_ = cur.c->in("cv", cur.j + 1).value(s);
        }

        // --- per-stage linear slew on the pitch ----------------------------
        float slewAmt = cur.c->in("slew", cur.j + 1).value(s);
        if (slewAmt <= 0.0f || !slewInit_) {
            slewOut_ = pitchTarget_; slewInit_ = true;
        } else {
            float maxStep = (0.1f / slewAmt) * s.secondsPerTick();  // 0.1 = 1 V
            float d = pitchTarget_ - slewOut_;
            if (std::fabs(d) <= maxStep) slewOut_ = pitchTarget_;
            else slewOut_ += (d > 0.0f ? maxStep : -maxStep);
        }

        float pitchOut = slewOut_ * in("outputscaling").value(s) + in("transpose").value(s);

        // --- gate output ---------------------------------------------------
        bool glPatched = in("gatelength").connected();
        float gateOut;
        if (!glPatched || period_ <= 0.0) {
            gateOut = (gateOn && clockHigh) ? 1.0f : 0.0f;  // clock passthrough
        } else {
            if (clockEdge && gateOn) {
                double gl = in("gatelength").value(s);
                if (gl < 0.0) gl = 0.0; if (gl > 1.0) gl = 1.0;
                long len = std::lround(gl * period_);
                if (len < 1) len = 1;
                gateUntil_ = s.tick + (uint64_t)len;
            }
            gateOut = (s.tick < gateUntil_) ? 1.0f : 0.0f;
        }

        out("pitchoutput").set(s, pitchOut);
        out("cvoutput").set(s, cvHeld_);
        out("gateoutput").set(s, gateOut);
    }

private:
    struct StageRef { Circuit* c; int j; };  // circuit + 0-based local step index

    // Number of used stages for one circuit: highest patched step index (any of
    // pitch/cv/gate/slew/repeat), optionally capped by that circuit's `stages`.
    static int usedStages(Circuit* c, EngineState& s) {
        int hi = 0;
        for (int j = 1; j <= 8; j++) {
            if (c->in("pitch", j).connected() || c->in("cv", j).connected() ||
                c->in("gate", j).connected() || c->in("slew", j).connected() ||
                c->in("repeat", j).connected())
                hi = j;
        }
        if (hi < 1) hi = 1;
        if (c->in("stages").connected()) {
            long st = std::lround(c->in("stages").value(s));
            if (st < 1) st = 1;
            if (st < hi) hi = (int)st;                       // caps at used inputs
        }
        return hi;
    }

    static int repeatAt(const std::vector<StageRef>& stages, int idx, EngineState& s) {
        const StageRef& st = stages[idx];
        long r = std::lround(st.c->in("repeat", st.j + 1).value(s));
        return r < 1 ? 1 : (int)r;
    }

    GateReader clockGate_, resetGate_;
    int stageIdx_ = 0, pulse_ = 1;
    long clocks_ = 0;
    bool started_ = false, resetPending_ = false, haveClock_ = false, slewInit_ = false;
    uint64_t lastClock_ = 0, gateUntil_ = 0;
    double period_ = 0.0;
    float pitchTarget_ = 0.0f, cvHeld_ = 0.0f, slewOut_ = 0.0f;
};

DROID_REGISTER_CIRCUIT(sequencer, Sequencer)

} // namespace droid
