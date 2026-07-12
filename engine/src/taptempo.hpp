#pragma once
// engine/src/taptempo.hpp — shared tap-tempo interval estimator (basics.md
// 3.3, previously duplicated verbatim in lfo and burst): keep up to the last
// 3 edge times; two edges give one interval, three average two. A gap > 4 s
// starts a new measurement row. The interval is preset to 0.5 s (2 Hz) until
// first measured.

#include <cstdint>

namespace droid {

struct TapTempo {
    float interval = 0.5f;   // seconds; preset 2 Hz until first measurement

    void record(uint64_t t, float tickRateHz) {
        if (nEdges_ > 0 && double(t - edges_[nEdges_ - 1]) > 4.0 * tickRateHz)
            nEdges_ = 0;
        if (nEdges_ < 3) {
            edges_[nEdges_++] = t;
        } else {
            edges_[0] = edges_[1]; edges_[1] = edges_[2]; edges_[2] = t;
        }
        double intervalTicks = -1.0;
        if (nEdges_ >= 3)      intervalTicks = double(edges_[2] - edges_[0]) / 2.0;
        else if (nEdges_ == 2) intervalTicks = double(edges_[1] - edges_[0]);
        if (intervalTicks > 0.0) interval = float(intervalTicks / tickRateHz);
    }

private:
    uint64_t edges_[3] = {0, 0, 0};
    int      nEdges_ = 0;
};

} // namespace droid
