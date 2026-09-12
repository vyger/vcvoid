// motoquencer — Motor fader performance sequencer (M4 skin). The whole sequencer
// engine lives in the shared core engine/src/seqcore.hpp (see it for the full
// implemented-vs-deferred scope and the SPEC-GAP notes); this file provides only
// the M4 editing surface: each lane is an absolute-position motor fader with
// motorized recall on page/mode change and per-notch feel, and the touch plate
// below it edits the buttonmode parameter (gate / skip / gate-pattern). Editing a
// step's pitch to a new notch auto-switches its gate on.
#include "../src/registry.hpp"
#include "../src/seqcore.hpp"

namespace droid {

class Motoquencer : public SeqCore {
public:
    int availableLanes(EngineState& s) override {
        int n = s.controllers.faderCount();
        return n > 0 ? n : 4;
    }

    void editSurface(EngineState& s, int page, int fm, int bm, bool recall,
                     bool faders, bool buttons) override {
        int Nfeel = notchesFor(s, fm);
        if ((int)hold_.size() < numFaders_) hold_.resize(numFaders_);
        for (int i = 0; i < numFaders_; i++) {
            int step = page * numFaders_ + i;
            if (step >= numsteps_) continue;
            int fdr = firstFader_ + i;
            FaderState* f = s.controllers.fader(fdr);
            if (!f) continue;

            // touch plate editing (independent of fadermode). The plate BELOW the
            // fader is the step button — grabbing/moving the fader itself
            // (f->touched) is a different sensor and must never press the step.
            // Both edges are reported: buttonmode 1's two-finger gesture ends on
            // the release of the held anchor plate (seqcore.hpp plateEdge).
            if (buttons) {
                bool pressed = f->plate;
                bool wasPressed = (i < (int)prevTouch_.size()) ? prevTouch_[i] : false;
                if (pressed != wasPressed) plateEdge(bm, i, step, pressed);
                if (i < (int)prevTouch_.size()) prevTouch_[i] = pressed;
            }

            if (!faders) continue;                         // another chain member's faders
            if (recall || !wasSelected_) {                // motorized recall
                float stored = storedPos(s, fm, step);
                fc::source(hold_[i], true, stored, f->position, f->touched);   // arm (#45)
                s.controllers.commandFader(fdr, stored);
                f->notches = Nfeel <= 25 ? Nfeel : 0;
            } else {                                       // read user movement
                float snapped;
                // A recall on a HELD fader stays authoritative until the fader
                // physically moves: the motor is off under a finger, so the
                // unchanged position must not be read back as an edit (#45,
                // fadercore.hpp RecallHold).
                float pos = fc::source(hold_[i], false, storedPos(s, fm, step),
                                       f->position, f->touched);
                bool changed = applyEdit(s, fm, step, pos, snapped);
                s.controllers.commandFader(fdr, snapped);
                f->notches = Nfeel <= 25 ? Nfeel : 0;
                if (changed && fm == 0) { cur_.gate[step] = true; onCvEdited(s, step); }  // auto-on + compose audition
            }
        }
        wasSelected_ = true;
    }

    // One recall hold per LANE (fader), not per step: it guards the physical
    // fader, so a page change re-arms it through the recall branch above.
    std::vector<fc::RecallHold> hold_;

    void setLaneLed(EngineState& s, int lane, float bright, float color) override {
        if (FaderState* f = s.controllers.fader(firstFader_ + lane)) {
            f->led = bright;
            f->ledColor = color;
        }
    }
};

DROID_REGISTER_CIRCUIT(motoquencer, Motoquencer)

} // namespace droid
