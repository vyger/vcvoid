#pragma once
#include <cstdint>
#include <string>

namespace droid {

// Per-model controller element table, hand-written from manual/hardware.md
// §6.4–6.12 (firmware blue-7). Each count is the highest valid element number
// (1-based) for that register kind on the model; 0 means the model has no
// register of that kind. `faders` counts motor faders (M4) — these are NOT
// registers (addressed by global fader number, see motorfader.md) but are
// tracked here for completeness. `permissive` marks a model whose register
// semantics hardware.md leaves unclear: for such a model the loader validates
// only the controller index (no element-range check). No model is currently
// permissive — hardware.md is unambiguous for all nine.
struct ControllerModel {
    const char* name;
    uint8_t pots;      // P registers
    uint8_t buttons;   // B registers
    uint8_t leds;      // L registers
    uint8_t switches;  // S registers
    uint8_t encoders;  // E registers
    uint8_t faders;    // motor faders (not a register)
    bool permissive;
};

// Returns the descriptor for a controller model name (e.g. "p2b8"), or nullptr
// if the name is not a known controller. x7 is intentionally absent: it carries
// no P/B/L/S/E controls and never occupies a controller-numbering position.
const ControllerModel* findControllerModel(const std::string& name);

// True if element `num` (1-based) of register kind `t` (one of P,B,L,S,E) exists
// on model `m`. Unknown kinds return false. Permissive models return true for
// any non-zero num of any kind.
bool controllerHasElement(const ControllerModel& m, char t, unsigned num);

} // namespace droid
