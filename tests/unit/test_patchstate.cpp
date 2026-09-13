// test_patchstate.cpp — per-patch circuit state (issue #42).
//
// Three layers, all headless: the structural fingerprint (what counts as "the
// same patch"), Engine::migrateState (following a circuit across a structural
// edit), and PatchStateStore (which snapshot a load starts from, and the
// promise that nothing is ever evicted).
#include "harness.hpp"
#include "src/engine.hpp"
#include "src/patchstate.hpp"
#include <string>
#include <vector>
using namespace droid;

static void press(Engine& e, const char* in) {
    e.setValue(in, 1.0f); e.tick();
    e.setValue(in, 0.0f); e.tick();
}

// ---------------------------------------------------------------------------
// Fingerprint
// ---------------------------------------------------------------------------

TEST(fingerprint_stable_across_param_and_comment_edits) {
    const char* a =
        "# My patch\n"
        "# O1: [CLK] master clock\n"
        "[lfo]\n hz = 5\n square = O1\n"
        "[button]\n button = I1\n output = O2\n";
    // Same circuits, different title/labels/parameter values/whitespace.
    const char* b =
        "# A completely different title\n"
        "# O1: [TICK] the clock, renamed\n"
        "# O2: [GO] and a new label\n"
        "\n"
        "[lfo]\n"
        "    hz = 9\n"
        "    square = O1\n"
        "[button]\n"
        "    button = I2\n"
        "    output = O2\n";
    CHECK(patchFingerprint(a) == patchFingerprint(b));
    CHECK(patchFingerprint(a).size() == 16);
}

TEST(fingerprint_ignores_controller_declarations) {
    // Controllers hold no state, so swapping one must not orphan the snapshot.
    CHECK(patchFingerprint("[p2b8]\n[lfo]\n hz = 5\n square = O1\n") ==
          patchFingerprint("[p4b2]\n[lfo]\n hz = 5\n square = O1\n"));
}

TEST(fingerprint_changes_on_structural_edits) {
    const std::string base = "[lfo]\n hz = 5\n square = O1\n"
                             "[button]\n button = I1\n output = O2\n";
    const std::string inserted = "[copy]\n input = I1\n output = O3\n" + base;
    const std::string removed = "[lfo]\n hz = 5\n square = O1\n";
    const std::string reordered = "[button]\n button = I1\n output = O2\n"
                                  "[lfo]\n hz = 5\n square = O1\n";
    CHECK(patchFingerprint(base) != patchFingerprint(inserted));
    CHECK(patchFingerprint(base) != patchFingerprint(removed));
    CHECK(patchFingerprint(base) != patchFingerprint(reordered));
}

// ---------------------------------------------------------------------------
// `# STATE:` tag + path normalisation
// ---------------------------------------------------------------------------

TEST(state_tag_header_only) {
    CHECK(patchStateTag("# Title\n# STATE: my-rig\n[lfo]\n hz = 5\n") == "my-rig");
    // Declaring a controller does not close the header.
    CHECK(patchStateTag("# Title\n[p2b8]\n# STATE: my-rig\n[lfo]\n hz = 5\n") == "my-rig");
    // Past the first circuit it is just a comment.
    CHECK(patchStateTag("# Title\n[lfo]\n hz = 5\n# STATE: my-rig\n").empty());
    // Past a section separator, likewise.
    CHECK(patchStateTag("# Title\n# ------\n# STATE: my-rig\n[lfo]\n hz = 5\n").empty());
    CHECK(patchStateTag("# Just a title\n[lfo]\n hz = 5\n").empty());
}

TEST(path_normalisation) {
    CHECK(normalizePatchPath("/a/b//c.ini") == "/a/b/c.ini");
    CHECK(normalizePatchPath("./a/./b.ini") == "a/b.ini");
    CHECK(normalizePatchPath("  /a/b.ini  ") == "/a/b.ini");
    CHECK(normalizePatchPath("C:\\patches\\droid.ini") == "C:/patches/droid.ini");
}

