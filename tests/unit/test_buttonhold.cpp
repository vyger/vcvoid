#include "harness.hpp"
#include "ButtonHold.hpp"
using vcvoid::HoldArbiter;
using vcvoid::ButtonHold;

// Mod-hold / Latch for the momentary DROID buttons (issue #39). A mouse can
// only hold one momentary widget at a time, which makes every CTRL+button
// chord in a DROID patch unreachable; these two gestures give the pointer the
// extra fingers. The rules that matter:
//   - at most ONE hold at a time, so the SECOND modified click is a plain
//     press and the pair reads as a chord;
//   - the hold ends on POLLED modifier state, never on a key-up event;
//   - latches are independent of the modifier and of each other.
// Which modifier arms it is a Rack constraint decided in HoldWidget.hpp (it
// cannot be Alt — Rack's ScrollWidget eats Alt+click before any module sees
// it); this layer only ever hears "the press carried it" / "it is still down".

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

    h.modHeld = true;
    CHECK(h.pressed());
    CHECK(h.held());
}

TEST(buttonhold_first_modified_click_takes_the_hold) {
    HoldArbiter a;
    CHECK(a.press(kCtrl, true));
    CHECK(a.isHeld(kCtrl));
    CHECK(!a.isHeld(kOther));
    CHECK(a.modDown);          // the press itself proves the key is down
}

TEST(buttonhold_second_modified_click_is_the_chord) {
    HoldArbiter a;
    a.press(kCtrl, true);
    // Still holding the modifier, click the button the patch actually wants:
    // an ordinary momentary press, NOT a second hold.
    CHECK(!a.press(kOther, true));
    CHECK(a.isHeld(kCtrl));
    CHECK(!a.isHeld(kOther));
    // And a third, and a fourth — CTRL stays the only held button.
    CHECK(!a.press(kThird, true));
    CHECK(!a.press(kOther, true));
    CHECK(a.isHeld(kCtrl));
    CHECK(!a.isHeld(kThird));
}

TEST(buttonhold_modclicking_the_held_button_again_is_a_plain_press) {
    HoldArbiter a;
    a.press(kCtrl, true);
    CHECK(!a.press(kCtrl, true));   // does not re-take, does not release
    CHECK(a.isHeld(kCtrl));
}

TEST(buttonhold_plain_click_never_takes_a_hold) {
    HoldArbiter a;
    CHECK(!a.press(kCtrl, false));
    CHECK(!a.isHeld(kCtrl));
    CHECK(!a.owner);
}

TEST(buttonhold_releasing_the_modifier_releases_the_button) {
    HoldArbiter a;
    a.press(kCtrl, true);
    a.pollMod(true);                // still down: survives the frame poll
    CHECK(a.isHeld(kCtrl));
    a.pollMod(false);
    CHECK(!a.isHeld(kCtrl));
    CHECK(!a.modDown);
}

TEST(buttonhold_modified_click_after_release_starts_a_new_single_hold) {
    HoldArbiter a;
    a.press(kCtrl, true);
    a.pollMod(false);               // modifier released, hold dropped
    CHECK(a.press(kOther, true));   // a fresh modified click takes a new hold
    CHECK(a.isHeld(kOther));
    CHECK(!a.isHeld(kCtrl));
    CHECK(!a.press(kCtrl, true));   // and the single-hold rule still applies
}

TEST(buttonhold_release_all_clears_the_mod_hold) {
    HoldArbiter a;
    a.press(kCtrl, true);
    a.releaseAll();
    CHECK(!a.isHeld(kCtrl));
    // The key is still physically down, but nothing is held until the next click.
    a.pollMod(true);
    CHECK(!a.owner);
}

TEST(buttonhold_forget_only_clears_the_owner) {
    HoldArbiter a;
    a.press(kCtrl, true);
    a.forget(kOther);               // some other widget going away
    CHECK(a.isHeld(kCtrl));
    a.forget(kCtrl);                // the held one going away
    CHECK(!a.isHeld(kCtrl));
    CHECK(!a.owner);
}

TEST(buttonhold_latch_is_independent_of_the_modifier) {
    // Latches are per-button state, not arbitrated: any number at once, and
    // releasing the modifier leaves them alone.
    HoldArbiter a;
    ButtonHold ctrl, other;
    ctrl.latched = true;
    other.latched = true;
    CHECK(ctrl.pressed() && other.pressed());

    a.press(&ctrl, true);
    ctrl.modHeld = a.isHeld(&ctrl);
    a.pollMod(false);
    ctrl.modHeld = a.isHeld(&ctrl);
    CHECK(!ctrl.modHeld);
    CHECK(ctrl.pressed());          // still latched
    CHECK(other.pressed());
}

TEST(buttonhold_one_arbiter_spans_every_kind_of_control) {
    // The drawn buttons are ParamWidgets and the E4/DB8E encoder pushes are
    // not, but both are B registers and both share ONE arbiter — otherwise
    // "hold CTRL on the p2b8, then click an encoder push on the e4" would
    // quietly become two holds instead of a chord.
    static const char kEncoderPush[] = "E1.1 push";
    HoldArbiter a;
    CHECK(a.press(kCtrl, true));              // a button takes the hold
    CHECK(!a.press(kEncoderPush, true));      // the encoder push chords with it
    CHECK(a.isHeld(kCtrl));
    CHECK(!a.isHeld(kEncoderPush));

    a.pollMod(false);
    CHECK(a.press(kEncoderPush, true));       // and the reverse direction
    CHECK(!a.press(kCtrl, true));
    CHECK(a.isHeld(kEncoderPush));
    CHECK(!a.isHeld(kCtrl));
}
