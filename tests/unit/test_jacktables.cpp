#include "harness.hpp"
#include "gen/jacktables.gen.hpp"
using namespace droid::gen;

TEST(jacktables_copy) {
    const CircuitDef* c = findCircuit("copy");
    CHECK(c != nullptr);
    CHECK(c->ramSize == 24);
    CHECK(!c->deprecated);
    int idx = 0;
    const JackDef* in = findJack(*c, "input", idx);
    CHECK(in && in->isInput && in->type == JackType::Cv && idx == 1);
    CHECK(in->hasDefault); CHECK_NEAR(in->defaultValue, 0.0, 1e-9);
    const JackDef* out = findJack(*c, "o", idx);   // short alias
    CHECK(out && !out->isInput && idx == 1);
    CHECK(findJack(*c, "nonsense", idx) == nullptr);
}

TEST(jacktables_arrays_and_aliases) {
    const CircuitDef* m = findCircuit("mixer");
    CHECK(m != nullptr);
    int idx = 0;
    const JackDef* j = findJack(*m, "input3", idx);
    CHECK(j && idx == 3 && j->count == 8);
    j = findJack(*m, "i8", idx);
    CHECK(j && idx == 8);
    CHECK(findJack(*m, "input9", idx) == nullptr);   // out of range
    const CircuitDef* l = findCircuit("lfo");
    j = findJack(*l, "taptempo", idx);
    CHECK(j && j->ramHint == RamHint::TaptempoInput);
}

TEST(jacktables_comma_separated_array_name) {
    // Regression: firmware lists some array jacks as "input1, input2"
    // (comma-separated) rather than "input1 ... input8" (space-separated).
    // jackgen's base_name() must strip the trailing comma before stripping
    // digits, or the jack ends up named "input1," and findJack("input1"/
    // "input2") never matches it.
    const CircuitDef* m = findCircuit("math");
    CHECK(m != nullptr);
    int idx = 0;
    const JackDef* j = findJack(*m, "input1", idx);
    CHECK(j && idx == 1 && j->count == 2);
    j = findJack(*m, "input2", idx);
    CHECK(j && idx == 2);
}

TEST(jacktables_literal_name_ending_in_digit) {
    // Regression: some scalar (count 1) jacks have a literal name/short
    // alias that itself ends in a digit -- math's "log2"/"l2". The naive
    // splitParam-based array lookup would misparse "log2" as stem "log"
    // index 2 and fail to find it (and "log" isn't even a jack on this
    // circuit). findJack must try an exact, unsplit match before falling
    // back to the array-index split.
    const CircuitDef* m = findCircuit("math");
    CHECK(m != nullptr);
    int idx = 0;
    const JackDef* j = findJack(*m, "log2", idx);
    CHECK(j && idx == 1 && j->count == 1);
    j = findJack(*m, "l2", idx);
    CHECK(j && idx == 1);
    // "logarithm" (short "l") must still resolve independently.
    j = findJack(*m, "logarithm", idx);
    CHECK(j && idx == 1);
    j = findJack(*m, "l", idx);
    CHECK(j && idx == 1);
}

TEST(jacktables_digit_ending_array_base) {
    // Regression: matrixmixer/fadermatrix expose button11 … button44 and
    // led11 … led44 -- four arrays whose BASE names themselves end in a digit
    // (button1 … button4, led1 … led4), each of count 4. jackgen must name the
    // bases from the firmware "prefix" (not strip all trailing digits, which
    // collapses all four to "button"), and findJack must resolve "button23" as
    // base "button2" + index 3 via a longest-prefix match -- NOT base "button"
    // (nonexistent) + index 23.
    const CircuitDef* m = findCircuit("matrixmixer");
    CHECK(m != nullptr);
    int idx = 0;
    const JackDef* j = findJack(*m, "button11", idx);
    CHECK(j && idx == 1 && j->count == 4 && j->isInput);
    j = findJack(*m, "button23", idx);          // row 2, col 3
    CHECK(j && idx == 3);
    const JackDef* j2 = findJack(*m, "button21", idx);
    CHECK(j2 && idx == 1 && j2 == j);            // same jack def (base button2)
    j = findJack(*m, "button44", idx);
    CHECK(j && idx == 4);
    CHECK(findJack(*m, "button45", idx) == nullptr);   // col out of range
    // short aliases: b2 + col -> "b23"
    j = findJack(*m, "b23", idx);
    CHECK(j && idx == 3);
    // LEDs (outputs) resolve the same way.
    j = findJack(*m, "led34", idx);
    CHECK(j && idx == 4 && !j->isInput);
    // A bare base name still resolves to element 1 (exact match).
    j = findJack(*m, "button2", idx);
    CHECK(j && idx == 1);
}

TEST(jacktables_globals) {
    CHECK(kNumCircuits == 76);
    CHECK(kAvailableMemory[0] == 112867);
    CHECK(kAvailableMemory[1] == 109015);
    CHECK(kInitialJacktableSize == 168);
    const CircuitDef* dep = findCircuit("togglebutton");
    CHECK(dep && dep->deprecated);
    const ControllerDef* p2b8 = findController("p2b8");
    CHECK(p2b8 && p2b8->ramSize == 144);
    CHECK(findCircuit("flobbulator") == nullptr);
}

TEST(jacktables_duplicate_name_input_and_output) {
    // Regression: algoquencer uses `pitch` for BOTH the pitch1..pitch16 input
    // array and the scalar CV output (the only such duplicate in blue-7).
    // Bare "pitch" must resolve to the scalar output (scalar beats array base
    // in the exact-match pass) so patch text `pitch = O1` binds the output;
    // "pitch3" still reaches the input array. The wantInput filter lets
    // Circuit::in()/out() pick their own direction for the same name.
    const CircuitDef* a = findCircuit("algoquencer");
    CHECK(a != nullptr);
    int idx = 0;
    const JackDef* j = findJack(*a, "pitch", idx);            // loader view
    CHECK(j && !j->isInput && j->count == 1 && idx == 1);
    j = findJack(*a, "pitch3", idx);
    CHECK(j && j->isInput && j->count == 16 && idx == 3);
    j = findJack(*a, "pitch", idx, /*wantInput=*/1);          // in("pitch")
    CHECK(j && j->isInput && j->count == 16 && idx == 1);
    j = findJack(*a, "pitch", idx, /*wantInput=*/0);          // out("pitch")
    CHECK(j && !j->isInput && j->count == 1);
}

TEST(jacktables_zero_based_array) {
    // Regression: calibrator's tune0..tune8 is the firmware's only zero-based
    // array jack (start_at 0, count 9). "tune0" must resolve to internal index
    // 1 and "tune8" to 9; "tune9" is out of range. 1-based arrays still reject
    // a 0 suffix ("input0" on mixer).
    const CircuitDef* c = findCircuit("calibrator");
    CHECK(c != nullptr);
    int idx = 0;
    const JackDef* j = findJack(*c, "tune0", idx);
    CHECK(j && j->count == 9 && j->startAt == 0 && idx == 1);
    j = findJack(*c, "tune8", idx);
    CHECK(j && idx == 9);
    CHECK(findJack(*c, "tune9", idx) == nullptr);
    const CircuitDef* m = findCircuit("mixer");
    CHECK(findJack(*m, "input0", idx) == nullptr);
}