// ---------------------------------------------------------------------------
// Migration
// ---------------------------------------------------------------------------

// Two toggle buttons driving distinct cables, so each has its own signature.
static const std::string kButtonsAB =
    "[button]\n button = I1\n output = _A\n"
    "[button]\n button = I2\n output = _B\n"
    "[copy]\n input = _A\n output = O1\n"
    "[copy]\n input = _B\n output = O2\n";

TEST(migrate_signature_survives_insert_before) {
    // A new button of the SAME type is inserted ahead of both existing ones.
    // The hardware rule would slide every saved state down one slot; the
    // signature match keeps _A and _B with their own buttons.
    Engine a; CHECK(a.load(kButtonsAB).ok);
    press(a, "I1");                           // _A -> 1
    press(a, "I2"); press(a, "I2");           // _B -> 0
    StateSnapshot snap = a.saveState();
    CHECK(snap.entries.size() == 2);
    CHECK(snap.entries[0].signature == "_A");
    CHECK(snap.entries[1].signature == "_B");

    const std::string edited =
        "[button]\n button = I3\n output = _NEW\n"
        "[copy]\n input = _NEW\n output = O3\n" + kButtonsAB;
    Engine b; CHECK(b.load(edited).ok);
    b.migrateState(snap);
    b.tick();
    CHECK_NEAR(b.getValue("O1"), 1.0, 1e-6);   // _A's button kept its state
    CHECK_NEAR(b.getValue("O2"), 0.0, 1e-6);   // _B's button kept its state
    CHECK_NEAR(b.getValue("O3"), 0.0, 1e-6);   // the new one starts at default

    // ... and the plain hardware rule really would have got this wrong.
    Engine c; CHECK(c.load(edited).ok);
    c.restoreState(snap);
    c.tick();
    CHECK_NEAR(c.getValue("O3"), 1.0, 1e-6);   // _A's state landed on the new button
}

TEST(migrate_other_type_insert_keeps_everything) {
    // Adding a circuit of a DIFFERENT type must not disturb any saved state,
    // even though it shifts every later circuit's position in the file.
    Engine a; CHECK(a.load(kButtonsAB).ok);
    press(a, "I1");
    press(a, "I2"); press(a, "I2");
    StateSnapshot snap = a.saveState();

    const std::string edited =
        "[lfo]\n hz = 5\n square = O5\n" + kButtonsAB +
        "[lfo]\n hz = 3\n square = O6\n";
    Engine b; CHECK(b.load(edited).ok);
    b.migrateState(snap);
    b.tick();
    CHECK_NEAR(b.getValue("O1"), 1.0, 1e-6);
    CHECK_NEAR(b.getValue("O2"), 0.0, 1e-6);
}

// Two motoquencers on their own fader banks, each with its own output cables —
// the case the design is really about, since a motoquencer's state is a whole
// sequence rather than one bit.
// quantize = 0 keeps the fader-to-CV mapping continuous and direct, so each
// lane's step 1 reads back as an unmistakable number.
static const std::string kTwoLanes =
    "[m4]\n[m4]\n"
    "[motoquencer]\n clock = I1\n quantize = 0\n"
    " firstfader = 1\n numfaders = 3\n numsteps = 3\n cv = _LANE1_CV\n"
    "[motoquencer]\n clock = I1\n quantize = 0\n"
    " firstfader = 4\n numfaders = 3\n numsteps = 3\n cv = _LANE2_CV\n"
    "[copy]\n input = _LANE1_CV\n output = O1\n"
    "[copy]\n input = _LANE2_CV\n output = O2\n";

// Clock once and read what each lane plays on its first step.
static void playStep1(Engine& e) {
    e.tick();                       // warm-up: the circuits take their faders
    e.setValue("I1", 1.0f); e.tick();
}
// Dial step 1 of each lane to a distinct pitch, then play it.
static void dialLanes(Engine& e) {
    e.tick();                       // warm-up: the circuits take their faders
    e.moveFader("F1", 0.3f);
    e.moveFader("F4", 0.8f);
    e.tick();
    e.setValue("I1", 1.0f); e.tick();
}

