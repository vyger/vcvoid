#pragma once

#include <cstddef>

// Holding a DROID button down with a mouse (issue #39).
//
// Pure logic, no Rack dependencies — unit-tested headless
// (tests/unit/test_buttonhold.cpp), consumed by the momentary button widgets
// in DroidWidgets.hpp on the UI thread.
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
//   - ALT-HOLD, the transient one. Alt+click a button and it stays pressed for
//     as long as the Alt key is physically down; release Alt and it releases.
//     At most ONE button is alt-held at a time — the first click while Alt is
//     down takes the hold, and every further click while Alt stays down is an
//     ordinary momentary press. That asymmetry IS the chord: Alt+click CTRL,
//     then (still holding Alt) click the button you actually want. Letting a
//     second Alt+click take a second hold would make the common case — the
//     chord — the thing you cannot express.
//
//   - LATCH, the persistent one. Toggled from a button's right-click menu; a
//     latched button is held until it is unlatched, across as many actions as
//     the user likes. Independent of Alt and of the single-hold rule: any
//     number of buttons may be latched at once.
//
// Neither is serialized into the Rack patch (see DroidWidgets.hpp): a reload
// starts with nothing held, so a patch can never come back with a mystery
// button stuck down.

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
    bool altHeld = false;

    bool pressed() const { return mouseDown || latched || altHeld; }
    bool held() const { return latched || altHeld; }
};

// Rack-wide arbiter for the alt-hold gesture.
//
// Widgets are identified by opaque pointer. The arbiter only ever COMPARES
// them, never dereferences them, so a stale id is harmless — but widgets still
// call forget() when they are destroyed so a recycled address cannot inherit
// a hold.
struct AltHoldArbiter {
    // The one button currently alt-held, or null.
    const void* owner = nullptr;
    // Physical Alt state as of the last poll.
    bool altDown = false;

    // A left-button press landed on `id`. `altMod` is whether the press
    // carried the Alt modifier (and nothing else). Returns true when this
    // press TAKES the alt-hold; false when it is an ordinary momentary press,
    // which covers both a plain click and the second half of a chord.
    bool press(const void* id, bool altMod) {
        if (!altMod)
            return false;
        // A press with Alt down is proof Alt is down, a frame before the next
        // poll would say so; without this an Alt+click could be undone by a
        // stale `altDown` from the previous frame.
        altDown = true;
        if (owner)
            return false;   // a hold is already out: this click is the chord
        owner = id;
        return true;
    }

    // Called once per UI frame with the POLLED physical Alt state — not with a
    // key-up event, which can be delivered to some other widget (or to a menu)
    // and never reach the button, leaving it stuck down forever.
    void pollAlt(bool down) {
        altDown = down;
        if (!down)
            owner = nullptr;
    }

    bool isAltHeld(const void* id) const { return owner && owner == id; }

    // "Release all latches" also drops the alt-hold: the menu item means
    // "nothing is held any more", and a hold the user cannot see the Alt key
    // for is exactly the thing they are trying to clear.
    void releaseAll() { owner = nullptr; }

    // A widget is going away.
    void forget(const void* id) {
        if (owner == id)
            owner = nullptr;
    }
};

} // namespace vcvoid
