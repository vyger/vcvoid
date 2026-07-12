// clockedtrigger — Delay a trigger until the next clock tick.
// Spec: manual/circuits/clockedtrigger.md. A trigger is stored pending until the
// next clock edge, then emitted once at `clockedtrigger`; the `value` input is
// frozen at trigger time and released to `clockedvalue` on the clock. `clear`
// forgets a pending trigger and zeroes clockedvalue. Standard A*B+C input math.
//
// SPEC-GAP: only the trigger+clock same-cycle case is documented (immediate
// pass-through). We order edges within a tick as trigger, then clear, then
// clock -- which produces that pass-through -- and leave the precedence of
// clear vs the others as this literal order.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"

namespace droid {

class ClockedTrigger : public Circuit {
public:
    void tick(EngineState& s) override {
        bool trigFired  = trigGate_.risingEdge(in("trigger").value(s));
        bool clearFired = clearGate_.risingEdge(in("clear").value(s));
        bool clockFired = clockGate_.risingEdge(in("clock").value(s));

        if (trigFired) {
            pending_ = true;
            pendingValue_ = in("value").value(s);   // freeze at trigger time
        }
        if (clearFired) {
            pending_ = false;
            pendingValue_ = 0.0f;
            clockedValue_ = 0.0f;
        }
        bool emit = false;
        if (clockFired && pending_) {
            emit = true;
            clockedValue_ = pendingValue_;
            pending_ = false;
        }

        out("clockedtrigger").set(s, emit ? 1.0f : 0.0f);
        out("clockedvalue").set(s, clockedValue_);
        out("pendingtrigger").set(s, pending_ ? 1.0f : 0.0f);
        out("pendingvalue").set(s, pendingValue_);
    }

private:
    GateReader trigGate_, clearGate_, clockGate_;
    bool  pending_ = false;
    float pendingValue_ = 0.0f;   // frozen value awaiting release
    float clockedValue_ = 0.0f;   // last released value (persists)
};

DROID_REGISTER_CIRCUIT(clockedtrigger, ClockedTrigger)

} // namespace droid
