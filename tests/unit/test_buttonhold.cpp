#include "harness.hpp"
#include "ButtonHold.hpp"
using vcvoid::AltHoldArbiter;
using vcvoid::ButtonHold;

// Alt-hold / Latch for the momentary DROID buttons (issue #39). A mouse can
// only hold one momentary widget at a time, which makes every CTRL+button
// chord in a DROID patch unreachable; these two gestures give the pointer the
// extra fingers. The rules that matter:
//   - at most ONE alt-hold at a time, so the SECOND click under Alt is a plain
//     press and the pair reads as a chord;
//   - the hold ends on POLLED Alt state, never on a key-up event;
//   - latches are independent of Alt and of each other.

// Stand-ins for widget addresses; the arbiter only ever compares them.
static const char kCtrl[] = "B1.8";
static const char kOther[] = "B1.1";
static const char kThird[] = "B1.2";

TEST(buttonhold_pressed_is_any_of_the_three) {
    ButtonHold h;
    CHECK(!h.pressed());
    CHECK(!h.held());

    h.mouseDown = true;
    CHECK(h.pressed());
    CHECK(!h.held());          // a finger on it is not "sticky" — no ring
    h.mouseDown = false;

    h.latched = true;
    CHECK(h.pressed());
    CHECK(h.held());
    h.latched = false;

    h.altHeld = true;
    CHECK(h.pressed());
    CHECK(h.held());
}

TEST(buttonhold_first_alt_click_takes_the_hold) {
    AltHoldArbiter a;
    CHECK(a.press(kCtrl, true));
    CHECK(a.isAltHeld(kCtrl));
    CHECK(!a.isAltHeld(kOther));
    CHECK(a.altDown);          // the press itself proves Alt is down
}

TEST(buttonhold_second_click_under_alt_is_the_chord) {
    AltHoldArbiter a;
    a.press(kCtrl, true);
    // Still holding Alt, click the button the patch actually wants: an
    // ordinary momentary press, NOT a second hold.
    CHECK(!a.press(kOther, true));
    CHECK(a.isAltHeld(kCtrl));
    CHECK(!a.isAltHeld(kOther));
    // And a third, and a fourth — CTRL stays the only held button.
    CHECK(!a.press(kThird, true));
    CHECK(!a.press(kOther, true));
    CHECK(a.isAltHeld(kCtrl));
    CHECK(!a.isAltHeld(kThird));
}

TEST(buttonhold_alt_clicking_the_held_button_again_is_a_plain_press) {
    AltHoldArbiter a;
    a.press(kCtrl, true);
    CHECK(!a.press(kCtrl, true));   // does not re-take, does not release
    CHECK(a.isAltHeld(kCtrl));
}

TEST(buttonhold_plain_click_never_takes_a_hold) {
    AltHoldArbiter a;
    CHECK(!a.press(kCtrl, false));
    CHECK(!a.isAltHeld(kCtrl));
    CHECK(!a.owner);
}

TEST(buttonhold_releasing_alt_releases_the_button) {
    AltHoldArbiter a;
    a.press(kCtrl, true);
    a.pollAlt(true);                // still down: survives the frame poll
    CHECK(a.isAltHeld(kCtrl));
    a.pollAlt(false);
    CHECK(!a.isAltHeld(kCtrl));
    CHECK(!a.altDown);
}

TEST(buttonhold_alt_click_after_release_starts_a_new_single_hold) {
    AltHoldArbiter a;
    a.press(kCtrl, true);
    a.pollAlt(false);               // Alt released, hold dropped
    CHECK(a.press(kOther, true));   // a fresh Alt+click takes a new hold
    CHECK(a.isAltHeld(kOther));
    CHECK(!a.isAltHeld(kCtrl));
    CHECK(!a.press(kCtrl, true));   // and the single-hold rule still applies
}

TEST(buttonhold_release_all_clears_the_alt_hold) {
    AltHoldArbiter a;
    a.press(kCtrl, true);
    a.releaseAll();
    CHECK(!a.isAltHeld(kCtrl));
    // Alt is still physically down, but nothing is held until the next click.
    a.pollAlt(true);
    CHECK(!a.owner);
}

TEST(buttonhold_forget_only_clears_the_owner) {
    AltHoldArbiter a;
    a.press(kCtrl, true);
    a.forget(kOther);               // some other widget going away
    CHECK(a.isAltHeld(kCtrl));
    a.forget(kCtrl);                // the held one going away
    CHECK(!a.isAltHeld(kCtrl));
    CHECK(!a.owner);
}

TEST(buttonhold_latch_is_independent_of_alt) {
    // Latches are per-button state, not arbitrated: any number at once, and
    // releasing Alt leaves them alone.
    AltHoldArbiter a;
    ButtonHold ctrl, other;
    ctrl.latched = true;
    other.latched = true;
    CHECK(ctrl.pressed() && other.pressed());

    a.press(&ctrl, true);
    ctrl.altHeld = a.isAltHeld(&ctrl);
    a.pollAlt(false);
    ctrl.altHeld = a.isAltHeld(&ctrl);
    CHECK(!ctrl.altHeld);
    CHECK(ctrl.pressed());          // still latched
    CHECK(other.pressed());
}
