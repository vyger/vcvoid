#include "harness.hpp"
#include "src/rng.hpp"
using namespace droid;

TEST(rng_determinism_fixed_seed_raw) {
    uint32_t s = 12345u;
    CHECK(nextRand(s) == 3336926330u);
    CHECK(nextRand(s) == 1697253807u);
    CHECK(nextRand(s) == 2816511904u);
    CHECK(nextRand(s) == 1955480042u);
    CHECK(nextRand(s) == 718842323u);
}

TEST(rng_determinism_fixed_seed_uniform) {
    uint32_t s = 12345u;
    CHECK_NEAR(randUniform(s), 0.776938677, 1e-6);
    CHECK_NEAR(randUniform(s), 0.395172656, 1e-6);
    CHECK_NEAR(randUniform(s), 0.655770242, 1e-6);
    CHECK_NEAR(randUniform(s), 0.455295622, 1e-6);
    CHECK_NEAR(randUniform(s), 0.167368472, 1e-6);
}

TEST(rng_range_bounds) {
    uint32_t s = 999u;
    for (int i = 0; i < 10000; i++) {
        float u = randUniform(s);
        CHECK(u >= 0.0f);
        CHECK(u < 1.0f);
    }
}

TEST(rng_never_zero_state_for_nonzero_seed) {
    // xorshift32 has a fixed point at s=0; any nonzero seed should never
    // reach it via this transform in a short run (sanity, not exhaustive).
    uint32_t s = 1u;
    for (int i = 0; i < 1000; i++) {
        nextRand(s);
        CHECK(s != 0u);
    }
}

TEST(rng_coarse_uniformity) {
    // 10k draws from a fixed seed, bucketed into 10 equal-width buckets.
    // Expected count per bucket is 1000; allow generous +/-15% tolerance
    // to avoid flakiness while still catching a badly broken generator.
    uint32_t s = 42u;
    int buckets[10] = {0};
    for (int i = 0; i < 10000; i++) {
        float u = randUniform(s);
        int b = static_cast<int>(u * 10.0f);
        if (b < 0) b = 0;
        if (b > 9) b = 9;
        buckets[b]++;
    }
    // pinned exact counts for seed 42 (deterministic, computed once)
    static const int expected[10] = {1064, 994, 999, 1013, 1039, 993, 922, 962, 1029, 985};
    for (int i = 0; i < 10; i++) {
        CHECK(buckets[i] == expected[i]);
        CHECK(buckets[i] > 700 && buckets[i] < 1300); // generous sanity bound
    }
}
