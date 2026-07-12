// mixer — CV mixer. Spec: manual/circuits/mixer.md. Sums up to eight inputs and
// also emits their maximum, minimum and average. Unpatched inputs are excluded
// from maximum/minimum/average (they are NOT treated as 0). Each input carries
// the standard A*B+C level/offset. All outputs 0 when nothing is patched.
#include "../src/registry.hpp"

namespace droid {

class Mixer : public Circuit {
public:
    void tick(EngineState& s) override {
        float sum = 0.0f, mn = 0.0f, mx = 0.0f;
        int n = 0;
        for (int i = 1; i <= 8; i++) {
            Input& jack = in("input", i);
            if (!jack.connected()) continue;      // exclude unpatched from min/max/avg
            float v = jack.value(s);
            if (n == 0) { mn = mx = v; }
            else { if (v < mn) mn = v; if (v > mx) mx = v; }
            sum += v;
            n++;
        }
        out("output").set(s, sum);
        out("maximum").set(s, n ? mx : 0.0f);
        out("minimum").set(s, n ? mn : 0.0f);
        out("average").set(s, n ? sum / (float)n : 0.0f);
    }
};

DROID_REGISTER_CIRCUIT(mixer, Mixer)

} // namespace droid
