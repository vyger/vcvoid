// sample — Sample & Hold. Spec: manual/circuits/sample.md. A rising edge at
// `sample` stores the current `input`. `gate` tracks the input while high and
// freezes when it goes low. `timewindow` keeps tracking for that many seconds
// after a sample trigger (to let a settling CV reach its final value) before
// freezing. `bypass` copies input to output while high without disturbing the
// stored value. Standard A*B+C input math applies.
//
// SPEC-GAP: the manual doesn't state whether bypass updates the stored sample;
// we treat it as a non-destructive overlay (gate is the tracking variant).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>

namespace droid {

class Sample : public Circuit {
public:
    void tick(EngineState& s) override {
        float input = in("input").value(s);
        bool bypass = in("bypass").value(s) >= kGateHighThreshold;
        bool gate   = in("gate").value(s)   >= kGateHighThreshold;

        if (sampleGate_.risingEdge(in("sample").value(s))) {
            held_ = input;                       // sample now
            float tw = in("timewindow").value(s);
            windowUntil_ = tw > 0.0f
                ? s.tick + (uint64_t)std::llround((double)tw * (double)s.tickRateHz)
                : 0;
        }
        // gate and the post-trigger time window both track the live input.
        if (gate || s.tick < windowUntil_) held_ = input;

        // bypass overlays the input onto the output without touching held_.
        out("output").set(s, bypass ? input : held_);
    }

private:
    GateReader sampleGate_;
    float held_ = 0.0f;
    uint64_t windowUntil_ = 0;
};

DROID_REGISTER_CIRCUIT(sample, Sample)

} // namespace droid
