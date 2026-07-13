#include "harness.hpp"
#include "AdaptiveRate.hpp"

// The load->rate curve is the single calibration point for adaptive timing
// (spec 2026-07-12): these tests document the current constants and must be
// updated in the same commit as any recalibration.

TEST(adaptive_empty_patch_is_base_rate) {
    // 0 bytes -> 1e6/180 = 5555.55 Hz, clamped to kMaxAdaptiveHz.
    CHECK_NEAR(vcvoid::adaptiveTickHz(0), 5555.0, 0.01);
}

TEST(adaptive_full_patch_near_2khz) {
    // ~110000 bytes (full master): cycle = 180 + 0.0029*110000 = 499 us.
    CHECK_NEAR(vcvoid::adaptiveTickHz(110000), 1e6 / 499.0, 0.1);
}

TEST(adaptive_clamps_low) {
    CHECK_NEAR(vcvoid::adaptiveTickHz(1000000), 2000.0, 1e-6);
}

TEST(adaptive_monotonic_decreasing) {
    CHECK(vcvoid::adaptiveTickHz(10000) > vcvoid::adaptiveTickHz(50000));
    CHECK(vcvoid::adaptiveTickHz(50000) > vcvoid::adaptiveTickHz(100000));
}
