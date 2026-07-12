// notchedpot — Obsolete center-notch pot helper. Spec:
// manual/circuits/notchedpot.md. Superseded by `pot`; the DROID Forge flags it
// as deprecated (an accepted crosscheck divergence).
//
// A pure function of the physical pot: `output` = the pot value with a center
// notch of size `notch` (default 0.1) applied — a flat zone around 0.5 that
// makes the exact center easy to hit while keeping the endpoints at 0 and 1.
// The bipolar/absbipolar/left|righthalf[inv] outputs are the same hemisphere
// derivations `pot` uses, here always at scale 1.0 (notchedpot has no
// outputscale). All the shared math lives in potshape.hpp.
//
// This circuit is stateless — no virtual pot, presets, select, discrete or
// slope (those are `pot`-only features). notch is clamped to [0, 0.5].
#include "../src/registry.hpp"
#include "../src/potshape.hpp"

namespace droid {

class NotchedPot : public Circuit {
public:
    void tick(EngineState& s) override {
        float v = potshape::applyNotch(in("pot").value(s), in("notch").value(s));
        out("output").set(s, v);
        potshape::Hemispheres h = potshape::hemispheres(v, 1.0f);
        out("bipolar").set(s, h.bipolar);
        out("absbipolar").set(s, h.absbipolar);
        out("lefthalf").set(s, h.lefthalf);
        out("righthalf").set(s, h.righthalf);
        out("lefthalfinv").set(s, h.lefthalfinv);
        out("righthalfinv").set(s, h.righthalfinv);
    }
};

DROID_REGISTER_CIRCUIT(notchedpot, NotchedPot)

} // namespace droid
