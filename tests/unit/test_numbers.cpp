#include "harness.hpp"
#include "src/numbers.hpp"
#include <cmath>          // std::isinf (was relied on transitively)
using droid::parsePatchNumber;

static float num(const std::string& s) { float v = -999; CHECK(parsePatchNumber(s, v)); return v; }

TEST(numbers_plain) {
    CHECK_NEAR(num("0.45"), 0.45, 1e-7);
    CHECK_NEAR(num("-5.0"), -5.0, 1e-7);
    CHECK_NEAR(num("12345.67"), 12345.67, 1e-2);
    CHECK_NEAR(num("0"), 0.0, 1e-9);
}
TEST(numbers_suffixes) {
    CHECK_NEAR(num("45%"), 0.45, 1e-7);   // % divides by 100
    CHECK_NEAR(num("2V"), 0.2, 1e-7);     // V divides by 10
    CHECK_NEAR(num("-2.5V"), -0.25, 1e-7);
    CHECK_NEAR(num("10V"), 1.0, 1e-7);
}
TEST(numbers_words) {
    CHECK_NEAR(num("on"), 1.0, 1e-9);
    CHECK_NEAR(num("off"), 0.0, 1e-9);
}
TEST(numbers_overflow) {
    // Internal math is unbounded 32-bit float (see CLAUDE.md); a token beyond
    // float range parses successfully and saturates to +inf rather than failing.
    float v = 0;
    CHECK(parsePatchNumber("999999999999999999999999999999999999999", v));
    CHECK(std::isinf(v) && v > 0);
}
TEST(numbers_rejects) {
    float v;
    CHECK(!parsePatchNumber(".5", v));        // no leading dot
    CHECK(!parsePatchNumber("3.4e-10", v));   // no scientific notation
    CHECK(!parsePatchNumber("--1", v));
    CHECK(!parsePatchNumber("1V2", v));
    CHECK(!parsePatchNumber("", v));
    CHECK(!parsePatchNumber("I1", v));
    CHECK(!parsePatchNumber("1.", v));        // digits required after dot
}
