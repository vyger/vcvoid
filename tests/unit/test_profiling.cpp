#include "harness.hpp"
#include "src/engine.hpp"
#include <string>
using namespace droid;

// Profiling must be observably free: identical outputs with it on or off
// (the issue-#3 determinism guarantee), plus sane snapshot bookkeeping.

static const char* kPatch =
    "[lfo]\n hz = 5\n square = _CLK\n"
    "[copy]\n input = _CLK\n output = O1\n"
    "[contour]\n trigger = _CLK\n attack = 0.1\n release = 0.3\n output = O2\n";

TEST(profiling_does_not_change_results) {
    Engine a, b;
    CHECK(a.load(kPatch).ok);
    CHECK(b.load(kPatch).ok);
    b.setProfiling(true);
    for (int t = 0; t < 2000; t++) {
        a.tick(); b.tick();
        CHECK(a.getValue("O1") == b.getValue("O1"));
        CHECK(a.getValue("O2") == b.getValue("O2"));
    }
}

TEST(profiling_snapshot_shape) {
    Engine e;
    CHECK(e.load(kPatch).ok);
    e.setProfiling(true);
    for (int t = 0; t < 100; t++) e.tick();
    auto snap = e.profileSnapshot();
    CHECK(snap.size() == 3);
    CHECK(std::string(snap[0].name) == "lfo");
    CHECK(std::string(snap[1].name) == "copy");
    CHECK(std::string(snap[2].name) == "contour");
    for (auto& c : snap) {
        CHECK(c.totalUs >= 0.0);
        CHECK(c.ticks == 100);
    }
    // Disabled by default; re-enabling clears.
    e.setProfiling(false);
    CHECK(!e.profilingEnabled());
    e.setProfiling(true);
    CHECK(e.profileSnapshot()[0].ticks == 0);
}
