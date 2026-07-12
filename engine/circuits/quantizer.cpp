// quantizer — Non-musical step quantizer. Spec: manual/circuits/quantizer.md.
// Quantizes the input to `steps` steps per 1 V (=0.1 engine units), with a
// direction (down/nearest/up), optional hysteresis, bypass, and a triggered
// sample-and-hold mode. Emits a 10 ms `changed` trigger when the output moves to
// a new value. Standard A*B+C input math applies.
//
// Note: `steps` is declared integer but the manual explicitly allows fractional
// values (e.g. 1.2 = 12 steps per 10 V), so it is read as a float. steps <= 0
// means "infinite steps" -> quantization disabled.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>

namespace droid {

class Quantizer : public Circuit {
public:
    void tick(EngineState& s) override {
        float input = in("input").value(s);
        bool trigConnected = in("trigger").connected();
        bool trigFired = trigConnected && trigGate_.risingEdge(in("trigger").value(s));

        if (in("bypass").value(s) >= kGateHighThreshold) {
            out("output").set(s, input);       // bypass: copy input through
            out("changed").set(s, 0.0f);        // no changed trigger in bypass
            return;
        }

        bool doQuant;
        if (trigConnected) {
            doQuant = trigFired;                // triggered mode: sample on edge
        } else {
            float hist = in("histeresis").value(s);
            doQuant = !hasPrev_ || std::fabs(input - prevInput_) > hist;
        }

        if (doQuant) {
            float q = quantize(input, in("steps").value(s),
                               (int)std::lround(in("direction").value(s)));
            if (hasPrev_ && q != prevOutput_) {
                long dur = std::lround(0.01 * (double)s.tickRateHz);  // 10 ms
                if (dur < 1) dur = 1;
                changedUntil_ = s.tick + (uint64_t)dur;
            }
            prevOutput_ = q;
            prevInput_ = input;
            hasPrev_ = true;
        }

        out("output").set(s, hasPrev_ ? prevOutput_ : 0.0f);
        out("changed").set(s, (hasPrev_ && s.tick < changedUntil_) ? 1.0f : 0.0f);
    }

private:
    static float quantize(float x, float steps, int direction) {
        if (steps <= 0.0f) return x;            // infinite steps -> no quantization
        float stepSize = 0.1f / steps;          // steps per 1 V (0.1 engine units)
        double n = (double)x / (double)stepSize;
        double k;
        if (direction == 0)      k = std::floor(n);   // quantize down
        else if (direction == 2) k = std::ceil(n);    // quantize up
        else                     k = std::round(n);   // nearest
        return (float)(k * (double)stepSize);
    }

    GateReader trigGate_;
    bool  hasPrev_ = false;
    float prevInput_ = 0.0f;
    float prevOutput_ = 0.0f;
    uint64_t changedUntil_ = 0;
};

DROID_REGISTER_CIRCUIT(quantizer, Quantizer)

} // namespace droid