TEST(migrate_motoquencers_survive_other_type_insert) {
    Engine a; CHECK(a.load(kTwoLanes).ok);
    dialLanes(a);
    double lane1 = a.getValue("O1"), lane2 = a.getValue("O2");
    CHECK(std::fabs(lane1 - lane2) > 1e-6);   // the two lanes really do differ
    StateSnapshot snap = a.saveState();

    // Add circuits of a DIFFERENT type, before and after: every motoquencer
    // keeps its own sequence even though every position in the file moved.
    const std::string edited =
        "[lfo]\n hz = 5\n square = O5\n" + kTwoLanes +
        "[lfo]\n hz = 3\n square = O6\n";
    Engine b; CHECK(b.load(edited).ok);
    b.migrateState(snap);
    playStep1(b);
    CHECK_NEAR(b.getValue("O1"), lane1, 1e-4);
    CHECK_NEAR(b.getValue("O2"), lane2, 1e-4);
}

TEST(migrate_motoquencers_survive_same_type_insert_before) {
    Engine a; CHECK(a.load(kTwoLanes).ok);
    dialLanes(a);
    double lane1 = a.getValue("O1"), lane2 = a.getValue("O2");
    StateSnapshot snap = a.saveState();

    // A THIRD motoquencer, on its own faders, inserted ahead of both existing
    // lanes. Positionally it would inherit lane 1's sequence and push every
    // other lane's state one slot along; the output-cable signature stops that.
    const std::string edited =
        "[m4]\n[m4]\n"
        "[motoquencer]\n clock = I1\n quantize = 0\n"
        " firstfader = 7\n numfaders = 2\n numsteps = 2\n cv = _LANE3_CV\n"
        "[motoquencer]\n clock = I1\n quantize = 0\n"
        " firstfader = 1\n numfaders = 3\n numsteps = 3\n cv = _LANE1_CV\n"
        "[motoquencer]\n clock = I1\n quantize = 0\n"
        " firstfader = 4\n numfaders = 3\n numsteps = 3\n cv = _LANE2_CV\n"
        "[copy]\n input = _LANE1_CV\n output = O1\n"
        "[copy]\n input = _LANE2_CV\n output = O2\n"
        "[copy]\n input = _LANE3_CV\n output = O3\n";
    Engine b; CHECK(b.load(edited).ok);
    b.migrateState(snap);
    playStep1(b);
    CHECK_NEAR(b.getValue("O1"), lane1, 1e-4);
    CHECK_NEAR(b.getValue("O2"), lane2, 1e-4);
    CHECK_NEAR(b.getValue("O3"), 0.0, 1e-4);   // the new lane starts empty

    // ... and the plain hardware rule really would have handed lane 1's
    // sequence to the newly inserted lane.
    Engine c; CHECK(c.load(edited).ok);
    c.restoreState(snap);
    playStep1(c);
    CHECK_NEAR(c.getValue("O3"), lane1, 1e-4);
}

