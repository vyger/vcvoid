// random — Random number generator. Spec: manual/circuits/random.md. Produces a
// random value in [minimum, maximum]. With `clock` patched it samples & holds a
// new value on each rising edge; unclocked it draws every tick. `steps` quantizes
// to N evenly-spaced levels (0 = continuous). Draws only through the engine RNG.
//
// SPEC-GAP: in clocked mode the pre-first-clock output is 0 (the manual does not
// say what is held before the first clock).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include "../src/rng.hpp"
#include <cmath>

namespace droid {

class Random : public Circuit {
public:
    void tick(EngineState& s) override {
        bool rising = clockGate_.risingEdge(in("clock").value(s));  // track each tick
        bool clocked = in("clock").connected();
        bool draw = clocked ? rising : true;   // clocked: on edge; else every tick
        if (draw) value_ = compute(s);
        out("output").set(s, value_);
    }

private:
    float compute(EngineState& s) {
        float mn = in("minimum").value(s);
        float mx = in("maximum").value(s);
        long steps = std::lround(in("steps").value(s));
        if (steps == 1) return (mn + mx) * 0.5f;      // single level: the average
        if (steps >= 2) {
            float u = randUniform(s.rngState);
            long i = (long)(u * (float)steps);        // in [0, steps-1]
            if (i >= steps) i = steps - 1;
            return mn + (float)i / (float)(steps - 1) * (mx - mn);
        }
        // steps <= 0: continuous
        return mn + randUniform(s.rngState) * (mx - mn);
    }

    GateReader clockGate_;
    float value_ = 0.0f;   // 0 until first draw
};

DROID_REGISTER_CIRCUIT(random, Random)

} // namespace droid
