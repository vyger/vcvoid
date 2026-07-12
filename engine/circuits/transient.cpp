// transient — Transient generator: a linear ramp from start to end over a
// duration given in seconds (or clock ticks if `clock` is patched). Spec:
// manual/circuits/transient.md. The slope is recomputed each tick as
// (end - output) / remaining_time, which keeps the output smooth across in-flight
// changes to start/end/duration and always arrives at end when phase reaches 1.
// loop/pingpong/freeze/reset per the manual. Standard A*B+C input math applies.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include "../src/taptempo.hpp"

namespace droid {

class Transient : public Circuit {
public:
    void tick(EngineState& s) override {
        float start = in("start").value(s);
        float end   = in("end").value(s);
        float dur   = in("duration").value(s);
        if (dur < 0.0f) dur = 0.0f;                 // negative -> 0
        bool loop     = in("loop").value(s)     >= kGateHighThreshold;
        bool pingpong = in("pingpong").value(s) >= kGateHighThreshold;
        bool freeze   = in("freeze").value(s)   >= kGateHighThreshold;
        bool clockMode = in("clock").connected();

        if (clockMode && clockGate_.risingEdge(in("clock").value(s)))
            tap_.record(s.tick, s.tickRateHz);

        // `start` is only read when the transient (re)starts: on load, on reset,
        // or on a loop restart.
        bool doStart = !initialized_ || resetGate_.risingEdge(in("reset").value(s));
        if (doStart) {
            output_ = start;
            phase_ = 0.0;
            dir_ = +1;
            endReached_ = false;
            initialized_ = true;
        }

        float durSec = clockMode ? dur * tap_.interval : dur;
        float to = (dir_ > 0) ? end : start;        // pingpong swaps the target
        float eotPulse = 0.0f;

        if (!freeze) {
            if (durSec <= 0.0f) {
                if (phase_ < 1.0) { output_ = to; phase_ = 1.0; legComplete(start, loop, pingpong, eotPulse); }
                else output_ = to;
            } else if (!doStart && phase_ < 1.0) {
                double dt = s.secondsPerTick();
                double remaining = (1.0 - phase_) * durSec;   // > 0 since phase < 1
                output_ += ((double)to - output_) / remaining * dt;
                phase_ += dt / durSec;
                if (phase_ >= 1.0) { phase_ = 1.0; output_ = to; legComplete(start, loop, pingpong, eotPulse); }
            }
        }

        out("output").set(s, (float)output_);
        // phase is the internal clock ("measures which part of the transient
        // has been run through already"), presented as if start=0 and end=1 —
        // NOT the normalized output position, which would jump or stall on
        // in-flight `end` changes. On the pingpong return leg start/end count
        // as exchanged, so it runs back 1 -> 0.
        float ph = (dir_ > 0) ? (float)phase_ : (float)(1.0 - phase_);
        out("phase").set(s, ph);
        out("endoftransient").set(s, endReached_ ? 1.0f : eotPulse);
    }

private:
    // Called when a leg finishes (phase reached 1). Handles pingpong/loop/end.
    void legComplete(float start, bool loop, bool pingpong, float& eotPulse) {
        if (pingpong) {
            if (dir_ > 0) { dir_ = -1; phase_ = 0.0; }          // turn back, no eot
            else { dir_ = +1; phase_ = 0.0; eotPulse = 1.0f; }  // full cycle -> eot
        } else if (loop) {
            output_ = start; phase_ = 0.0; dir_ = +1; eotPulse = 1.0f;
        } else {
            endReached_ = true;                                 // sustain high
        }
    }

    GateReader clockGate_, resetGate_;
    bool initialized_ = false, endReached_ = false;
    int dir_ = +1;
    double phase_ = 0.0, output_ = 0.0;
    TapTempo tap_;   // basics.md 3.3 estimator (shared, see taptempo.hpp)
};

DROID_REGISTER_CIRCUIT(transient, Transient)

} // namespace droid
