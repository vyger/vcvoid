#include "harness.hpp"
#include "../../plugin/src/droidcolor.hpp"   // pure header, no Rack types
#include <cmath>

using namespace droid::color;

static bool inUnit(const RGB& c) {
    return c.r >= 0.f && c.r <= 1.f && c.g >= 0.f && c.g <= 1.f &&
           c.b >= 0.f && c.b <= 1.f;
}

// 0.0 = dark, per manual/basics.md §5.5 ("Sending a value of 0.0 to such a
// register makes the corresponding LED dark"). EXACTLY 0.0 -- "other values
// select a color at full brightness", so the neighbourhood of 0 is a hue.
TEST(droidcolor_dark) {
    RGB dark = fromValue(0.0f);
    CHECK(dark.r == 0.f && dark.g == 0.f && dark.b == 0.f);
}

// Below the first named stop the hue line (hue = 180 deg - 300 deg * (v - 0.2))
// runs back to blue as v -> 0, so 0..0.2 blends blue -> cyan and 0.1 is azure.
// That is the colour the motoquencer/encoquencer `buttoncolor` default of 0.1
// paints on the buttonmode-0 gate LEDs, which the circuits' own LED table calls
// "blue" (issue #76).
TEST(droidcolor_below_cyan) {
    RGB nearZero = fromValue(0.001f);
    CHECK(nearZero.r == 0.f && nearZero.g < 0.01f && nearZero.b == 1.f);
    RGB azure = fromValue(0.1f);
    CHECK(azure.r == 0.f && azure.b == 1.f);
    CHECK(std::fabs(azure.g - 0.5f) < 1e-5f);
}

// Negative = the white sentinel (SeqCore::kLedWhite): the motoquencer/
// encoquencer played-step marker. White is not in the manual's value table,
// so the engine encodes it out-of-band as a negative colour value.
TEST(droidcolor_white_sentinel) {
    RGB w = fromValue(-1.0f);
    CHECK(w.r == 1.f && w.g == 1.f && w.b == 1.f);
    RGB w2 = fromValue(-0.01f);
    CHECK(w2.r == 1.f && w2.g == 1.f && w2.b == 1.f);
}

// Named-colour anchors transcribed from the manual table (RGB is our
// interpretation of each standard hue; the *values* are the manual's).
TEST(droidcolor_anchors) {
    RGB cyan = fromValue(0.2f);
    CHECK(cyan.r == 0.f && cyan.g == 1.f && cyan.b == 1.f);
    RGB green = fromValue(0.4f);
    CHECK(green.r == 0.f && green.g == 1.f && green.b == 0.f);
    RGB yellow = fromValue(0.6f);
    CHECK(yellow.r == 1.f && yellow.g == 1.f && yellow.b == 0.f);
    RGB orange = fromValue(0.73f);
    CHECK(orange.r == 1.f && orange.b == 0.f && orange.g > 0.f && orange.g < 1.f);
    RGB red = fromValue(0.8f);
    CHECK(red.r == 1.f && red.g == 0.f && red.b == 0.f);
    RGB magenta = fromValue(1.0f);
    CHECK(magenta.r == 1.f && magenta.g == 0.f && magenta.b == 1.f);
    RGB violet = fromValue(1.1f);
    CHECK(violet.g == 0.f && violet.r > 0.f && violet.r < 1.f && violet.b == 1.f);
    RGB blue = fromValue(1.2f);
    CHECK(blue.r == 0.f && blue.g == 0.f && blue.b == 1.f);
    // Clamp above the top stop.
    RGB above = fromValue(2.0f);
    CHECK(above.r == 0.f && above.g == 0.f && above.b == 1.f);
}

// Every channel stays in [0,1] across the whole documented range.
TEST(droidcolor_in_range) {
    for (float v = -0.5f; v <= 1.5f; v += 0.005f)
        CHECK(inUnit(fromValue(v)));
}

// Intermediate values interpolate: the ramp is continuous (adjacent samples
// never jump abruptly), as the manual documents ("intermediate values give
// intermediate colors"). Scanned from just above 0: the ONLY intended
// discontinuity is at exactly 0.0, where the manual's dark overrides the hue
// line (covered by droidcolor_dark / droidcolor_below_cyan).
TEST(droidcolor_continuous) {
    const float dv = 0.001f;
    RGB prev = fromValue(dv);
    for (float v = 2 * dv; v <= 1.2f; v += dv) {
        RGB cur = fromValue(v);
        // Largest per-channel slope is the blue->cyan ramp over 0..0.2:
        // 1.0 unit / 0.2 = 5 per unit -> 0.005 per dv step. Allow generous
        // headroom; a discontinuity (a stop jump) would far exceed this.
        CHECK(std::fabs(cur.r - prev.r) < 0.02f);
        CHECK(std::fabs(cur.g - prev.g) < 0.02f);
        CHECK(std::fabs(cur.b - prev.b) < 0.02f);
        prev = cur;
    }
}

// A sample strictly between two stops blends them (cyan 0.2 -> green 0.4):
// midpoint 0.3 keeps g == 1 and has partial blue.
TEST(droidcolor_intermediate_blend) {
    RGB mid = fromValue(0.3f);
    CHECK(mid.g == 1.f);
    CHECK(mid.r == 0.f);
    CHECK(mid.b > 0.f && mid.b < 1.f);
}
