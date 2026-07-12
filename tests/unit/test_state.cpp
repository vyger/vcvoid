// test_state.cpp — DROIDSTA.BIN state-persistence contract (hardware.md §11.1).
// Covers the type+ordinal matching rules (Engine::saveState/restoreState) and a
// manual-interaction round-trip per stateful circuit family: the dialed state of
// a circuit must survive a patch reload (fresh engine, same patch), which is
// exactly what the plugin's hot-reload + Rack save/reopen paths rely on.
#include "harness.hpp"
#include "src/engine.hpp"
#include <string>
#include <vector>
using namespace droid;

// Rising-edge a trigger input: one high tick (the edge) then one low tick.
static void press(Engine& e, const char* in) {
    e.setValue(in, 1.0f); e.tick();
    e.setValue(in, 0.0f); e.tick();
}

// Two toggle buttons (states=2 -> output 0/1, within the O-register ±10V clamp).
// Each press flips its button; distinct press counts give distinguishable state.
static const char* kTwoButtons =
    "[button]\n button = I1\n output = O1\n"
    "[button]\n button = I2\n output = O2\n";

// ---------------------------------------------------------------------------
// Matching rules
// ---------------------------------------------------------------------------

TEST(state_same_type_ordinal_mapping) {
    // Two same-type circuits: each button's dialed state loads back into the
    // button at the same per-type ordinal.
    Engine a; CHECK(a.load(kTwoButtons).ok);
    press(a, "I1");                   // button 1 -> on (1)
    press(a, "I2"); press(a, "I2");   // button 2 -> off (0)
    CHECK_NEAR(a.getValue("O1"), 1.0, 1e-6);
    CHECK_NEAR(a.getValue("O2"), 0.0, 1e-6);
    StateSnapshot snap = a.saveState();
    CHECK(snap.entries.size() == 2);
    CHECK(snap.entries[0].type == "button" && snap.entries[0].ordinal == 1);
    CHECK(snap.entries[1].ordinal == 2);

    Engine b; CHECK(b.load(kTwoButtons).ok);
    b.restoreState(snap);
    b.tick();
    CHECK_NEAR(b.getValue("O1"), 1.0, 1e-6);
    CHECK_NEAR(b.getValue("O2"), 0.0, 1e-6);
}

TEST(state_extra_circuit_defaults) {
    // Save from a 1-button patch; load into a 2-button patch. The extra (2nd)
    // button has no matching saved state -> starts at its default (0).
    Engine a; CHECK(a.load("[button]\n button = I1\n output = O1\n").ok);
    press(a, "I1");                            // toggle on
    StateSnapshot snap = a.saveState();

    Engine b; CHECK(b.load(kTwoButtons).ok);
    b.restoreState(snap);
    b.tick();
    CHECK_NEAR(b.getValue("O1"), 1.0, 1e-6);   // matched
    CHECK_NEAR(b.getValue("O2"), 0.0, 1e-6);   // extra -> default
}

TEST(state_surplus_ignored) {
    // Save from a 2-button patch; load into a 1-button patch. The surplus saved
    // state (button 2) is silently ignored (no crash, no effect).
    Engine a; CHECK(a.load(kTwoButtons).ok);
    press(a, "I1");   // button 1 -> 1
    press(a, "I2");   // button 2 -> 1 (will be surplus)
    StateSnapshot snap = a.saveState();

    Engine b; CHECK(b.load("[button]\n button = I1\n output = O1\n").ok);
    b.restoreState(snap);
    b.tick();
    CHECK_NEAR(b.getValue("O1"), 1.0, 1e-6);
}

TEST(state_reorder_misassign) {
    // Reordering same-type circuits in the file mis-assigns state (documented
    // hardware behavior): the state saved as "button 1" loads into whichever
    // button now appears first, regardless of which jacks it drives.
    Engine a; CHECK(a.load(kTwoButtons).ok);   // O1=first, O2=second
    press(a, "I1");                            // button 1 -> 1  (saved ordinal 1)
    press(a, "I2"); press(a, "I2");            // button 2 -> 0  (saved ordinal 2)
    StateSnapshot snap = a.saveState();

    // Same two circuits, swapped in file order: O2's circuit now appears first.
    Engine b; CHECK(b.load(
        "[button]\n button = I2\n output = O2\n"
        "[button]\n button = I1\n output = O1\n").ok);
    b.restoreState(snap);
    b.tick();
    CHECK_NEAR(b.getValue("O2"), 1.0, 1e-6);   // ordinal-1 state (1) -> first circuit
    CHECK_NEAR(b.getValue("O1"), 0.0, 1e-6);   // ordinal-2 state (0), swapped
}

