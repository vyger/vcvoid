// explin — Exponential to linear converter. Spec: manual/circuits/explin.md.
// Maps an exponential decay (from startvalue toward endvalue) to a linear ramp:
//   wet = startvalue * ln(input/endvalue) / ln(startvalue/endvalue)
// which equals startvalue at input=startvalue and 0 at input=endvalue. `mix`
// blends dry (input) and wet: output = input + mix*(wet-input). Standard A*B+C
// input math applies.
#include "../src/registry.hpp"
#include <cmath>

namespace droid {

class Explin : public Circuit {
public:
    void tick(EngineState& s) override {
        float x = in("input").value(s);
        if (x < 0.0f) x = 0.0f;                 // input must be positive, else 0
        float startv = in("startvalue").value(s);
        float endv   = in("endvalue").value(s);
        if (endv < 0.001f) endv = 0.001f;       // forced >= 0.001
        float mix    = in("mix").value(s);

        float wet;
        float denom = std::log(startv / endv);
        if (x <= endv || startv <= 0.0f || denom == 0.0f || !std::isfinite(denom))
            wet = 0.0f;                          // at/below endvalue -> 0
        else
            wet = startv * std::log(x / endv) / denom;

        out("output").set(s, x + mix * (wet - x));
    }
};

DROID_REGISTER_CIRCUIT(explin, Explin)

} // namespace droid
