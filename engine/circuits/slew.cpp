// slew — Slew limiter with three shapes. Spec: manual/circuits/slew.md.
// Standard A*B+C input math applies. `slew` is the slew time; slewup/slewdown
// scale it per direction; a patched `gate` gates limiting (slew acts as 0 while
// low). slew <= 0 disables limiting (immediate follow).
//
// LINEAR has an exact spec: `slew` seconds are needed for a 1 V (=0.1 engine)
// change, i.e. rate = 0.1 / effectiveSlew engine units per second.
//
// SPEC-GAP: the exponential and S-curve shapes are only "tuned to sound similar"
// with no stated time constant. We implement exponential as a one-pole low-pass
// with tau = effectiveSlew, and S-curve as two cascaded one-poles (S-shaped step
// response). Their exact curves are unverified vs hardware; a reference trace
// would pin them. (Also unreconciled: the manual's "120 V/s at pot 0.5" does not
// match the "slew seconds per 1 V" definition used here.)
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>

namespace droid {

class Slew : public Circuit {
public:
    void tick(EngineState& s) override {
        float input = in("input").value(s);
        float slew  = in("slew").value(s);
        if (slew < 0.0f) slew = 0.0f;                 // negative treated as 0
        float up    = in("slewup").value(s);
        float down  = in("slewdown").value(s);
        bool  active = !in("gate").connected() ||
                       in("gate").value(s) >= kGateHighThreshold;
        float dt = s.secondsPerTick();

        // Effective slew time for a move from `from` toward `to`; 0 = no limit.
        auto effSlew = [&](float from, float to) -> float {
            if (!active) return 0.0f;
            float e = slew * (to >= from ? up : down);
            return e < 0.0f ? 0.0f : e;
        };

        // --- linear (exact) ---
        {
            float e = effSlew(linOut_, input);
            if (e <= 0.0f) {
                linOut_ = input;
            } else {
                float maxStep = (0.1f / e) * dt;      // 0.1 engine = 1 V
                float d = input - linOut_;
                if (std::fabs(d) <= maxStep) linOut_ = input;
                else linOut_ += (d > 0.0f ? maxStep : -maxStep);
            }
            out("linear").set(s, linOut_);
        }
        // --- exponential (one-pole, SPEC-GAP shape) ---
        {
            float e = effSlew(expOut_, input);
            if (e <= 0.0f) expOut_ = input;
            else expOut_ += (input - expOut_) * (1.0f - std::exp(-dt / e));
            out("exponential").set(s, expOut_);
        }
        // --- S-curve (two cascaded one-poles, SPEC-GAP shape) ---
        {
            float e = effSlew(scOut_, input);
            if (e <= 0.0f) { scStage_ = input; scOut_ = input; }
            else {
                float a = 1.0f - std::exp(-dt / e);
                scStage_ += (input - scStage_) * a;
                scOut_   += (scStage_ - scOut_) * a;
            }
            out("scurve").set(s, scOut_);
        }
    }

private:
    float linOut_ = 0.0f, expOut_ = 0.0f, scOut_ = 0.0f, scStage_ = 0.0f;
};

DROID_REGISTER_CIRCUIT(slew, Slew)

} // namespace droid
