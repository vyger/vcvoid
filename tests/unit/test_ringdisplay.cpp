#include "harness.hpp"
#include "src/engine.hpp"
using namespace droid;

// Select-gated E4 ring display (issue #15), distilled from the four-menu
// hardware fixture (patches/e4-menus*.ini): overlaid encoderbanks share the
// same physical encoders and only the SELECTED bank may drive the ring. The
// original bug: every bank wrote ringDisplay each tick, so the last bank in
// patch order owned the ring regardless of selection.

// Two banks overlaid on E1.1, selected by I1 (0 = bank A, 1 = bank B).
// Bank A mirrors the fixture's menu 1 (mode 1, cyan, startvalue 0.25, led
// overlay); bank B mirrors menu 3 (bipolar, violet/blue, startvalue 0.75).
static const char* kOverlayPatch =
    "[e4]\n"
    "[encoderbank]\n"
    "    select = I1\n"
    "    selectat = 0\n"
    "    firstencoder = E1.1\n"
    "    mode = 1\n"
    "    color = 0.2\n"
    "    startvalue = 0.25\n"
    "    led1 = 0.5\n"
    "    output1 = O1\n"
    "[encoderbank]\n"
    "    select = I1\n"
    "    selectat = 1\n"
    "    firstencoder = E1.1\n"
    "    mode = 2\n"
    "    color = 1.1\n"
    "    negativecolor = 1.2\n"
    "    startvalue = 0.75\n"
    "    output1 = O2\n";

TEST(ring_selected_bank_owns_the_ring) {
    Engine e;
    CHECK(e.load(kOverlayPatch).ok);
    e.setValue("I1", 0.0f);
    e.tick();
    RingDisplay rd = e.encoderRingInfo(1);
    CHECK(rd.active);
    CHECK(!rd.bipolar);
    CHECK(rd.fill);                       // ledfill defaults to 1
    CHECK_NEAR(rd.value, 0.25, 1e-4);     // bank A's startvalue, NOT bank B's
    CHECK_NEAR(rd.color, 0.2, 1e-6);
    CHECK_NEAR(rd.overlay, 0.5, 1e-6);    // led1 white overlay, select-gated
    // legacy dot readback is select-gated too
    CHECK_NEAR(e.encoderRing(1), 0.25, 1e-4);
}

TEST(ring_jumps_on_menu_switch) {
    Engine e;
    CHECK(e.load(kOverlayPatch).ok);
    e.setValue("I1", 0.0f);
    e.tick();
    // Switch menus: the very next tick must show bank B's image (the hardware
    // "jump to the right values" the bug report says vcvoid didn't do).
    e.setValue("I1", 1.0f);
    e.tick();
    RingDisplay rd = e.encoderRingInfo(1);
    CHECK(rd.active);
    CHECK(rd.bipolar);                    // mode = 2, zero at top-center
    // startvalue 0.75 in position space -> logical 2*0.75-1 = +0.5
    CHECK_NEAR(rd.value, 0.5, 1e-4);
    CHECK_NEAR(rd.color, 1.1, 1e-6);
    CHECK_NEAR(rd.negColor, 1.2, 1e-6);
    CHECK_NEAR(rd.overlay, 0.0, 1e-6);    // bank B wires no led1
    // ... and back
    e.setValue("I1", 0.0f);
    e.tick();
    rd = e.encoderRingInfo(1);
    CHECK(!rd.bipolar);
    CHECK_NEAR(rd.value, 0.25, 1e-4);
}

TEST(ring_dark_when_nothing_selected) {
    Engine e;
    CHECK(e.load(
        "[e4]\n"
        "[encoderbank]\n"
        "    select = I1\n"
        "    selectat = 1\n"
        "    firstencoder = E1.1\n"
        "    output1 = O1\n").ok);
    e.setValue("I1", 0.0f);   // never selected
    e.tick();
    CHECK(!e.encoderRingInfo(1).active);
    // bounds safety of the new readback
    CHECK(!e.encoderRingInfo(0).active);
    CHECK(!e.encoderRingInfo(99).active);
}

TEST(ring_mode0_blanks_but_overlay_survives) {
    Engine e;
    CHECK(e.load(
        "[e4]\n"
        "[encoderbank]\n"
        "    firstencoder = E1.1\n"
        "    mode = 0\n"
        "    led1 = 1\n"
        "    output1 = O1\n").ok);
    e.tick();
    RingDisplay rd = e.encoderRingInfo(1);
    CHECK(!rd.active);                    // mode 0: "its LEDs are off"
    CHECK_NEAR(rd.overlay, 1.0, 1e-6);    // explicit led overlay still honored
}

TEST(ring_discrete_and_default_color) {
    Engine e;
    CHECK(e.load(
        "[e4]\n"
        "[encoderbank]\n"
        "    firstencoder = E1.1\n"
        "    discrete = 32\n"
        "    output1 = O1\n").ok);
    e.tick();
    RingDisplay rd = e.encoderRingInfo(1);
    CHECK(rd.active);
    CHECK(!rd.bipolar);
    CHECK_NEAR(rd.value, 0.0, 1e-6);      // switch position 0 -> bottom-center
    CHECK_NEAR(rd.color, 1.2, 1e-6);      // no color param -> default blue
    // A turn moves the switch: sensivity 1 -> 8 positions per turn (96/12
    // detents per position). 12 detents = position 1 -> value 1/31.
    e.turnEncoder("E1.1", 12);
    e.tick();
    CHECK_NEAR(e.encoderRingInfo(1).value, 1.0 / 31.0, 1e-4);
}

TEST(ring_single_encoder_circuit) {
    Engine e;
    CHECK(e.load(
        "[e4]\n"
        "[encoder]\n"
        "    encoder = E1.1\n"
        "    mode = 2\n"
        "    negativecolor = 0.4\n"
        "    output = O1\n").ok);
    // mode 2 seeds at startvalue 0 -> pos 0 -> logical -1 (full negative)
    e.tick();
    RingDisplay rd = e.encoderRingInfo(1);
    CHECK(rd.active);
    CHECK(rd.bipolar);
    CHECK_NEAR(rd.value, -1.0, 1e-4);
    CHECK_NEAR(rd.negColor, 0.4, 1e-6);
    CHECK_NEAR(rd.color, 1.2, 1e-6);      // default blue
}
