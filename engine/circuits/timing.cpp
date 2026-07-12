// timing — Shuffle/swing and complex timing generator. Spec:
// manual/circuits/timing.md. Converts a steady input clock into an output clock
// where each step of an up-to-8-step pattern is shifted by a relative fraction
// of one clock cycle (timing1..8, clipped to -0.9999..0.9999; positive = later,
// negative = earlier).
//
// Model: measure the clock period P from consecutive edges. Input beat b (0-
// based) belongs to step (b mod L), L = highest patched timing index. Its output
// target = anchor + timing[step]*P. Each beat is scheduled twice-idempotently:
// once PREDICTIVELY at the previous edge (anchor = predictedEdge = prevEdge+P),
// which lets NEGATIVE (early) shifts fire before the actual edge; and once at the
// beat's OWN edge (anchor = actual tick), which re-syncs to real timing. A beat
// is emitted the first tick its (live-recomputed) target is reached, then never
// replayed (manual: "at max one output beat per input beat"). Timings are read
// live, so reducing a pending beat's delay into the past fires it immediately
// (manual timing.md:96). Coincident beats merge into one trigger (the output is
// a single gate). A later beat whose target falls before an earlier unplayed
// beat's is dropped for good (manual timing.md:91).
//
// SPEC-GAPs (literal readings; manual silent): (1) the first beat has no measured
// period so it cannot be predicted/shifted early -- it fires on time; the first
// NEGATIVE-shifted beat likewise fires on time until P is known (a startup
// transient -- the manual advises a steady clock + reset for a synchronised
// start). Beat 0 always fires on time (any shift needs a known P; shifts take
// effect from beat 1). (2) reset restarts the step counter
// and clears pending/consumed state but keeps the measured period. (3) output
// trigger length = 10 ms (round(0.01*tickRate) ticks), the length used by other
// DROID trigger outputs; the manual states none.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>

namespace droid {

class Timing : public Circuit {
public:
    void tick(EngineState& s) override {
        int L = 0;
        for (int i = 1; i <= 8; i++) if (in("timing", i).connected()) L = i;
        if (L == 0) L = 1;   // smart default: single on-time step (pass-through)

        bool clk = clockGate_.risingEdge(in("clock").value(s));
        bool rst = resetGate_.risingEdge(in("reset").value(s));

        if (rst) {
            beatCount_ = 0;
            consumed_.clear();
            pending_.clear();
        }
        if (clk) {
            long b = beatCount_;
            if (haveFirst_) { P_ = double(s.tick - lastEdge_); haveP_ = true; }
            lastEdge_ = s.tick;
            haveFirst_ = true;
            beatCount_ = b + 1;
            schedule(b, double(s.tick));                    // this beat, actual edge
            if (haveP_) schedule(b + 1, double(s.tick) + P_); // next beat, predicted
        }

        // --- emit processing ---
        double now = double(s.tick);
        bool fired = false;
        double runningMax = -1e18;
        for (auto it = pending_.begin(); it != pending_.end(); ) {
            long seq = it->first;
            double tgt = targetOf(seq, it->second, L, s);
            if (tgt < runningMax - 1e-9) {
                consumed_.insert(seq);                 // later-before-earlier: drop
                it = pending_.erase(it);
                continue;
            }
            runningMax = std::max(runningMax, tgt);
            // Fire at the first tick the target is reached. The 1e-3-tick slack
            // absorbs float noise in shift*P (e.g. 0.3f*20 = 26.0000002) so an
            // intended integer target fires on that tick, not the next one; it
            // is far below one tick, so genuinely-fractional targets still ceil.
            if (tgt <= now + 1e-3) {
                fired = true;                          // coincident beats merge here
                consumed_.insert(seq);
                it = pending_.erase(it);
            } else {
                ++it;
            }
        }

        if (fired)
            highCountdown_ = std::max(1L, std::lround(0.01 * (double)s.tickRateHz));
        out("output").set(s, highCountdown_ > 0 ? 1.0f : 0.0f);
        if (highCountdown_ > 0) highCountdown_--;

        // bound the consumed set: beats below beatCount_-2 are never rescheduled
        while (!consumed_.empty() && *consumed_.begin() < beatCount_ - 2)
            consumed_.erase(consumed_.begin());
    }

private:
    void schedule(long seq, double anchor) {
        if (consumed_.count(seq)) return;
        pending_[seq] = anchor;   // overwrite: own-edge anchor supersedes prediction
    }
    double targetOf(long seq, double anchor, int L, EngineState& s) {
        int step = (int)(seq % L);
        float sh = in("timing", step + 1).value(s);
        if (sh >  0.9999f) sh =  0.9999f;
        if (sh < -0.9999f) sh = -0.9999f;
        return anchor + (double)sh * P_;
    }

    GateReader clockGate_, resetGate_;
    bool haveFirst_ = false, haveP_ = false;
    double P_ = 0.0;
    uint64_t lastEdge_ = 0;
    long beatCount_ = 0;
    long highCountdown_ = 0;
    std::map<long, double> pending_;   // seq -> anchor tick
    std::set<long> consumed_;          // emitted or permanently dropped
};

DROID_REGISTER_CIRCUIT(timing, Timing)

} // namespace droid
