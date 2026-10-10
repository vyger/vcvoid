// motoquencer2 — EXPERIMENTAL (voidbot only, #85): motoquencer plus one input,
// `probabilitymode`, which narrows the notches the gate-probability fader lane
// (fadermode 2) offers. Spec: manual/circuits/experimental/motoquencer2.md.
//
// Everything else is motoquencer, inherited verbatim from engine/src/motoquencer.hpp
// (the M4 editing surface) and engine/src/seqcore.hpp (the sequencer). The whole
// feature is the one override below: SeqCore asks its subclass for the mode and
// keeps the eight-notch behaviour when it is 0, so [motoquencer] and [encoquencer]
// are bit-for-bit what they were.
//
// The stored value stays the same 0..7 `gateprob` index in every mode — presets,
// saved state, the LED codes and the playing rule are untouched — so narrowing the
// lane never rewrites a step: a value the active mode cannot reach keeps playing
// and its fader simply shows the nearest reachable notch (seqcore.hpp probTable /
// probNotchOf).
#include "../src/motoquencer.hpp"
#include <cmath>

namespace droid {

class Motoquencer2 : public Motoquencer {
public:
    // Read like fadermode — a plain integer off this circuit's own input — and
    // then resolved chain-wide by SeqCore::tick, which only calls this on the
    // chain main, so a linked member's own `probabilitymode` is ignored. Out-of-
    // range values clamp there too (0..2).
    int probabilityModeRaw(EngineState& s) override {
        return (int)std::lround(in("probabilitymode").value(s));
    }
};

DROID_REGISTER_CIRCUIT(motoquencer2, Motoquencer2)

} // namespace droid