TEST(state_dontsave_not_saved) {
    // dontsave=1 on the first button suppresses SAVING its state: the snapshot
    // has no entry for it (but it still consumes ordinal 1, so the 2nd button
    // stays ordinal 2).
    Engine a; CHECK(a.load(
        "[button]\n dontsave = 1\n button = I1\n output = O1\n"
        "[button]\n button = I2\n output = O2\n").ok);
    press(a, "I1");   // button 1 -> 1 (should NOT be saved)
    press(a, "I2");   // button 2 -> 1
    StateSnapshot snap = a.saveState();
    CHECK(snap.entries.size() == 1);
    CHECK(snap.entries[0].ordinal == 2);

    // Loading into a plain 2-button patch: button 1 has no entry -> default.
    Engine b; CHECK(b.load(kTwoButtons).ok);
    b.restoreState(snap);
    b.tick();
    CHECK_NEAR(b.getValue("O1"), 0.0, 1e-6);   // not restored
    CHECK_NEAR(b.getValue("O2"), 1.0, 1e-6);   // restored at ordinal 2
}

TEST(state_dontsave_not_loaded) {
    // dontsave=1 on the target circuit suppresses LOADING: even with a matching
    // saved entry, the circuit stays at its default.
    Engine a; CHECK(a.load(kTwoButtons).ok);
    press(a, "I1");   // button 1 -> 1
    press(a, "I2");   // button 2 -> 1
    StateSnapshot snap = a.saveState();

    Engine b; CHECK(b.load(
        "[button]\n dontsave = 1\n button = I1\n output = O1\n"
        "[button]\n button = I2\n output = O2\n").ok);
    b.restoreState(snap);
    b.tick();
    CHECK_NEAR(b.getValue("O1"), 0.0, 1e-6);   // dontsave -> not loaded
    CHECK_NEAR(b.getValue("O2"), 1.0, 1e-6);
}

TEST(state_unparseable_blob_defaults) {
    // A blob the circuit cannot parse (wrong version / wrong length) is ignored
    // and the circuit keeps its default state.
    Engine b; CHECK(b.load("[button]\n button = I1\n output = O1\n").ok);
    StateSnapshot bad;
    CircuitState wrongVer{"button", 1, 999, {1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0}};
    CircuitState wrongLen{"button", 1, 1, {1.0, 0.0}};   // too short
    bad.entries = {wrongVer};
    b.restoreState(bad);
    b.tick();
    CHECK_NEAR(b.getValue("O1"), 0.0, 1e-6);   // bad version ignored

    Engine c; CHECK(c.load("[button]\n button = I1\n output = O1\n").ok);
    bad.entries = {wrongLen};
    c.restoreState(bad);
    c.tick();
    CHECK_NEAR(c.getValue("O1"), 0.0, 1e-6);   // bad length ignored
}

// ---------------------------------------------------------------------------
// Per-family round-trips (manual interaction survives a fresh-engine reload)
// ---------------------------------------------------------------------------

TEST(state_roundtrip_button) {
    const char* p = "[button]\n button = I1\n output = O1\n";   // toggle
    Engine a; CHECK(a.load(p).ok);
    press(a, "I1");                            // toggle on
    CHECK_NEAR(a.getValue("O1"), 1.0, 1e-6);
    StateSnapshot snap = a.saveState();
    Engine b; CHECK(b.load(p).ok);
    b.restoreState(snap);
    b.tick();
    CHECK_NEAR(b.getValue("O1"), 1.0, 1e-6);   // toggle state survived
}

TEST(state_roundtrip_encoder) {
    // Dial an E4 encoder half a turn; the virtual value must survive a reload.
    const char* p =
        "[e4]\n"
        "[encoder]\n encoder = E1.1\n smooth = 0\n output = O1\n";
    Engine a; CHECK(a.load(p).ok);
    a.turnEncoder("E1.1", 48);   // 48/96 = half sweep (normal mode)
    a.tick();
    CHECK_NEAR(a.getValue("O1"), 0.5, 1e-3);
    StateSnapshot snap = a.saveState();

    Engine b; CHECK(b.load(p).ok);
    b.restoreState(snap);
    b.tick();                    // no dialing
    CHECK_NEAR(b.getValue("O1"), 0.5, 1e-3);
}

