#include "harness.hpp"
#include "src/gatereader.hpp"
using namespace droid;

TEST(gatereader_rising_edge_at_threshold) {
    GateReader g;
    CHECK(!g.risingEdge(0.0f));          // low, no edge
    CHECK(!g.risingEdge(0.09f));         // still below 0.1 (1 V) threshold
    CHECK(g.risingEdge(0.1f));           // at threshold -> high, rising edge
    CHECK(!g.risingEdge(0.1f));          // held high: no double-trigger
}

TEST(gatereader_no_double_trigger_on_held_high) {
    GateReader g;
    CHECK(g.risingEdge(1.0f));
    CHECK(!g.risingEdge(1.0f));
    CHECK(!g.risingEdge(0.5f));          // still above threshold, still no edge
    CHECK(!g.risingEdge(0.99f));
}

TEST(gatereader_falling_edge) {
    GateReader g;
    CHECK(g.risingEdge(1.0f));
    CHECK(!g.fallingEdge(1.0f));         // still high, no falling edge
    CHECK(g.fallingEdge(0.0f));          // drops below threshold -> falling edge
    CHECK(!g.fallingEdge(0.0f));         // held low: no double-trigger
}

TEST(gatereader_falling_edge_exact_threshold) {
    GateReader g;
    g.risingEdge(1.0f);
    CHECK(!g.fallingEdge(0.1f));         // at threshold still counts as high (>=)
    CHECK(g.fallingEdge(0.09999f));      // just below -> falling edge
}

TEST(gatereader_threshold_crossing_multiple_cycles) {
    GateReader g;
    bool sawRising = false, sawFalling = false;
    float samples[] = {0.0f, 0.05f, 0.1f, 0.5f, 1.0f, 0.5f, 0.09f, 0.0f, 0.2f};
    // manually walk risingEdge/fallingEdge together like a circuit would
    for (float v : samples) {
        if (g.risingEdge(v)) sawRising = true;
    }
    // reset and redo with falling detection since risingEdge already mutated state above;
    // use a fresh reader to check falling behavior across the same sequence
    GateReader g2;
    for (float v : samples) {
        if (g2.fallingEdge(v)) sawFalling = true;
    }
    CHECK(sawRising);
    CHECK(sawFalling);
}
