#pragma once
// DROID coloured-LED value -> RGB table. Pure C++, NO Rack includes, so it
// links into the headless unit-test build as well as the Rack widget.
//
// Source: manual/basics.md §5.5 "Overriding the LEDs of master, G8 and X7"
// (the "| Value | Color |" table, ~line 2895). The manual documents:
//   "Sending a value of 0.0 to such a register makes the corresponding LED
//    dark. Other values select a color at full brightness. Here is the table
//    of colors (intermediate values give intermediate colors):"
//
//   | Value | Color   |
//   |-------|---------|
//   | 0.2   | cyan    |
//   | 0.4   | green   |
//   | 0.6   | yellow  |
//   | 0.73  | orange  |
//   | 0.8   | red     |
//   | 1.0   | magenta |
//   | 1.1   | violet  |
//   | 1.2   | blue    |
//
// The manual names colours but gives NO RGB triples, so the RGB values below
// are our interpretation of the standard hues (structure the test pins: 0 =
// dark, the named hues at their documented values, continuous interpolation
// between stops). Above the last stop (>= 1.2) we clamp to blue.
//
// BELOW the first named stop (0 < v < 0.2) the manual's own wording settles it.
// Only the exact value 0.0 is dark -- "Sending a value of 0.0 to such a register
// makes the corresponding LED dark. Other values select a color at FULL
// BRIGHTNESS" -- so that region is a hue, not a fade to black. The named stops
// sit on a straight hue line, hue = 180 deg - 300 deg * (v - 0.2): 0.2 cyan
// 180, 0.4 green 120, 0.6 yellow 60, 0.8 red 0, 1.0 magenta 300, 1.2 blue 240
// (0.73 orange is the one listed deviation from the line and is kept as
// listed). Continued backwards the line reaches 240 deg -- blue -- as v -> 0,
// passing through azure at 0.1. That is also what the motoquencer/encoquencer
// `buttoncolor` default of 0.1 asks for: their LED table calls the buttonmode-0
// gate LED "blue" (motoquencer.md "LED colors"; issue #76). So the leading stop
// is blue and 0..0.2 blends blue -> cyan, with exact 0 still dark.
//
// A NEGATIVE value is the WHITE sentinel (SeqCore::kLedWhite): the motoquencer/
// encoquencer played-step marker ("white | currently played step | always",
// motoquencer.md LED colors). White is not in the manual's value table, so the
// engine encodes it out-of-band.
namespace droid {
namespace color {

struct RGB { float r, g, b; };

inline RGB fromValue(float v) {
    if (v < 0.f) return {1.f, 1.f, 1.f};    // white sentinel (played step)
    if (v == 0.f) return {0.f, 0.f, 0.f};

    // (value, RGB) stops, ascending by value. The leading stop continues the hue
    // line below cyan (see the header comment), so 0..0.2 blends blue -> cyan
    // and 0.1 lands on azure; the trailing stop clamps the top.
    struct Stop { float v; RGB c; };
    static const Stop stops[] = {
        {0.00f, {0.f, 0.f, 1.f}},   // blue (the hue line's limit as v -> 0)
        {0.20f, {0.f, 1.f, 1.f}},   // cyan
        {0.40f, {0.f, 1.f, 0.f}},   // green
        {0.60f, {1.f, 1.f, 0.f}},   // yellow
        {0.73f, {1.f, 0.5f, 0.f}},  // orange
        {0.80f, {1.f, 0.f, 0.f}},   // red
        {1.00f, {1.f, 0.f, 1.f}},   // magenta
        {1.10f, {0.5f, 0.f, 1.f}},  // violet
        {1.20f, {0.f, 0.f, 1.f}},   // blue
    };
    const int n = (int)(sizeof(stops) / sizeof(stops[0]));

    if (v >= stops[n - 1].v) return stops[n - 1].c;   // clamp above blue

    for (int i = 1; i < n; i++) {
        if (v <= stops[i].v) {
            const Stop& a = stops[i - 1];
            const Stop& b = stops[i];
            float t = (v - a.v) / (b.v - a.v);         // 0..1 within the segment
            return {a.c.r + (b.c.r - a.c.r) * t,
                    a.c.g + (b.c.g - a.c.g) * t,
                    a.c.b + (b.c.b - a.c.b) * t};
        }
    }
    return stops[n - 1].c;   // unreachable
}

}  // namespace color
}  // namespace droid
