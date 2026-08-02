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

// --- DB8E screen helpers ---------------------------------------------------
// Largest DB8E target worth resolving; anything beyond is inert (no such DB8E).
// Well within int and float-exact, so the cast in floorClamp never overflows.
inline constexpr int kDisplayTargetMax = 256;
// Text numbers are 1-based; at/above this is "no such text" (textForNumber
// returns ""). Float-exact and int-safe so a huge input-math product can't make
// the cast UB.
inline constexpr int kTextMax = 1 << 20;

// Range-check-before-cast: a finite-but-out-of-range float cast to a narrow int
// is UB, and input math (`= I1 * 1e20`) can produce one. Compare AS FLOAT, clamp
// into [lo, hi], and only ever cast an already-in-range value. Non-finite -> lo.
inline int floorClamp(float v, int lo, int hi) {
    if (!std::isfinite(v) || v <= float(lo)) return lo;
    if (v >= float(hi)) return hi;
    return (int)std::floor(v);
}

// Text numbers are 1-based positive integers (0 = empty). <=0 (and non-finite)
// map to 0 (empty); a large number clamps to kTextMax, which resolves to "" —
// textForNumber's contract, without a UB cast.
inline int floorText(float v) { return floorClamp(v, 0, kTextMax); }

// Resolve a circuit's `display` jack to a DB8E screen: 0 => suppressed (never
// writes), an out-of-range/huge/non-finite target => no such DB8E => inert
// (not a load error, mirroring G8's absent-hardware tolerance). Default 1.
inline DisplayState* targetDisplay(Circuit& c, EngineState& s) {
    int n = floorClamp(c.in("display").value(s), 0, kDisplayTargetMax);
    if (n == 0) return nullptr;
    return s.controllers.display(n);
}

// Circuit-tier screen write (hardware.md §6.12 "Circuits with user interaction":
// "When you operate a control that changes a circuit's state, you rather want to
// see that state and not the raw value of the control"). Used by the circuits the
// manual lists as displaying themselves — encoder, pot, motorfader, ... — to push
// their own user-facing value plus their `header` onto the DB8E named by their
// `display` jack.
//
// Arbitration mirrors the [display] circuit's (owner / linger / same-tick, see
// display.cpp) with the two differences the precedence list implies:
//   * the write carries kTierCircuit, so a [display] circuit writing on the SAME
//     tick keeps the screen regardless of patch order;
//   * it installs no linger of its own. Precedence is an order, not a hold: an
//     encoder turned later must be able to take the screen back once the higher
//     tier's linger has expired.
// The caller decides WHEN there is something to show (i.e. what counts as user
// interaction for that circuit) and owns the "last displayed value" baseline —
// on a rejected write the caller must leave its baseline untouched so the write
// keeps re-attempting until it lands, exactly as [display] does.
// Returns true iff the write was accepted.
inline bool showCircuitValue(Circuit& c, EngineState& s, float value) {
    DisplayState* d = targetDisplay(c, s);
    if (!d) return false;
    bool accepted = (d->owner == &c) ||
                    (s.tick >= d->lingerUntilTick) ||
                    (d->active && d->lastWriteTick == s.tick &&
                     kTierCircuit >= d->ownerTier);
    if (!accepted) return false;
    d->active = true;
    d->headerText = floorText(c.in("header").value(s));
    d->isText = false;
    d->value = value;
    // No numbermode/fontsize jacks on these circuits: leave the DB8E's own
    // user-selected format alone (0 = "use the buttons on the DB8E").
    d->numbermode = 0;
    d->fontsize = 0;
    d->owner = &c;
    d->ownerTier = kTierCircuit;
    d->lingerUntilTick = s.tick;   // no hold of its own
    d->lastWriteTick = s.tick;
    return true;
}

} // namespace ui
} // namespace droid
