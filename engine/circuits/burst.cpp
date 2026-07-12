// burst — Generate a burst of pulses. Spec: manual/circuits/burst.md.
// A trigger fires `count` pulses at a rate set by hz / rate / taptempo, each
// 50%-duty (gate width = half the period). `skip` delays the start by that many
// periods; `reset` stops an ongoing burst; a new trigger restarts from the
// beginning. Shares lfo's 1V/oct `rate` scheme. Standard A*B+C input math.
//
// Phase convention (SPEC-GAP, analogous to lfo): the first pulse is emitted on
// the trigger tick itself (phase 0 sampled at the trigger tick), then phase
// advances by 1/P per tick. A pulse fires in each cycle in [skip, skip+count-1]
// during the first half of that cycle.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include "../src/taptempo.hpp"
#include <cmath>

namespace droid {

class Burst : public Circuit {
public:
    void tick(EngineState& s) override {
        // --- edge handling: reset first, then trigger, so a simultaneous
        // trigger+reset starts a fresh burst (SPEC-GAP: precedence undocumented).
        bool resetFired   = resetGate_.risingEdge(in("reset").value(s));
        bool triggerFired = trigGate_.risingEdge(in("trigger").value(s));

        // taptempo edge measurement (always tracked so the interval is current)
        bool tapConnected = in("taptempo").connected();
        if (tapConnected && tapGate_.risingEdge(in("taptempo").value(s)))
            tap_.record(s.tick, s.tickRateHz);

        if (resetFired) active_ = false;
        if (triggerFired) {
            phase_ = 0.0;
            active_ = true;
            // Latch the burst structure at start time.
            count_ = (long)std::lround(in("count").value(s));
            skip_  = (long)std::lround(in("skip").value(s));
            if (count_ < 0) count_ = 0;
            if (skip_  < 0) skip_  = 0;
        }

        // --- frequency ---
        // rate is 1V/oct: 1 V (0.1 units) doubles the rate. hz is exclusive to
        // taptempo (manual) but rate applies in both cases.
        float octMult = std::pow(2.0f, in("rate").value(s) * 10.0f);
        float freq = tapConnected ? (1.0f / tap_.interval) * octMult
                                   : in("hz").value(s) * octMult;

        bool high = false;
        if (active_ && count_ > 0 && freq > 0.0f) {
            // period in ticks, clamped so the output rate never exceeds the
            // hardware maximum (SPEC-GAP: exact max unknown; 2 ticks = one high
            // + one low guarantees no pulse is lost). See rate-limit-fast.gold.
            float P = s.tickRateHz / freq;
            if (P < 2.0f) P = 2.0f;

            long cycle = (long)std::floor(phase_);
            double frac = phase_ - (double)cycle;
            if (cycle >= skip_ && cycle < skip_ + count_ && frac < 0.5)
                high = true;

            // advance; deactivate once all cycles are done
            phase_ += 1.0 / (double)P;
            if (phase_ >= (double)(skip_ + count_))
                active_ = false;
        }

        out("output").set(s, high ? 1.0f : 0.0f);
    }

private:
    GateReader trigGate_;
    GateReader resetGate_;
    GateReader tapGate_;
    bool   active_ = false;
    double phase_ = 0.0;      // cycles since burst start
    long   count_ = 1;
    long   skip_  = 0;
    TapTempo tap_;   // basics.md 3.3 estimator, identical scheme to lfo (taptempo.hpp)
};

DROID_REGISTER_CIRCUIT(burst, Burst)

} // namespace droid