// The interactive start/end range travels with the sequence (issue #62): the
// motivating case is a patch that was EDITED between the save and the load —
// new circuits inserted ahead of the sequencer — where migration, not an exact
// fingerprint hit, is what brings the state back.
TEST(migrate_motoquencer_manual_range_survives_an_edit) {
    const std::string before =
        "[m4]\n"
        "[motoquencer]\n clock = I1\n numsteps = 16\n numfaders = 4\n"
        " endstep = 8\n buttonmode = 1\n cv = _LANE_CV\n"
        " startstepout = _SS\n endstepout = _ES\n"
        "[mixer]\n input1 = _LANE_CV\n input2 = _SS\n input3 = _ES\n output = O1\n";

    Engine a; CHECK(a.load(before).ok);
    a.tick();
    // buttonmode 1: hold plate 3 (the END), then a second finger on plate 2
    // (the START) — the manual's two-finger gesture.
    a.touchFader(3, true); a.pressFaderPlate(3, true); a.tick();
    a.touchFader(2, true); a.pressFaderPlate(2, true); a.tick();
    a.pressFaderPlate(2, false); a.touchFader(2, false);
    a.pressFaderPlate(3, false); a.touchFader(3, false); a.tick();
    CHECK_NEAR(a.getValue("_SS"), 2.0, 1e-6);
    CHECK_NEAR(a.getValue("_ES"), 3.0, 1e-6);
    StateSnapshot snap = a.saveState();

    // The edit from the issue in miniature: circuits of another type inserted
    // before the sequencer, so the fingerprint changes and migration runs.
    const std::string after =
        "[m4]\n[lfo]\n hz = 5\n square = O5\n[lfo]\n hz = 3\n square = O6\n" +
        before.substr(std::string("[m4]\n").size());
    Engine b; CHECK(b.load(after).ok);
    b.migrateState(snap);
    b.tick();
    CHECK_NEAR(b.getValue("_SS"), 2.0, 1e-6);
    CHECK_NEAR(b.getValue("_ES"), 3.0, 1e-6);
}

TEST(migrate_rewired_circuit_falls_back_to_ordinal) {
    // Signatures that match nothing (the cable was renamed) must still land
    // positionally among the leftovers — the hardware rule, as a floor.
    Engine a; CHECK(a.load(kButtonsAB).ok);
    press(a, "I1");
    StateSnapshot snap = a.saveState();

    const std::string renamed =
        "[button]\n button = I1\n output = _RENAMED\n"
        "[button]\n button = I2\n output = _B\n"
        "[copy]\n input = _RENAMED\n output = O1\n"
        "[copy]\n input = _B\n output = O2\n";
    Engine b; CHECK(b.load(renamed).ok);
    b.migrateState(snap);
    b.tick();
    CHECK_NEAR(b.getValue("O1"), 1.0, 1e-6);   // leftover pairing, in order
    CHECK_NEAR(b.getValue("O2"), 0.0, 1e-6);
}

TEST(migrate_dontsave_circuit_is_skipped) {
    // A circuit with dontsave high neither saves nor loads, and must not
    // consume a snapshot entry that belongs to a later circuit of its type.
    const std::string src =
        "[button]\n button = I1\n output = _A\n"
        "[copy]\n input = _A\n output = O1\n";
    Engine a; CHECK(a.load(src).ok);
    press(a, "I1");
    StateSnapshot snap = a.saveState();
    CHECK(snap.entries.size() == 1);

    const std::string dst =
        "[button]\n button = I1\n dontsave = 1\n output = _A\n"
        "[copy]\n input = _A\n output = O1\n";
    Engine b; CHECK(b.load(dst).ok);
    b.tick();                 // let dontsave settle high
    b.migrateState(snap);
    b.tick();
    CHECK_NEAR(b.getValue("O1"), 0.0, 1e-6);
}

// ---------------------------------------------------------------------------
// The store
// ---------------------------------------------------------------------------

static PatchIdentity id(const std::string& text, const std::string& path) {
    return patchIdentity(text, path);
}
static StateSnapshot oneEntry(const char* type, double v) {
    StateSnapshot s;
    CircuitState cs; cs.type = type; cs.ordinal = 1; cs.version = 1;
    cs.values.push_back(v);
    s.entries.push_back(std::move(cs));
    return s;
}

TEST(store_exact_fingerprint_restores) {
    const std::string text = "[lfo]\n hz = 5\n square = O1\n";
    PatchStateStore st;
    st.store(id(text, "/p/a.ini"), oneEntry("lfo", 7), 100);
    StateLookup lk = st.lookup(id(text, "/p/a.ini"));
    CHECK(lk.origin == StateOrigin::Restored);
    CHECK(lk.source && lk.source->state.entries.size() == 1);
    CHECK_NEAR(lk.source->state.entries[0].values[0], 7.0, 1e-9);
}

