#pragma once
// engine/src/uihelpers.hpp — helpers shared by the UI-circuit family
// (button, buttongroup, pot, encoder, the bank circuits, display, ...).
// These were byte-identical private copies in 11-13 circuit files; the
// semantics are documented in manual/basics.md §4 (overlaying / presets)
// and repeated in each circuit's Inputs table.

#include "circuit.hpp"
#include "gatereader.hpp"
#include <cmath>

namespace droid {
namespace ui {

// select/selectat overlay gating: with `selectat` patched the circuit is
// active iff round(select) == round(selectat); with only `select` patched,
// gate-high semantics; neither patched = always selected (smart default).
inline bool isSelected(Circuit& c, EngineState& s) {
    if (c.in("selectat").connected()) {
        long sel = std::lround(c.in("select").value(s));
        long at  = std::lround(c.in("selectat").value(s));
        return sel == at;
    }
    if (c.in("select").connected())
        return c.in("select").value(s) >= kGateHighThreshold;
    return true;   // smart default: always selected
}

// Clamp a preset index to 0..maxIndex (15 for the 16-slot scalar UI
// circuits; kPresets-1 for the bank circuits).
inline int clampPreset(long n, int maxIndex) {
    return n < 0 ? 0 : (n > maxIndex ? maxIndex : (int)n);
}

// Preset number: from `preset` when patched, else the trigger's own value
// (value-carrying trigger, e.g. 0.3 -> preset 0).
inline int presetNum(Circuit& c, EngineState& s, float trigValue,
                     bool presetPatched, int maxIndex) {
    float v = presetPatched ? c.in("preset").value(s) : trigValue;
    return clampPreset(std::lround(v), maxIndex);
}

} // namespace ui
} // namespace droid
