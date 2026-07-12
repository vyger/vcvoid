// crossfader — Morph between up to 8 inputs. Spec: manual/circuits/crossfader.md.
// N = the highest patched input index (unpatched inputs read their 0 V default).
// `fade` 0..1 maps to a position p = fade*(N-1); the output is the linear
// interpolation of input[floor(p)] and input[floor(p)+1]. fade beyond 1.0 wraps
// the last input back to the first (period N in p). Standard A*B+C input math.
#include "../src/registry.hpp"
#include <cmath>

namespace droid {

class Crossfader : public Circuit {
public:
    void tick(EngineState& s) override {
        int n = 0;
        for (int i = 1; i <= 8; i++)
            if (in("input", i).connected()) n = i;

        if (n == 0) { out("output").set(s, 0.0f); return; }
        if (n == 1) { out("output").set(s, in("input", 1).value(s)); return; }

        double p = (double)in("fade").value(s) * (n - 1);
        // p modulo n, in [0, n): wraps the last input back to the first.
        double pm = p - std::floor(p / n) * n;
        int seg = (int)std::floor(pm);
        double frac = pm - seg;
        if (seg >= n) { seg = 0; frac = 0.0; }   // guard the pm==n float edge

        float a = in("input", seg + 1).value(s);            // 1-based jacks
        float b = in("input", (seg + 1) % n + 1).value(s);
        out("output").set(s, float(a * (1.0 - frac) + b * frac));
    }
};

DROID_REGISTER_CIRCUIT(crossfader, Crossfader)

} // namespace droid
