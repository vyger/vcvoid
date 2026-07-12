#include "harness.hpp"
#include "src/controllerstate.hpp"
#include "src/registers.hpp"
using namespace droid;

static RegId rid(const std::string& s) {
    return parseRegisterName(s).value_or(RegId{});
}

TEST(cs_global_index_mixed_chain) {
    // Chain: p2b8 (no enc), e4 (4 enc), db8e (1 enc) -> globals 1..5.
    ControllerState cs;
    cs.configure({"p2b8", "e4", "db8e"});
    CHECK(cs.encoderCount() == 5);
    CHECK(cs.globalIndexForReg(rid("E2.1")) == 1);
    CHECK(cs.globalIndexForReg(rid("E2.4")) == 4);
    CHECK(cs.globalIndexForReg(rid("E3.1")) == 5);
    // reverse
    CHECK(cs.regForGlobal(1) == rid("E2.1"));
    CHECK(cs.regForGlobal(5) == rid("E3.1"));
    // non-existent encoders
    CHECK(cs.globalIndexForReg(rid("E1.1")) == 0);   // p2b8 has no encoders
    CHECK(cs.globalIndexForReg(rid("E2.5")) == 0);   // e4 only has 4
    CHECK(cs.globalIndexForReg(rid("E3.2")) == 0);   // db8e only has 1
    CHECK(cs.validateGlobal(5) == 5);
    CHECK(cs.validateGlobal(6) == 0);
    CHECK(cs.validateGlobal(0) == 0);
}

TEST(cs_two_e4s) {
    ControllerState cs;
    cs.configure({"e4", "e4"});
    CHECK(cs.encoderCount() == 8);
    CHECK(cs.globalIndexForReg(rid("E1.1")) == 1);
    CHECK(cs.globalIndexForReg(rid("E2.1")) == 5);   // 2nd E4, 1st encoder
    CHECK(cs.globalIndexForReg(rid("E2.4")) == 8);
    CHECK(cs.regForGlobal(5) == rid("E2.1"));
}

TEST(cs_detent_accumulate_and_drain) {
    ControllerState cs;
    cs.configure({"e4"});
    // accumulation across several turns before a drain
    CHECK(cs.turnEncoder(rid("E1.1"), 3));
    CHECK(cs.turnEncoder(rid("E1.1"), 2));
    CHECK(cs.turnEncoder(rid("E1.1"), -1));
    CHECK(cs.encoder(1)->pendingDetents == 4);
    // other encoders untouched
    CHECK(cs.encoder(2)->pendingDetents == 0);
    // drain resets all
    cs.endTick();
    CHECK(cs.encoder(1)->pendingDetents == 0);
    // fresh accumulation after drain
    cs.turnEncoder(1, -7);
    CHECK(cs.encoder(1)->pendingDetents == -7);
}

TEST(cs_push_state_is_level) {
    ControllerState cs;
    cs.configure({"e4"});
    CHECK(!cs.encoder(1)->pushed);
    CHECK(cs.pushEncoder(rid("E1.2"), true));
    CHECK(cs.encoder(2)->pushed);
    // push is a level, NOT drained by endTick
    cs.endTick();
    CHECK(cs.encoder(2)->pushed);
    CHECK(cs.pushEncoder(rid("E1.2"), false));
    CHECK(!cs.encoder(2)->pushed);
}

TEST(cs_bare_global_reg_form) {
    // "E<n>" (ctrl == 0) addresses a global index directly.
    ControllerState cs;
    cs.configure({"p2b8", "e4"});   // globals 1..4 map to E2.1..E2.4
    CHECK(cs.turnEncoder(rid("E3"), 5));   // global 3 == E2.3
    CHECK(cs.encoder(3)->pendingDetents == 5);
    CHECK(cs.regForGlobal(3) == rid("E2.3"));
}

TEST(cs_resolution_errors) {
    ControllerState cs;
    cs.configure({"e4"});   // globals 1..4
    // wrong register type
    CHECK(!cs.turnEncoder(rid("P1.1"), 1));
    CHECK(!cs.pushEncoder(rid("B1.1"), true));
    // out-of-range chain register / global
    CHECK(!cs.turnEncoder(rid("E1.5"), 1));   // e4 has only 4
    CHECK(!cs.turnEncoder(rid("E2.1"), 1));   // no 2nd controller
    CHECK(!cs.turnEncoder(rid("E9"), 1));     // global 9 out of range
    // out-of-range index-form is a silent no-op (no crash), encoder() null
    cs.turnEncoder(99, 3);
    CHECK(cs.encoder(99) == nullptr);
    CHECK(cs.encoder(0) == nullptr);
}

