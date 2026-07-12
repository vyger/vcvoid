// Property tests for the motoquencer/encoquencer "I Feel Lucky" surface. The
// deterministic extremes are pinned by goldens (tests/golden/**/lucky-*.gold); here
// we exercise the RANDOMIZED paths and assert their invariants (values in range,
// luckychance = 0 leaves the sequence untouched, luckyshuffle is a permutation that
// carries every column together). State is observed through the motor-fader
// readback: with fadermode = k the faders rest at storedPos(k), so getValue("F<n>")
// reports each step's value in the selected lane (cvpos, repeats, ratchets, ...).
//
// The MASTER has only 8 input jacks, so luckychance/luckyamount/luckycvbase are
// baked into the patch as literals per test and only fadermode (I1) and the one
// trigger under test (I2) are wired to registers the test can drive.
#include "harness.hpp"
#include "src/engine.hpp"
#include <cmath>
#include <string>
using namespace droid;

// A 4-step motoquencer with continuous pitch (cvpos == CV), the given lucky config
// baked in, and `trig` wired to I2. fadermode is on I1.
static std::string patch(float chance, float amount, float cvbase, const std::string& trig) {
    return "[m4]\n[motoquencer]\n"
           "    numfaders   = 4\n"
           "    quantize    = 0\n"
           "    cvbase      = 0\n"
           "    cvrange     = 1\n"
           "    holdcv      = 0\n"
           "    fadermode   = I1\n"
           "    luckychance = " + std::to_string(chance) + "\n"
           "    luckyamount = " + std::to_string(amount) + "\n"
           "    luckycvbase = " + std::to_string(cvbase) + "\n"
           "    " + trig + " = I2\n";
}

static std::string F(int i) { return "F" + std::to_string(i + 1); }

// Show lane `fm`, dial the four steps to the given fader positions, commit them.
static void setLane(Engine& e, int fm, const float p[4]) {
    e.setValue("I1", (float)fm); e.tick();   // switch mode -> motors recall to rest
    for (int i = 0; i < 4; i++) e.moveFader(F(i), p[i]);
    e.tick();                                // next tick reads the user movement
}
// Show lane `fm` and read the four steps' stored positions off the motors.
static void readLane(Engine& e, int fm, float out[4]) {
    e.setValue("I1", (float)fm); e.tick();   // mode change -> recall to storedPos
    for (int i = 0; i < 4; i++) out[i] = e.getValue(F(i));
}
static void fire(Engine& e) {
    e.setValue("I2", 1.0f); e.tick(); e.setValue("I2", 0.0f);
}

TEST(lucky_chance_zero_leaves_sequence_untouched) {
    Engine e(MasterType::Master16, 100.0f, 1);
    CHECK(e.load(patch(0.0f, 1.0f, 0.0f, "luckyinvert")).ok);
    const float dialed[4] = {0.1f, 0.3f, 0.6f, 0.9f};
    setLane(e, 0, dialed);
    fire(e);                                 // luckyinvert would flip all four...
    float got[4]; readLane(e, 0, got);
    for (int i = 0; i < 4; i++) CHECK_NEAR(got[i], dialed[i], 1e-4);  // ...but nothing changed
}

TEST(lucky_cvs_values_within_amount_window) {
    Engine e(MasterType::Master16, 100.0f, 1);
    CHECK(e.load(patch(1.0f, 0.5f, 0.0f, "luckycvs")).ok);  // window [0, 0.5]
    const float dialed[4] = {0.05f, 0.05f, 0.05f, 0.05f};
    setLane(e, 0, dialed);
    fire(e);
    float got[4]; readLane(e, 0, got);
    bool moved = false;
    for (int i = 0; i < 4; i++) {
        CHECK(got[i] >= 0.0f - 1e-4f);
        CHECK(got[i] <= 0.5f + 1e-4f);
        if (std::fabs(got[i] - dialed[i]) > 1e-4) moved = true;
    }
    CHECK(moved);                            // the reroll actually did something
}

TEST(lucky_repeats_respect_amount_ceiling) {
    Engine e(MasterType::Master16, 100.0f, 1);
    // amount = 0.4 -> max repeats = round(1 + 0.4*15) = round(7) = 7.
    CHECK(e.load(patch(1.0f, 6.0f / 15.0f, 0.0f, "luckyrepeats")).ok);
    fire(e);
    float got[4]; readLane(e, 3, got);       // fadermode 3: storedPos = (repeats-1)/15
    for (int i = 0; i < 4; i++) {
        int repeats = (int)std::lround(got[i] * 15.0f) + 1;
        CHECK(repeats >= 1 && repeats <= 7);
    }
}

TEST(lucky_ratchets_amount_zero_forces_one) {
    Engine e(MasterType::Master16, 100.0f, 1);
    CHECK(e.load(patch(1.0f, 0.0f, 0.0f, "luckyratchets")).ok);  // max ratchets = 1
    fire(e);
    float got[4]; readLane(e, 5, got);       // fadermode 5: storedPos = (ratchets-1)/7
    for (int i = 0; i < 4; i++) {
        int ratchets = (int)std::lround(got[i] * 7.0f) + 1;
        CHECK(ratchets == 1);
    }
}

TEST(lucky_shuffle_permutes_and_carries_columns) {
    Engine e(MasterType::Master16, 100.0f, 1);
    CHECK(e.load(patch(1.0f, 1.0f, 0.0f, "luckyshuffle")).ok);
    // Distinct CVs, and a repeats value uniquely tied to each step (step i -> i+1).
    const float cvs[4]  = {0.1f, 0.3f, 0.6f, 0.9f};
    const float reps[4] = {0.0f, 1.0f / 15.0f, 2.0f / 15.0f, 3.0f / 15.0f};  // repeats 1,2,3,4
    setLane(e, 0, cvs);
    setLane(e, 3, reps);
    fire(e);
    float gotCv[4], gotRep[4];
    readLane(e, 0, gotCv);
    readLane(e, 3, gotRep);
    bool seen[4] = {false, false, false, false};
    for (int i = 0; i < 4; i++) {
        // Recover which original step this one came from via its (distinct) CV.
        int src = -1;
        for (int j = 0; j < 4; j++) if (std::fabs(gotCv[i] - cvs[j]) < 1e-4) src = j;
        CHECK(src >= 0);                     // CV multiset preserved
        if (src < 0) continue;
        CHECK(!seen[src]);                   // used exactly once -> a true permutation
        seen[src] = true;
        int repeats = (int)std::lround(gotRep[i] * 15.0f) + 1;
        CHECK(repeats == src + 1);           // the repeats column moved WITH the CV
    }
    for (int j = 0; j < 4; j++) CHECK(seen[j]);
}
