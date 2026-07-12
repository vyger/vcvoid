// once — Output exactly one trigger `delay` seconds after start. Spec:
// manual/circuits/once.md. Fires a single one-tick pulse on the first tick where
// elapsed time (tick / tickRateHz) reaches `delay`, then never again.
//
// SPEC-GAP: `onlycoldstart` would suppress the trigger on a warm start (patch
// reload) but the headless engine has no cold-vs-warm-start state -- every load
// is treated as a cold start -- so it currently has no effect. Rack/hardware
// reload state would be needed to honor it.
#include "../src/registry.hpp"

namespace droid {

class Once : public Circuit {
public:
    void tick(EngineState& s) override {
        bool fire = false;
        if (!fired_) {
            double elapsed = (double)s.tick / (double)s.tickRateHz;
            if (elapsed >= (double)in("delay").value(s)) {
                fire = true;
                fired_ = true;
            }
        }
        out("trigger").set(s, fire ? 1.0f : 0.0f);
    }

private:
    bool fired_ = false;
};

DROID_REGISTER_CIRCUIT(once, Once)

} // namespace droid
