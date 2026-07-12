#pragma once
// engine/src/rng.hpp — xorshift32 over EngineState::rngState.
// All circuit randomness flows through this; goldens pin the seed.

#include <cstdint>

namespace droid {

inline uint32_t nextRand(uint32_t& s) {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s;
}

inline float randUniform(uint32_t& s) {          // [0, 1)
    return (nextRand(s) >> 8) * (1.0f / 16777216.0f);
}

} // namespace droid
