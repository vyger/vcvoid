// unusedfaders — declare a range of M4 motor faders as unused. Spec:
// manual/circuits/unusedfaders.md. While the circuit is selected the declared
// faders are parked at the bottom (position 0) and their LEDs go dark, so faders
// that are momentarily not driven by any selected circuit don't confusingly keep
// their old position/LED. The circuit has NO output jacks — its only observable
// effect is the parked fader position (`expect F<n>`) and the darkened LED
// (panel-only, like every other M4 LED here).
//
// firstfader (default 1) and numfaders (default 1) are static config, resolved
// once at load. select/selectat gate the parking exactly like the overlay on
// the other M4 circuits; while deselected the faders are left alone (so another
// selected circuit — or the user — can use them). Parking is (re)asserted every
// tick while selected, so the motor holds the fader at the bottom even if the
// user pulls it up (instant motor, per fadercore.hpp / controllerstate.hpp).
#include "../src/registry.hpp"
#include "../src/uihelpers.hpp"
#include <cmath>

namespace droid {

class UnusedFaders : public Circuit {
public:
    void tick(EngineState& s) override {
        if (!inited_) init(s);
        if (!ui::isSelected(*this, s)) return;      // leave the faders alone
        for (int i = 0; i < numFaders_; i++) {
            int gf = firstFader_ + i;
            if (FaderState* f = s.controllers.fader(gf)) {
                s.controllers.commandFader(gf, 0.0f);   // park at the bottom
                f->notches = 0;                          // no haptics
                f->led = 0.0f;                           // darken (panel-only)
                f->ledColor = 0.0f;
            }
        }
    }

private:
    void init(EngineState& s) {
        long ff = std::lround(in("firstfader").value(s));
        firstFader_ = ff < 1 ? 1 : (int)ff;
        long nf = std::lround(in("numfaders").value(s));
        numFaders_ = nf < 1 ? 1 : (int)nf;
        inited_ = true;
    }

    bool inited_ = false;
    int  firstFader_ = 1;
    int  numFaders_ = 1;
};

DROID_REGISTER_CIRCUIT(unusedfaders, UnusedFaders)

} // namespace droid
