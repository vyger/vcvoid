// clocktool — Clock divider / multiplier / shifter. Spec:
// manual/circuits/clocktool.md. Reconstructs the input clock's period from
// consecutive edges and drives an output-beat phase:
//   outPos = (beat + frac - delay) * multiply / divide
// emitting an output gate whenever floor(outPos) increments. Gate length is a
// fixed `gatelength` (seconds), else `dutycycle` of the output period, else the
// 10 ms default. inputpitch/outputpitch express the clock rate on a 1V/oct base
// (60 BPM = 1 Hz = 0 V). Standard A*B+C input math applies.
//
// SPEC-GAP: multiply/dutycycle/delay all need a learned period, so the first
// input cycle is a transient (period unknown -> no mid pulses / 10 ms gate),
// matching the manual's "short time to learn the clock". Negative delay predicts
// the next edge from the measured period (steady-clock assumption).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>

namespace droid {

class Clocktool : public Circuit {
public:
    void tick(EngineState& s) override {
        long divide = std::lround(in("divide").value(s));
        if (divide < 1) divide = 1;
        long multiply = std::lround(in("multiply").value(s));
        if (multiply < 1) multiply = 1;
        float delay = in("delay").value(s);

        if (resetGate_.risingEdge(in("reset").value(s))) {
            curBeat_ = -1; lastOut_ = -1; havePrev_ = false; period_ = 0.0; gateOff_ = 0;
        }
        if (clockGate_.risingEdge(in("clock").value(s))) {
            if (havePrev_) {
                double gap = double(s.tick - lastEdge_);
                // The frac interpolation (capped at 2.0) fills in beats for up to
                // two periods of missing edges. A longer gap is an indefinite
                // stall: during it frac saturated and manufactured phantom output
                // beats, inflating lastOut_. Resync the output phase to the fresh
                // edge so the resuming clock emits instead of paying off a phantom
                // backlog (manual: a steady clock is required; after a stall the
                // divider realigns). Keep the last known period for interpolation.
                if (period_ > 0.0 && gap > 2.0 * period_) { curBeat_ = -1; lastOut_ = -1; }
                else period_ = gap;
            }
            lastEdge_ = s.tick; havePrev_ = true; curBeat_++;
        }

        double frac = 0.0;
        if (period_ > 0.0 && curBeat_ >= 0) {
            frac = double(s.tick - lastEdge_) / period_;
            if (frac < 0.0) frac = 0.0;
            if (frac > 2.0) frac = 2.0;          // cap if the clock stalls
        }

        if (curBeat_ >= 0) {
            double outPos = (curBeat_ + frac - (double)delay) * (double)multiply / (double)divide;
            long outBeat = (long)std::floor(outPos);
            if (outBeat > lastOut_) {
                lastOut_ = outBeat;
                double gateLen;
                if (in("gatelength").connected())
                    gateLen = (double)in("gatelength").value(s) * s.tickRateHz;
                else if (in("dutycycle").connected()) {
                    double outPeriod = (period_ > 0.0 ? period_ : 0.0) * (double)divide / (double)multiply;
                    gateLen = (double)in("dutycycle").value(s) * outPeriod;
                } else {
                    gateLen = 0.01 * s.tickRateHz;   // 10 ms default
                }
                if (gateLen < 1.0) gateLen = 1.0;
                gateOff_ = s.tick + (uint64_t)std::llround(gateLen);
            }
        }
        out("output").set(s, s.tick < gateOff_ ? 1.0f : 0.0f);

        // pitch outputs (1 V/oct, 60 BPM = 1 Hz = 0 V)
        double inFreq = (period_ > 0.0) ? (double)s.tickRateHz / period_ : 0.0;
        out("inputpitch").set(s, inFreq > 0.0 ? float(std::log2(inFreq) * 0.1) : 0.0f);
        double outFreq = inFreq * (double)multiply / (double)divide;
        out("outputpitch").set(s, outFreq > 0.0 ? float(std::log2(outFreq) * 0.1) : 0.0f);
    }

private:
    GateReader clockGate_, resetGate_;
    long   curBeat_ = -1, lastOut_ = -1;
    uint64_t lastEdge_ = 0, gateOff_ = 0;
    bool   havePrev_ = false;
    double period_ = 0.0;
};

DROID_REGISTER_CIRCUIT(clocktool, Clocktool)

} // namespace droid
