// cvlooper — Clocked CV looper. Spec: manual/circuits/cvlooper.md. Records
// cvin/gatein to a circular 8-second tape (one sample per engine tick here)
// while the loop is off and bypass is low; the tape head FREEZES during loop
// playback and bypass, since the manual says looped input is ignored and bypass
// "keeps the loop's content untouched". When the loop switch turns on it
// replays the last `length` clock ticks from the tape (window clamped to the
// tape capacity). reset restarts playback; pause holds; overlay routes the live
// input over the loop while gatein is high; overdub writes the live input into
// the tape at the play head. CV is recorded only while gatein is high; the gate
// state is always recorded.
//
// SPEC-GAP: the manual's fixed 1-sample-per-ms tape and tapespeed RESAMPLING are
// simplified to per-engine-tick sampling (tapespeed scales the effective loop
// length but is not a true resampler); the loop switch engages at the exact
// switch tick (not the "nearest, past or future" clock quantization). These need
// hardware timing fixtures to verify.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>
#include <vector>

namespace droid {

class Cvlooper : public Circuit {
    static constexpr double kTapeSeconds = 8.0;   // manual: 8 s tape

public:
    void tick(EngineState& s) override {
        if (cvTape_.empty()) {          // size the tape from the tick rate
            cap_ = (long)std::llround(kTapeSeconds * s.tickRateHz);
            if (cap_ < 1) cap_ = 1;
            cvTape_.assign(cap_, 0.0f);
            gateTape_.assign(cap_, 0);
        }
        float cvin = in("cvin").value(s);
        bool  gatein = in("gatein").value(s) >= kGateHighThreshold;
        long  length = std::lround(in("length").value(s));
        if (length < 1) length = 1; if (length > 128) length = 128;
        float tapespeed = in("tapespeed").value(s);
        if (tapespeed <= 0.0f) tapespeed = 1.0f;
        bool loopOn  = in("loopswitch").value(s) >= kGateHighThreshold;
        bool pause   = in("pause").value(s) >= kGateHighThreshold;
        bool overlay = in("overlay").value(s) >= kGateHighThreshold;
        bool overdub = in("overdub").value(s) >= kGateHighThreshold;
        bool bypass  = in("bypass").value(s) >= kGateHighThreshold;

        if (clockGate_.risingEdge(in("clock").value(s))) {
            if (haveClock_) period_ = double(s.tick - lastClock_);
            lastClock_ = s.tick; haveClock_ = true;
        }
        bool resetEdge = resetGate_.risingEdge(in("reset").value(s));

        // Loop switch transitions first: the engage tick already belongs to
        // playback (bypass disables looping without touching the tape).
        if (loopOn && !loopActive_ && !bypass) {
            loopAnchor_ = writePos_;     // window ends here (SPEC-GAP: no future snap)
            loopPhase_ = 0;
            loopActive_ = true;
        }
        if (!loopOn) loopActive_ = false;
        if (resetEdge && loopActive_) loopPhase_ = 0;

        // Record only while the loop is off and bypass is low; during playback
        // and bypass the tape head freezes so the loop's content is untouched.
        // (CV recorded only while the gate is high; gate state always recorded.)
        bool recording = !loopActive_ && !bypass;
        if (recording) {
            int w = (int)(writePos_ % cap_);
            if (gatein) cvTape_[w] = cvin;
            gateTape_[w] = gatein ? 1 : 0;
        }

        double per = period_ > 0.0 ? period_ : 1.0;
        long loopLen = (long)std::llround(length * per / tapespeed);
        if (loopLen < 1) loopLen = 1;
        if (loopLen > cap_) loopLen = cap_;   // window can't exceed the tape

        float cvOut; int gateOut;
        if (loopActive_ && !bypass) {
            if (overlay && gatein) {           // overlay: live input over the loop
                cvOut = cvin; gateOut = 1;
            } else {
                long readIdx = loopAnchor_ - loopLen + loopPhase_;
                int ri = (int)(((readIdx % cap_) + cap_) % cap_);
                if (overdub && gatein) {       // overdub: write live input into tape
                    cvTape_[ri] = cvin; gateTape_[ri] = 1;
                    cvOut = cvin; gateOut = 1;
                } else {
                    cvOut = cvTape_[ri]; gateOut = gateTape_[ri];
                }
            }
            if (!pause) { loopPhase_++; if (loopPhase_ >= loopLen) loopPhase_ = 0; }
        } else {
            cvOut = cvin; gateOut = gatein ? 1 : 0;    // bypass / loop off
        }

        if (recording) writePos_++;
        out("cvout").set(s, cvOut);
        out("gateout").set(s, gateOut ? 1.0f : 0.0f);
    }

private:
    std::vector<float> cvTape_;
    std::vector<char>  gateTape_;
    long cap_ = 0;
    long long writePos_ = 0, loopAnchor_ = 0;
    long loopPhase_ = 0;
    bool loopActive_ = false, haveClock_ = false;
    uint64_t lastClock_ = 0;
    double period_ = 0.0;
    GateReader clockGate_, resetGate_;
};

DROID_REGISTER_CIRCUIT(cvlooper, Cvlooper)

} // namespace droid