TEST(store_same_path_different_structure_migrates) {
    const std::string v1 = "[lfo]\n hz = 5\n square = O1\n";
    const std::string v2 = "[lfo]\n hz = 5\n square = O1\n[copy]\n input = I1\n output = O2\n";
    PatchStateStore st;
    st.store(id(v1, "/p/a.ini"), oneEntry("lfo", 7), 100);
    StateLookup lk = st.lookup(id(v2, "/p/a.ini"));
    CHECK(lk.origin == StateOrigin::Migrated);
    CHECK(lk.source && lk.source->path == "/p/a.ini");
}

TEST(store_state_tag_migrates_across_a_rename) {
    const std::string v1 = "# Rig\n# STATE: my-rig\n[lfo]\n hz = 5\n square = O1\n";
    const std::string v2 = "# Rig\n# STATE: my-rig\n[lfo]\n hz = 5\n square = O1\n"
                           "[copy]\n input = I1\n output = O2\n";
    PatchStateStore st;
    st.store(id(v1, "/p/old-name.ini"), oneEntry("lfo", 7), 100);
    StateLookup lk = st.lookup(id(v2, "/p/brand-new-name.ini"));
    CHECK(lk.origin == StateOrigin::Migrated);
}

TEST(store_unrelated_patch_gets_fresh_state) {
    const std::string a = "[lfo]\n hz = 5\n square = O1\n";
    const std::string b = "[button]\n button = I1\n output = O1\n";
    PatchStateStore st;
    st.store(id(a, "/p/a.ini"), oneEntry("lfo", 7), 100);
    StateLookup lk = st.lookup(id(b, "/p/b.ini"));
    CHECK(lk.origin == StateOrigin::Fresh);
    CHECK(lk.source == nullptr);
}

TEST(store_is_unbounded_and_never_evicts) {
    // User state is precious: there is no LRU and no cap. Store many distinct
    // patches and check that every single one is still retrievable.
    PatchStateStore st;
    const int kMany = 300;
    for (int i = 0; i < kMany; i++) {
        std::string text = "[lfo]\n hz = 5\n square = O1\n";
        for (int k = 0; k < i; k++) text += "[copy]\n input = I1\n output = _C" +
                                            std::to_string(k) + "\n";
        st.store(id(text, "/p/" + std::to_string(i) + ".ini"), oneEntry("lfo", i), 100 + i);
    }
    CHECK(st.size() == (size_t)kMany);
    for (int i = 0; i < kMany; i++) {
        std::string text = "[lfo]\n hz = 5\n square = O1\n";
        for (int k = 0; k < i; k++) text += "[copy]\n input = I1\n output = _C" +
                                            std::to_string(k) + "\n";
        StateLookup lk = st.lookup(id(text, "/p/" + std::to_string(i) + ".ini"));
        CHECK(lk.origin == StateOrigin::Restored);
        CHECK(lk.source && !lk.source->state.entries.empty());
        if (lk.source && !lk.source->state.entries.empty())
            CHECK_NEAR(lk.source->state.entries[0].values[0], (double)i, 1e-9);
    }
}

TEST(store_upsert_and_erase_current_only) {
    const std::string a = "[lfo]\n hz = 5\n square = O1\n";
    const std::string b = "[button]\n button = I1\n output = O1\n";
    PatchStateStore st;
    st.store(id(a, "/p/a.ini"), oneEntry("lfo", 1), 100);
    st.store(id(a, "/p/a.ini"), oneEntry("lfo", 2), 200);   // upsert, not append
    st.store(id(b, "/p/b.ini"), oneEntry("button", 3), 300);
    CHECK(st.size() == 2);
    CHECK_NEAR(st.find(patchFingerprint(a))->state.entries[0].values[0], 2.0, 1e-9);

    // "Reset circuit state" drops the CURRENT entry and nothing else.
    CHECK(st.erase(patchFingerprint(a)));
    CHECK(st.size() == 1);
    CHECK(st.find(patchFingerprint(a)) == nullptr);
    CHECK(st.find(patchFingerprint(b)) != nullptr);
    CHECK(!st.erase(patchFingerprint(a)));
}
