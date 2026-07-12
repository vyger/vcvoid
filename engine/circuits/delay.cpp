// delay — A tape delay for CVs, gates and numbers. Spec: manual/circuits/delay.md.
// A virtual tape records three tracks in parallel every engine tick: a continuous
// CV (clamped to -1..+1), eight gate tracks (0/1, high iff >= 0.1) and one discrete
// number (rounded, clamped 0..255). Each output reproduces its input shifted by the
// delay time: `delay` seconds, or `delay` clock ticks when `clock` is patched
// (period learned from consecutive edges). `bypass` copies inputs straight to
// outputs. Triggered mode (`sample` patched) records a new sample only on each
// rising edge of `sample`, holding between edges; `timewindow` keeps adapting the
// held sample to the live input for that many seconds after a trigger before
// freezing it.
//
// SPEC-GAP (simplifications from the manual, needing hardware fixtures to verify):
//   * The manual's 256-sample adaptive tape budget (avg one sample / 20 ms, 1 mV
//     interpolation error, min ~5.12 s) is modelled here as a per-engine-tick
//     circular buffer sized from the tick rate (16 s, comfortably above the
//     manual's guaranteed 5.12 s minimum; floor of 65536 ticks at low rates) --
//     so recordings are effectively lossless up to the tape length rather than
//     length-limited by CV chaos. `overflow` therefore fires when the requested
//     delay exceeds the tape, not at the true adaptive-budget exhaustion point
//     (the exact threshold is not derivable from the manual).
//   * `save`/`load`/`filenumber` are SD-card file I/O; the headless engine has no
//     SD card, so they are no-ops.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>
#include <vector>

namespace droid {

class Delay : public Circuit {
    static constexpr double kTapeSeconds = 16.0;  // > manual's 5.12 s guarantee
    static constexpr long   kMinCapTicks = 65536; // generous floor at low rates

public:
    void tick(EngineState& s) override {
        if (cvTape_.empty()) {             // size the tape from the tick rate
            cap_ = (long)std::llround(kTapeSeconds * s.tickRateHz);
            if (cap_ < kMinCapTicks) cap_ = kMinCapTicks;
            cvTape_.assign(cap_, 0.0f);
            numTape_.assign(cap_, 0);
            gateTape_.assign(cap_, 0);
        }

        // --- gather live inputs --------------------------------------------
        float cvin = clampCv(in("cvin").value(s));
        int   numberin = clampNum(in("numberin").value(s));
        uint8_t gates = 0;
        for (int k = 0; k < 8; k++)
            if (in("gatein", k + 1).value(s) >= kGateHighThreshold)
                gates |= uint8_t(1u << k);

        bool bypass = in("bypass").value(s) >= kGateHighThreshold;
        bool triggered = in("sample").connected();

        // --- delay time in ticks -------------------------------------------
        double delay = in("delay").value(s);
        bool clockMode = in("clock").connected();
        if (clockMode && clockGate_.risingEdge(in("clock").value(s))) {
            if (haveClock_) period_ = double(s.tick - lastClock_);
            lastClock_ = s.tick; haveClock_ = true;
        }
        bool delayReady = true;
        long delayTicks = 0;
        if (clockMode) {
            if (period_ > 0.0) delayTicks = std::lround(delay * period_);
            else delayReady = false;                     // period not learned yet
        } else {
            delayTicks = std::lround(delay * (double)s.tickRateHz);
        }
        if (delayTicks < 0) delayTicks = 0;

        // overflow: the requested delay can't fit on the tape -> data is lost.
        bool overflowing = delayTicks >= cap_;
        if (overflowing) {
            delayTicks = cap_ - 1;                        // read the oldest we have
            overflowUntil_ = s.tick + (uint64_t)std::llround(0.5 * (double)s.tickRateHz);
        }

        // --- decide what to record this tick -------------------------------
        float recCv; int recNum; uint8_t recGates;
        if (triggered) {
            bool edge = sampleGate_.risingEdge(in("sample").value(s));
            if (edge) {
                heldCv_ = cvin; heldNum_ = numberin; heldGates_ = gates;
                double tw = (double)in("timewindow").value(s);
                windowUntil_ = s.tick + (uint64_t)std::llround(tw * (double)s.tickRateHz);
                started_ = true;
            } else if (started_ && s.tick < windowUntil_) {
                // still inside the settling window: keep adapting to live input
                heldCv_ = cvin; heldNum_ = numberin; heldGates_ = gates;
            }
            recCv = heldCv_; recNum = heldNum_; recGates = heldGates_;
        } else {
            recCv = cvin; recNum = numberin; recGates = gates;
        }

        // --- write to tape (always recording) ------------------------------
        int w = (int)(s.tick % (uint64_t)cap_);
        cvTape_[w] = recCv;
        numTape_[w] = (uint8_t)recNum;
        gateTape_[w] = recGates;

        // --- produce outputs -----------------------------------------------
        float cvOut; int numOut; uint8_t gateOut;
        if (bypass) {
            cvOut = cvin; numOut = numberin; gateOut = gates;
        } else if (!delayReady || (long)s.tick < delayTicks) {
            cvOut = 0.0f; numOut = 0; gateOut = 0;       // empty tape / not ready
        } else {
            int r = (int)((s.tick - (uint64_t)delayTicks) % (uint64_t)cap_);
            cvOut = cvTape_[r]; numOut = numTape_[r]; gateOut = gateTape_[r];
        }

        out("cvout").set(s, cvOut);
        out("numberout").set(s, (float)numOut);
        for (int k = 0; k < 8; k++)
            out("gateout", k + 1).set(s, (gateOut & (1u << k)) ? 1.0f : 0.0f);
        out("overflow").set(s, s.tick < overflowUntil_ ? 1.0f : 0.0f);
    }

private:
    static float clampCv(float v) { return v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v); }
    static int clampNum(float v) {
        long n = std::lround(v);
        if (n < 0) n = 0; if (n > 255) n = 255;
        return (int)n;
    }

    std::vector<float>   cvTape_;
    std::vector<uint8_t> numTape_;
    std::vector<uint8_t> gateTape_;
    long cap_ = 0;
    GateReader clockGate_, sampleGate_;
    uint64_t lastClock_ = 0, overflowUntil_ = 0, windowUntil_ = 0;
    double period_ = 0.0;
    bool haveClock_ = false, started_ = false;
    float heldCv_ = 0.0f; int heldNum_ = 0; uint8_t heldGates_ = 0;
};

DROID_REGISTER_CIRCUIT(delay, Delay)

} // namespace droid
