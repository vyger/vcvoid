#pragma once
// Adaptive tick-rate mapping (issue #3): derive the engine tick rate from the
// loaded patch's RAM footprint, reproducing the hardware's load-dependent
// cycle-time stretch and bounding circuitCount x ticksPerSecond.
//
// Rack-free on purpose: the headless unit suite compiles this directly
// (tests/unit/test_adaptiverate.cpp). This header is the ONLY place the curve
// lives — recalibrate by editing these constants and their tests together.
//
// Anchors (manual, hardware.md/architecture.md): empty master cycle ~180 us
// (~5.5 kHz); a full ~110 000-byte patch ~500 us (2 kHz) -> linear fit
// kUsPerRamByte = (500 - 180) / 110000.
namespace vcvoid {

constexpr float kBaseCycleUs   = 180.0f;
constexpr float kUsPerRamByte  = 0.0029f;
constexpr float kMinAdaptiveHz = 2000.0f;
constexpr float kMaxAdaptiveHz = 5555.0f;   // 1e6 / kBaseCycleUs, rounded down

inline float adaptiveTickHz(unsigned patchRamBytes) {
    float cycleUs = kBaseCycleUs + kUsPerRamByte * (float)patchRamBytes;
    float hz = 1e6f / cycleUs;
    if (hz < kMinAdaptiveHz) hz = kMinAdaptiveHz;
    if (hz > kMaxAdaptiveHz) hz = kMaxAdaptiveHz;
    return hz;
}

} // namespace vcvoid
