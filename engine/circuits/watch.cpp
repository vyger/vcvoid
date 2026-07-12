// watch — Watch how a signal evolves. Spec: manual/circuits/watch.md.
//
// Four independent detectors, all usable at once, watching a single `input`:
//   1. Change detection (with hysteresis): trigger `changed`/`changedup`/
//      `changeddown` when the input differs from the LAST-SAMPLED value by more
//      than `histeresis`; the new value then becomes the sample. Catches slow
//      slopes as well as jumps.
//   2. Edge detection: trigger `edge`/`edgeup`/`edgedown` when the input jumps by
//      at least `edgesize` in ONE loop cycle (here one engine tick) — compared to
//      the previous tick's raw input, so a slow slope produces no edge.
//   3. Step / strumming: the CV range is sliced into compartments of `stepsize`;
//      `step`/`stepup`/`stepdown` trigger when the input crosses into a different
//      compartment (index = floor(input/stepsize)).
//   4. Slope analysis: the input is run through an exponential slew filter (time
//      constant = `response` seconds; 0 disables it) and `slope` outputs its rate
//      of change per second (or per clock tick when `taptempo` is patched).
//      `moving`/`movingup`/`movingdown` are LEVEL gates, high while |slope|
//      exceeds `threshold`.
//
// Trigger outputs are 10 ms (>= 1 tick), matching the engine's other trigger
// emitters (gatetool). Detection compares against the previous engine tick, so
// the "internal loop cycle" the manual references maps to one engine tick — exact
// edge timing is therefore emulation-rate dependent (per the manual's own
// verification_note).
//
// Conventions / SPEC-GAPs (manual silent on the exact comparators / filter):
//   * change uses strict `>` histeresis ("must exceed"); edge and slope-threshold
//     use `>=` / strict-outside per the manual's "at least" / "out of the range"
//     wording.
//   * `response` maps directly to the slew time constant in seconds, and the
//     exponential SHAPE mirrors slew's exponential output (unverified vs
//     hardware, same SPEC-GAP as slew.cpp). response=1.0 is "very slow", not
//     literally infinite.
//   * taptempo clock period is learned from consecutive rising edges; before two
//     edges are seen the per-tick slope falls back to a 0.5 s reference period.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>
#include <cstdint>

namespace droid {

class Watch : public Circuit {
    // trigger output indices
    enum { CHANGED, CHANGEDUP, CHANGEDDOWN, EDGE, EDGEUP, EDGEDOWN,
           STEP, STEPUP, STEPDOWN, NTRIG };

public:
    void tick(EngineState& s) override {
        float input = in("input").value(s);
        float dt = s.secondsPerTick();

        // --- taptempo clock period (consecutive rising edges) --------------
        bool ttConn = in("taptempo").connected();
        if (ttConn && tapGate_.risingEdge(in("taptempo").value(s))) {
            if (haveTap_) tapPeriodTicks_ = double(s.tick - lastTap_);
            lastTap_ = s.tick; haveTap_ = true;
        }

        if (!inited_) {                    // first tick: take the baseline sample
            slewed_       = input;
            changeSample_ = input;
            prevInput_    = input;
            prevStep_     = stepIndex(input, in("stepsize").value(s));
            inited_       = true;
        }

        // --- 4. slope analysis (exponential slew filter) -------------------
        float response = in("response").value(s);
        if (response < 0.0f) response = 0.0f;
        float newSlewed;
        if (response <= 0.0f) newSlewed = input;
        else newSlewed = slewed_ + (input - slewed_) * (1.0f - std::exp(-dt / response));
        float slopePerSec = (newSlewed - slewed_) / dt;
        slewed_ = newSlewed;

        double clockPeriodSec = (tapPeriodTicks_ > 0.0)
                                    ? tapPeriodTicks_ / (double)s.tickRateHz : 0.5;
        float slope = ttConn ? (float)(slopePerSec * clockPeriodSec) : slopePerSec;

        float threshold = in("threshold").value(s);
        if (threshold < 0.0f) threshold = 0.0f;
        bool movingUp   = slope > threshold;
        bool movingDown = slope < -threshold;

        // --- 1. change detection (hysteresis, vs persistent sample) --------
        float histeresis = in("histeresis").value(s);
        if (histeresis < 0.0f) histeresis = 0.0f;
        if (std::fabs(input - changeSample_) > histeresis) {
            bool up = input > changeSample_;
            fire(CHANGED, s);
            fire(up ? CHANGEDUP : CHANGEDDOWN, s);
            changeSample_ = input;
        }

        // --- 2. edge detection (vs previous tick's raw input) --------------
        float edgesize = in("edgesize").value(s);
        if (edgesize < 0.0f) edgesize = 0.0f;
        float d = input - prevInput_;
        if (d != 0.0f && std::fabs(d) >= edgesize) {
            fire(EDGE, s);
            fire(d > 0.0f ? EDGEUP : EDGEDOWN, s);
        }
        prevInput_ = input;

        // --- 3. step / strumming (compartment crossings) -------------------
        float stepsize = in("stepsize").value(s);
        if (stepsize > 0.0f) {
            long comp = stepIndex(input, stepsize);
            if (comp != prevStep_) {
                fire(STEP, s);
                fire(comp > prevStep_ ? STEPUP : STEPDOWN, s);
                prevStep_ = comp;
            }
        }

        // --- drive outputs -------------------------------------------------
        out("slope").set(s, slope);
        out("moving").set(s, (movingUp || movingDown) ? 1.0f : 0.0f);
        out("movingup").set(s, movingUp ? 1.0f : 0.0f);
        out("movingdown").set(s, movingDown ? 1.0f : 0.0f);

        static const char* kNames[NTRIG] = {
            "changed", "changedup", "changeddown", "edge", "edgeup", "edgedown",
            "step", "stepup", "stepdown"};
        for (int i = 0; i < NTRIG; i++)
            out(kNames[i]).set(s, s.tick < trigOff_[i] ? 1.0f : 0.0f);
    }

private:
    static long stepIndex(float v, float size) {
        if (size <= 0.0f) return 0;
        return (long)std::floor(v / size);
    }
    void fire(int idx, EngineState& s) {
        long len = std::max(1L, std::lround(0.01 * (double)s.tickRateHz));
        trigOff_[idx] = s.tick + (uint64_t)len;
    }

    GateReader tapGate_;
    bool inited_ = false, haveTap_ = false;
    float slewed_ = 0.0f, changeSample_ = 0.0f, prevInput_ = 0.0f;
    long  prevStep_ = 0;
    uint64_t lastTap_ = 0, trigOff_[NTRIG] = {0};
    double tapPeriodTicks_ = 0.0;
};

DROID_REGISTER_CIRCUIT(watch, Watch)

} // namespace droid
