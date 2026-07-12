// encoquencer — Performance sequencer using E4 encoders. Spec:
// manual/circuits/encoquencer.md, which states it is "an exact replica of the
// motoquencer circuit, but it uses encoders of the E4 instead of the motorfaders
// of an M4" and defers ALL sequencer semantics to motoquencer.md. So it shares the
// entire sequencer engine (engine/src/seqcore.hpp) with motoquencer; only the
// editing surface is re-skinned:
//   * encoder TURNS nudge the current step's value (in the current fadermode) by
//     detents — encoders have no absolute position, motor or haptic feel, and no
//     position readback, so unlike motoquencer there is no `expect F` recall check;
//     edits are observed through the cv/gate outputs.
//   * encoder PUSH buttons edit the buttonmode parameter (gate / skip / gate-
//     pattern), exactly like the M4 touch plates.
//   * moving a step's pitch to a new value auto-switches its gate on.
//
// encoquencer-only inputs (`ledpreview`, `zorder`, `nume4s`) are E4-panel concerns:
//   * `ledpreview` and the 25-LED ring visualization are panel-only (like every
//     other LED in the engine) — not golden-observable. The step LED (the
//     middle-three bottom ring cells: white played step + buttonmode colours,
//     SeqCore::updateLeds) IS modelled, in EncoderState.stepLed/-Color.
//   * `zorder` / `nume4s` only reshape HOW sequence steps map onto the physical
//     encoder grid across multiple E4s. zorder=0 (natural order, identical to the
//     fader layout) is modelled; zorder 1..3 and nume4s reshape the panel layout
//     and are inert here (documented SPEC-GAP — they need the E4 grid geometry and
//     have no effect with a single E4 or on the played cv/gate sequence).
#include "../src/registry.hpp"
#include "../src/seqcore.hpp"

namespace droid {

class Encoquencer : public SeqCore {
public:
    int availableLanes(EngineState& s) override {
        int n = s.controllers.encoderCount();
        return n > 0 ? n : 4;
    }

    void editSurface(EngineState& s, int page, int fm, int bm, bool recall) override {
        (void)recall;   // encoders have no motor -> nothing to recall; state persists
        for (int i = 0; i < numFaders_; i++) {
            int step = page * numFaders_ + i;
            if (step >= numsteps_) continue;
            int enc = firstFader_ + i;
            EncoderState* e = s.controllers.encoder(enc);
            if (!e) continue;

            // push-button editing (buttonmode)
            bool pushed = e->pushed;
            bool wasPushed = (i < (int)prevTouch_.size()) ? prevTouch_[i] : false;
            if (pushed && !wasPushed) pressStep(bm, step);
            if (i < (int)prevTouch_.size()) prevTouch_[i] = pushed;

            // turn editing (relative detents drained by the engine each tick)
            bool changed = adjustByDetents(s, fm, step, e->pendingDetents);
            if (changed && fm == 0) { cur_.gate[step] = true; onCvEdited(s, step); }  // gate auto-on + compose audition
            e->ringDisplay = storedPos(s, fm, step);          // panel-only readout
        }
        wasSelected_ = true;
    }

    void setLaneLed(EngineState& s, int lane, float bright, float color) override {
        if (EncoderState* e = s.controllers.encoder(firstFader_ + lane)) {
            e->stepLed = bright;
            e->stepLedColor = color;
        }
    }
};

DROID_REGISTER_CIRCUIT(encoquencer, Encoquencer)

} // namespace droid