TEST(state_roundtrip_pot_preset) {
    // Virtual pot: park a value into preset 0, wipe the live value, reload, and
    // confirm the SAVED PRESET survives (a fresh engine's preset 0 would be 0).
    const char* p =
        "[p2b8]\n"
        "[pot]\n pot = P1.1\n startvalue = 0\n"
        " savepreset = I1\n loadpreset = I2\n output = O1\n";
    Engine a; CHECK(a.load(p).ok);
    a.tick();                                  // init at P1.1=0 -> virtual value 0
    a.setValue("P1.1", 0.5f); a.tick();        // pickup drags virtual value to 0.5
    CHECK_NEAR(a.getValue("O1"), 0.5, 1e-3);
    press(a, "I1");                            // savepreset 0  (preset_[0] = 0.5)
    a.setValue("P1.1", 0.0f); a.tick();        // drag the live value back to 0
    CHECK_NEAR(a.getValue("O1"), 0.0, 1e-3);
    StateSnapshot snap = a.saveState();

    Engine b; CHECK(b.load(p).ok);
    b.setValue("P1.1", 0.0f);
    b.restoreState(snap);
    press(b, "I2");                            // loadpreset 0 -> back to 0.5
    CHECK_NEAR(b.getValue("O1"), 0.5, 1e-3);

    // Control: without the restore, preset 0 is the default 0.
    Engine c; CHECK(c.load(p).ok);
    c.setValue("P1.1", 0.0f); c.tick();
    press(c, "I2");
    CHECK_NEAR(c.getValue("O1"), 0.0, 1e-3);
}

TEST(state_roundtrip_algoquencer_preset) {
    // Toggle step buttons, park them in preset 0, wipe the live pattern, reload,
    // load the preset back, and confirm the gate pattern matches the original.
    // activity=0.5 / variation=0 / fills=0 / rolls=0 => gates follow the steps.
    const char* p =
        "[algoquencer]\n clock = I1\n"
        " button1 = I2\n button2 = I3\n button3 = I4\n button4 = I5\n"
        " savepreset = I6\n loadpreset = I7\n clear = I8\n gate = O1\n";
    auto dialAndSave = [&](Engine& e) {
        press(e, "I2");   // step 0 on
        press(e, "I4");   // step 2 on
        press(e, "I6");   // savepreset 0
        press(e, "I8");   // clear the live pattern (presets untouched)
    };
    // Reference: run a's sequence after loadpreset.
    auto runGates = [&](Engine& e) {
        std::vector<float> g;
        for (int k = 0; k < 8; k++) {
            e.setValue("I1", 1.0f); e.tick();
            g.push_back(e.getValue("O1"));
            e.setValue("I1", 0.0f); e.tick();
        }
        return g;
    };

    Engine a; CHECK(a.load(p).ok);
    dialAndSave(a);
    StateSnapshot snap = a.saveState();
    press(a, "I7");                 // loadpreset 0 (reference engine)
    std::vector<float> ref = runGates(a);

    Engine b; CHECK(b.load(p).ok);
    b.restoreState(snap);
    press(b, "I7");                 // loadpreset 0 (restored engine)
    std::vector<float> got = runGates(b);

    CHECK(ref.size() == got.size());
    bool anyHigh = false, allEqual = true;
    for (size_t i = 0; i < ref.size(); i++) {
        if (ref[i] > 0.5f) anyHigh = true;
        if (std::fabs(ref[i] - got[i]) > 1e-6f) allEqual = false;
    }
    CHECK(anyHigh);      // the dialed pattern actually fires gates
    CHECK(allEqual);     // restored preset reproduces it
}

TEST(state_roundtrip_encoquencer) {
    // The motivating bug (dfam.ini): dial an encoquencer's steps on E4 encoders,
    // then a patch hot-reload (fresh engine, same patch) must keep the dialed
    // sequence. Trimmed to the essential circuit structure (an E4 + encoquencer
    // driving cv/gate off an external clock) — not the user's file.
    const char* p =
        "[e4]\n"
        "[encoquencer]\n clock = I1\n numsteps = 4\n cv = O1\n gate = O2\n";

    auto dial = [&](Engine& e) {
        // fadermode 0 (pitch/CV), continuous: +d detents -> step cvpos += d/96,
        // and gate auto-switches on. Encoder global i edits step i-1.
        e.turnEncoder("E1.1", 20); e.tick();   // step 0
        e.turnEncoder("E1.2", 40); e.tick();   // step 1
        e.turnEncoder("E1.3", 60); e.tick();   // step 2
    };
    auto runCv = [&](Engine& e) {
        std::vector<float> cv;
        for (int k = 0; k < 6; k++) {
            e.setValue("I1", 1.0f); e.tick();
            cv.push_back(e.getValue("O1"));
            e.setValue("I1", 0.0f); e.tick();
        }
        return cv;
    };

    Engine a; CHECK(a.load(p).ok);
    dial(a);
    StateSnapshot snap = a.saveState();
    std::vector<float> ref = runCv(a);

    Engine b; CHECK(b.load(p).ok);
    b.restoreState(snap);
    std::vector<float> got = runCv(b);

    CHECK(ref.size() == got.size());
    bool anyNonZero = false, allEqual = true;
    for (size_t i = 0; i < ref.size(); i++) {
        if (ref[i] > 1e-4f) anyNonZero = true;
        if (std::fabs(ref[i] - got[i]) > 1e-6f) allEqual = false;
    }
    CHECK(anyNonZero);   // the dialed pitches are non-default
    CHECK(allEqual);     // survive the reload
}