TEST(cs_configure_resets_state) {
    ControllerState cs;
    cs.configure({"e4"});
    cs.turnEncoder(1, 5);
    cs.pushEncoder(1, true);
    cs.configure({"e4"});   // reconfigure clears everything
    CHECK(cs.encoder(1)->pendingDetents == 0);
    CHECK(!cs.encoder(1)->pushed);
}

// --- fader tier (M4c) ------------------------------------------------------

TEST(cs_fader_count_and_global_numbering) {
    // 4 faders per M4, enumerated in chain order; other controllers add none.
    ControllerState cs;
    cs.configure({"p2b8", "m4", "e4", "m4"});
    CHECK(cs.faderCount() == 8);
    CHECK(cs.validateFader(1));
    CHECK(cs.validateFader(8));
    CHECK(!cs.validateFader(9));
    CHECK(!cs.validateFader(0));
    // no faders at all when there is no M4
    ControllerState cs2;
    cs2.configure({"p2b8", "e4"});
    CHECK(cs2.faderCount() == 0);
    CHECK(!cs2.validateFader(1));
}

TEST(cs_fader_move_and_touch_are_levels) {
    ControllerState cs;
    cs.configure({"m4"});   // faders 1..4
    cs.moveFader(2, 0.35f);
    CHECK(cs.faderPosition(2) == 0.35f);
    CHECK(cs.fader(1)->position == 0.0f);     // others untouched
    // move clamps to 0..1
    cs.moveFader(2, 1.7f);
    CHECK(cs.faderPosition(2) == 1.0f);
    cs.moveFader(2, -0.5f);
    CHECK(cs.faderPosition(2) == 0.0f);
    // touch is a level, NOT drained by endTick (like encoder push)
    CHECK(!cs.fader(3)->touched);
    cs.touchFader(3, true);
    CHECK(cs.fader(3)->touched);
    cs.endTick();
    CHECK(cs.fader(3)->touched);
    CHECK(cs.faderPosition(2) == 0.0f);       // position also persists
    cs.touchFader(3, false);
    CHECK(!cs.fader(3)->touched);
}

TEST(cs_fader_motor_command_roundtrip) {
    ControllerState cs;
    cs.configure({"m4"});
    // motor command moves position instantly and records the target
    cs.commandFader(1, 0.8f);
    CHECK(cs.faderPosition(1) == 0.8f);
    CHECK(cs.fader(1)->motorTarget == 0.8f);
    // clamps
    cs.commandFader(1, 2.0f);
    CHECK(cs.faderPosition(1) == 1.0f);
    CHECK(cs.fader(1)->motorTarget == 1.0f);
}

TEST(cs_fader_out_of_range_is_safe) {
    ControllerState cs;
    cs.configure({"m4"});
    cs.moveFader(99, 0.5f);        // no crash
    cs.touchFader(0, true);
    cs.commandFader(-1, 0.5f);
    CHECK(cs.fader(99) == nullptr);
    CHECK(cs.fader(0) == nullptr);
    CHECK(cs.faderPosition(99) == 0.0f);
}

TEST(cs_configure_resets_faders) {
    ControllerState cs;
    cs.configure({"m4"});
    cs.moveFader(1, 0.5f);
    cs.touchFader(1, true);
    cs.fader(1)->notches = 8;
    cs.configure({"m4"});
    CHECK(cs.faderPosition(1) == 0.0f);
    CHECK(!cs.fader(1)->touched);
    CHECK(cs.fader(1)->notches == 0);
}

// --- display tier (M4 Wave 3a Task 4) --------------------------------------

TEST(controllerstate_displays) {
    ControllerState cs;
    cs.configure({"p2b8", "db8e", "e4", "db8e"});
    CHECK(cs.displayCount() == 2);            // DB8Es only, chain order
    CHECK(cs.display(0) == nullptr);
    CHECK(cs.display(3) == nullptr);
    CHECK(cs.display(1) != nullptr);
    CHECK(!cs.display(1)->active);
    cs.display(2)->active = true;
    cs.configure({"db8e"});                    // reconfigure resets
    CHECK(cs.displayCount() == 1);
    CHECK(!cs.display(1)->active);
}
