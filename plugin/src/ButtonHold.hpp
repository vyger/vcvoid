#pragma once

#include <cstddef>

// Holding a DROID button down with a mouse (issue #39).
//
// Pure logic, no Rack dependencies — unit-tested headless
// (tests/unit/test_buttonhold.cpp), consumed by the momentary button widgets
// in DroidWidgets.hpp and the encoder pushes in EncoderWidgets.hpp on the UI
// thread.
//
// Why: DROID patches are built around button CHORDS — hold a "CTRL"/shift
// button, press a second one. MFPS puts mute, presets, clear, reset-all,
// lucky, paste and ARP autostop behind CTRL (`B1.8`) + another button. A mouse
// can only hold one momentary widget at a time, so on the emulated panels
// every one of those chords is unreachable. Nothing about this is an engine
// problem: the `B` register is still just 0/1 and the patch is byte-identical
// to the one that runs on hardware. It is the pointer that has one finger.
//
// Two gestures give it more fingers:
//
//   - MOD-HOLD, the transient one. Shift+click a button and it stays pressed
//     for as long as the modifier key is physically down; release the key and
//     it releases. At most ONE button is mod-held at a time — the first click
//     while the key is down takes the hold, and every further click while it
//     stays down is an ordinary momentary press. That asymmetry IS the chord:
//     Shift+click CTRL, then (still holding Shift) Shift+click the button you
//     actually want. Letting a second modified click take a second hold would
//     make the common case — the chord — the thing you cannot express.
//
//   - LATCH, the persistent one. Toggled from a control's right-click menu; a
//     latched button is held until it is unlatched, across as many actions as
//     the user likes. Independent of the modifier and of the single-hold rule:
//     any number of buttons may be latched at once.
//
// WHICH modifier is a Rack constraint, not a preference, and it is named once
// in HoldWidget.hpp (`kHoldMod`) — see the comment there for why it cannot be
// Alt. This header is deliberately modifier-agnostic: it only ever hears
// "the press carried the hold modifier" and "the key is still down".
//
// Neither gesture is serialized into the Rack patch (see DroidWidgets.hpp): a
// reload starts with nothing held, so a patch can never come back with a
// mystery button stuck down.

namespace vcvoid {

// One button's hold state, and the decision table for what the engine sees.
//
// `mouseDown` is the plain momentary press Rack already implements; the other
// two are ours. A button is "pressed" if ANY of the three is true, and "held"
// (i.e. sticky — stays down with no mouse on it, and so earns the ring the
// widget draws) if either of ours is.
struct ButtonHold {
    bool mouseDown = false;
    bool latched = false;
    bool modHeld = false;

    bool pressed() const { return mouseDown || latched || modHeld; }
    bool held() const { return latched || modHeld; }
};

// Rack-wide arbiter for the mod-hold gesture.
//
// Widgets are identified by opaque pointer. The arbiter only ever COMPARES
// them, never dereferences them, so a stale id is harmless — but widgets still
// call forget() when they are destroyed so a recycled address cannot inherit
// a hold.
struct HoldArbiter {
    // The one control currently mod-held, or null.
    const void* owner = nullptr;
    // Physical modifier state as of the last poll.
    bool modDown = false;

    // A left-button press landed on `id`. `withMod` is whether the press
    // carried the hold modifier (and nothing else). Returns true when this
    // press TAKES the hold; false when it is an ordinary momentary press,
    // which covers both a plain click and the second half of a chord.
    bool press(const void* id, bool withMod) {
        if (!withMod)
            return false;
        // A press carrying the modifier is proof the key is down, a frame
        // before the next poll would say so; without this the press could be
        // undone by a stale `modDown` from the previous frame.
        modDown = true;
        if (owner)
            return false;   // a hold is already out: this click is the chord
        owner = id;
        return true;
    }

    // Called once per UI frame with the POLLED physical modifier state — not
    // with a key-up event, which can be delivered to some other widget (or to
    // a menu) and never reach the button, leaving it stuck down forever.
    void pollMod(bool down) {
        modDown = down;
        if (!down)
            owner = nullptr;
    }

    bool isHeld(const void* id) const { return owner && owner == id; }

    // "Release all latches" also drops the mod-hold: the menu item means
    // "nothing is held any more", and a hold whose modifier key the user
    // cannot see is exactly the thing they are trying to clear.
    void releaseAll() { owner = nullptr; }

    // A widget is going away.
    void forget(const void* id) {
        if (owner == id)
            owner = nullptr;
    }
};

} // namespace vcvoid
