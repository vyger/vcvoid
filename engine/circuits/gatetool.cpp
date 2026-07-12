// gatetool — Operate on triggers and gates, modify gatelength. Spec:
// manual/circuits/gatetool.md. Three input styles (usually patch one):
//   * inputgate   — honors the input gate's high-time (relevant for stretch/min/max)
//   * inputtrigger— only the rising edge counts (input high-time ignored)
//   * inputedge   — every transition (rise AND fall) counts as an event
// For each event it drives three outputs: outputgate (a gate of controllable
// length), outputtrigger (a 10 ms trigger), and outputedge (toggles 0/1).
//
// Output gate length:
//   * gatelength connected -> fixed length = gatelength (seconds, or fractions of a
//     clock tick if taptempo is patched); input high-time and stretch/min/max ignored.
//   * otherwise -> base length = the input gate's measured high-time (0 for a
//     trigger/edge event), times (1 + gatestretch), clamped to
//     [mingatelength (default 0.01 s), maxgatelength (unbounded if unpatched)].
// Because we can't predict a gate's length at its rising edge, stretch and the min
// bound are applied when the input gate falls, while the max bound is enforced live
// during the high phase (closing the gate early). taptempo only sets the time
// reference; its period is learned from consecutive edges (0.5 s default until known).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>
#include <cstdint>
#include <limits>

namespace droid {

class Gatetool : public Circuit {
    static constexpr uint64_t kNever = std::numeric_limits<uint64_t>::max();
public:
    void tick(EngineState& s) override {
        const float thr = kGateHighThreshold;

        // taptempo period (engine ticks between rising edges)
        bool tapConnected = in("taptempo").connected();
        if (tapConnected && tapReader_.risingEdge(in("taptempo").value(s))) {
            if (haveTap_) tapPeriod_ = double(s.tick - lastTap_);
            lastTap_ = s.tick; haveTap_ = true;
        }
        double tapTicks = tapPeriod_ > 0.0 ? tapPeriod_ : 0.5 * (double)s.tickRateHz;

        // length parameters -> engine ticks. In taptempo mode a value is a fraction
        // of one clock tick; otherwise it is seconds.
        auto toTicks = [&](float v) -> double {
            return tapConnected ? (double)v * tapTicks : (double)v * (double)s.tickRateHz;
        };
        bool   glConnected = in("gatelength").connected();
        double glLen  = toTicks(in("gatelength").value(s));
        double stretch = (double)in("gatestretch").value(s);
        double minLen = toTicks(in("mingatelength").value(s));
        bool   maxConnected = in("maxgatelength").connected();
        double maxLen = toTicks(in("maxgatelength").value(s));
        auto clampLen = [&](double L) -> double {
            if (L < minLen) L = minLen;
            if (maxConnected && L > maxLen) L = maxLen;
            return L;
        };

        // ---- detect events -------------------------------------------------
        int events = 0;        // # of trigger/edge events this tick (outputtrigger/edge)
        bool gateStart = false;// an event that (re)starts the output gate
        bool deferLen = false; // the start came from inputgate (length unknown yet)

        if (in("inputtrigger").connected() &&
            trigReader_.risingEdge(in("inputtrigger").value(s))) {
            events++; gateStart = true; deferLen = false;
        }
        if (in("inputedge").connected()) {
            bool h = in("inputedge").value(s) >= thr;
            if (h != edgePrevHigh_) { edgePrevHigh_ = h; events++; gateStart = true; deferLen = false; }
        }
        if (in("inputgate").connected()) {
            bool h = in("inputgate").value(s) >= thr;
            if (h && !gatePrevHigh_) {              // rising edge -> event
                events++; gateStart = true; deferLen = !glConnected;
            } else if (!h && gatePrevHigh_ && igActive_) {   // falling edge -> finalize
                long D = (long)(s.tick - outRise_);
                double L = clampLen((double)D * (1.0 + stretch));
                outGateOff_ = outRise_ + (uint64_t)std::llround(L);
                igActive_ = false;
            }
            gatePrevHigh_ = h;
        }

        // ---- (re)start the output gate on an event -------------------------
        if (gateStart) {
            outRise_ = s.tick;
            if (glConnected) {
                double L = glLen < 1.0 ? 1.0 : glLen;
                outGateOff_ = s.tick + (uint64_t)std::llround(L);
                igActive_ = false;
            } else if (deferLen) {
                igActive_ = true;                    // length finalized when input falls
                outGateOff_ = maxConnected ? s.tick + (uint64_t)std::llround(maxLen) : kNever;
            } else {                                 // trigger/edge: zero-length base
                double L = clampLen(0.0);
                outGateOff_ = s.tick + (uint64_t)std::llround(L);
                igActive_ = false;
            }
            trigOff_ = s.tick + (uint64_t)std::max(1L, std::lround(0.01 * (double)s.tickRateHz));
            if (events & 1) edgeState_ = !edgeState_;
        }

        // ---- enforce max bound live while an input gate is held ------------
        if (igActive_ && maxConnected &&
            (long)(s.tick - outRise_) >= std::lround(maxLen)) {
            outGateOff_ = s.tick;                    // close now; latch until next event
            igActive_ = false;
        }

        out("outputgate").set(s, s.tick < outGateOff_ ? 1.0f : 0.0f);
        out("outputtrigger").set(s, s.tick < trigOff_ ? 1.0f : 0.0f);
        out("outputedge").set(s, edgeState_ ? 1.0f : 0.0f);
    }

private:
    GateReader tapReader_, trigReader_;
    bool edgePrevHigh_ = false, gatePrevHigh_ = false;
    bool haveTap_ = false, igActive_ = false, edgeState_ = false;
    uint64_t lastTap_ = 0, outRise_ = 0, outGateOff_ = 0, trigOff_ = 0;
    double tapPeriod_ = 0.0;
};

DROID_REGISTER_CIRCUIT(gatetool, Gatetool)

} // namespace droid
