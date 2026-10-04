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

// Change baseline for circuit-tier display writes. Holds the last value that
// actually LANDED on the screen, and swallows the first tick's value so a patch
// that is merely loaded (or has its state restored) never activates the display
// on its own — on hardware the screen only wakes when you operate something.
//
// On a REJECTED write the caller must NOT call accept(), so the change stays
// pending and re-attempts every tick until the current owner's linger expires —
// delay-not-discard, exactly as the [display] circuit does with its own
// baseline.
struct DisplayBaseline {
    float sent = 0.0f;
    bool  seeded = false;
    bool changed(float value) {
        if (!seeded) { seeded = true; sent = value; return false; }
        return std::fabs(value - sent) > 1e-6f;
    }
    void accept(float value) { sent = value; }
};

// Per-ELEMENT change detection for the bank circuits (issue #22). encoderbank,
// faderbank and fadermatrix each hold 8/16/16 elements but share ONE screen,
// and the manual says the screen follows the element you touched —
// encoderbank.md: "updates the display whenever you turn one of the encoders.
// It then shows the updated value of `outputX` of that encoder".
//
// So every element gets its own DisplayBaseline (same first-tick-swallow and
// delay-not-discard contract as the scalar case) and this picks which one gets
// the screen when several moved on the same tick: the LOWEST element index,
// with the rest left pending so they land on following ticks. A hand moves one
// fader at a time, so a tie only arises from a preset recall or a scripted
// input; the manual says nothing about it, and leaving the losers pending is
// the rule already used for a rejected write rather than a new one.
//
// changed() runs for EVERY element, not just the winner, so the first tick
// seeds them all and a patch that is merely loaded leaves the screen dark.
// `values[0..count-1]` are the elements' current values; `base` must have at
// least `count` entries. Returns the element to show, or -1 for none.
inline int firstChangedElement(DisplayBaseline* base, const float* values, int count) {
    int first = -1;
    for (int i = 0; i < count; i++)
        if (base[i].changed(values[i]) && first < 0) first = i;
    return first;
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
// interaction for that circuit) and owns the "last displayed value" baseline
// (DisplayBaseline above, or a pending flag for the trigger-driven circuits) —
// on a rejected write the caller must leave its baseline untouched so the write
// keeps re-attempting until it lands, exactly as [display] does.
// Returns true iff the write was accepted.
//
// `autoHeaderText` is the title to use when `header` is NOT patched: the
// circuit's own derived one for a plain-value circuit, or the moved element's
// for a bank (issue #22) — see showCircuitValue just below, which supplies the
// scalar case.
//
// THIS function is everything a circuit-tier write shares no matter which
// LAYOUT it is sending (issue #22, Group C): target resolution, arbitration,
// the header, and the owner/tier/tick stamp. The per-layout writers below only
// add their own payload. Returns the claimed DisplayState, or nullptr when the
// write was suppressed (`display = 0`, no such DB8E) or refused — in which case
// the caller must leave its baseline untouched so the write re-attempts.
inline DisplayState* claimCircuitScreen(Circuit& c, EngineState& s,
                                        int autoHeaderText) {
    DisplayState* d = targetDisplay(c, s);
    if (!d) return nullptr;
    bool accepted = (d->owner == &c) ||
                    (s.tick >= d->lingerUntilTick) ||
                    (d->active && d->lastWriteTick == s.tick &&
                     kTierCircuit >= d->ownerTier);
    if (!accepted) return nullptr;
    d->active = true;
    // An explicit `header` wins; otherwise the title the Engine derived from the
    // `output` target at load (0 = none).
    d->headerText = c.in("header").connected() ? floorText(c.in("header").value(s))
                                               : autoHeaderText;
    d->owner = &c;
    d->ownerTier = kTierCircuit;
    d->lingerUntilTick = s.tick;   // no hold of its own
    d->lastWriteTick = s.tick;
    return d;
}

inline bool showValueWithHeader(Circuit& c, EngineState& s, float value,
                                int autoHeaderText, uint8_t numbermode = 0) {
    DisplayState* d = claimCircuitScreen(c, s, autoHeaderText);
    if (!d) return false;
    d->layout = DisplayLayout::Value;
    d->value = value;
    // These circuits have no numbermode/fontsize jacks. The default 0 leaves the
    // DB8E's own user-selected format alone ("use the buttons on the DB8E"); a
    // caller passes a mode only where the manual pins one (nudge's integer
    // display, display.md's numbermode table).
    d->numbermode = numbermode;
    d->fontsize = 0;
    return true;
}

// The plain-value circuits' form: the title is the one the Engine derived from
// this circuit's scalar `output` target.
inline bool showCircuitValue(Circuit& c, EngineState& s, float value,
                             uint8_t numbermode = 0) {
    return showValueWithHeader(c, s, value, c.autoHeaderText, numbermode);
}

// Text layout (issue #22, Group C): one interned text as the body, under the
// usual header. `textNumber` is normally a CIRCUIT-provided string interned at
// load through Circuit::internTexts — recorder's Recording/Playback/Bypass —
// since the point of this variant is words that are not in the patch.
inline bool showCircuitText(Circuit& c, EngineState& s, int textNumber) {
    DisplayState* d = claimCircuitScreen(c, s, c.autoHeaderText);
    if (!d) return false;
    d->layout = DisplayLayout::Text;
    d->bodyText = textNumber;
    return true;
}

// Bubbles layout (issue #22, Group C): the `button` circuit's state chain —
// `count` bubbles joined by short horizontal segments with the one at `index`
// filled solid (measured on hardware, issue #19), under the ordinary derived
// header. The payload is the two numbers; circles and spacing are the screen's
// business. `index` is clamped into the chain so a payload can never point past
// its own bubbles.
inline bool showStateBubbles(Circuit& c, EngineState& s, int count, int index) {
    if (count < 1) return false;
    if (count > 255) count = 255;
    if (index < 0) index = 0;
    if (index > count - 1) index = count - 1;
    DisplayState* d = claimCircuitScreen(c, s, c.autoHeaderText);
    if (!d) return false;
    d->layout = DisplayLayout::Bubbles;
    d->bubbles.count = (uint8_t)count;
    d->bubbles.index = (uint8_t)index;
    return true;
}

// NoteName layout (issue #22, Group C): the `notebuttons` selection, as a note
// number counted in semitones from C, under the ordinary derived header. The
// payload stays a number; "C#" vs "Db" and the glyphs are the screen's choice.
inline bool showNoteName(Circuit& c, EngineState& s, int semitone,
                         bool withOctave) {
    DisplayState* d = claimCircuitScreen(c, s, c.autoHeaderText);
    if (!d) return false;
    d->layout = DisplayLayout::NoteName;
    d->note.semitone = (int16_t)semitone;
    d->note.withOctave = withOctave;
    return true;
}

} // namespace ui
} // namespace droid
