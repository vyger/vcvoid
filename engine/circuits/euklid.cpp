// euklid — Euclidean rhythm generator. Spec: manual/circuits/euklid.md.
// Each clock advances one step through a length-step pattern with `beats` beats
// distributed evenly (offset rotates it). On beat steps the clock is passed
// through to `output` (exact voltage + gate length); on non-beat steps to
// `offbeats`. If `outputsignal` is patched, that value is sent for the whole
// active step instead of the gated clock. `reset` restarts the pattern.
//
// SPEC-GAP: the manual's ASCII pattern tables are unreliable (e.g. the
// "length: 13" row renders 14 cells) and the low-res figure can't disambiguate
// the exact rotation of interior beats when length isn't a multiple of beats.
// We use the standard Euclidean/Bresenham test ((step*beats) mod length < beats),
// which reproduces the unambiguous manual examples (evenly-divisible cases and
// the length-4 beats-2 offset example). Goldens assert only rotation-invariant
// facts (step 0 is always a beat, minimum-gap guarantees) plus that example.
// Hardware reference data on a non-dividing case would pin the rotation.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>

namespace droid {

class Euklid : public Circuit {
public:
    void tick(EngineState& s) override {
        long length = std::lround(in("length").value(s));
        if (length < 1) length = 1;          // "must be > 0, else 1 is assumed"
        if (length > 64) length = 64;        // documented maximum
        long beats = std::lround(in("beats").value(s));
        if (beats < 0) beats = 0;
        if (beats > length) beats = length;  // capped to length
        long offset = std::lround(in("offset").value(s));

        // reset first, then advance on the clock, so a same-tick reset+clock
        // lands on step 0.
        if (resetGate_.risingEdge(in("reset").value(s))) stepIndex_ = -1;
        if (clockGate_.risingEdge(in("clock").value(s))) stepIndex_++;

        bool haveStep = stepIndex_ >= 0;
        bool active = false;
        if (haveStep && beats > 0) {
            long pos = stepIndex_ % length;
            long j = ((pos - offset) % length + length) % length;  // undo offset
            active = (j * beats) % length < beats;                 // Euclidean test
        }

        float clock = in("clock").value(s);
        if (in("outputsignal").connected()) {
            // Held for the whole step (not gated by the clock).
            float sig = in("outputsignal").value(s);
            out("output").set(s,   (haveStep && active)  ? sig : 0.0f);
            out("offbeats").set(s, (haveStep && !active) ? sig : 0.0f);
        } else {
            // Pass the clock through (voltage + gate length).
            out("output").set(s,   active                ? clock : 0.0f);
            out("offbeats").set(s, (haveStep && !active) ? clock : 0.0f);
        }
    }

private:
    GateReader clockGate_;
    GateReader resetGate_;
    long stepIndex_ = -1;   // -1 = before the first clock
};

DROID_REGISTER_CIRCUIT(euklid, Euklid)

} // namespace droid
