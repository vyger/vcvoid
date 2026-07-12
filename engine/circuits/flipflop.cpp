// flipflop — Simple flip flop: stores one bit, manipulated by trigger inputs.
// Spec: manual/circuits/flipflop.md. Standard A*B+C input math applies to
// every input before we read it.
//
// Precedence when several triggers fire on the same tick: the manual
// documents each trigger's individual effect but never states which one
// wins if several rise in the same tick. SPEC-GAP: we apply effects in the
// order the manual's Inputs table lists them -- toggle, set, reset, clear,
// load -- so a later-listed trigger overrides an earlier one on a tie. See
// tests/golden/flipflop/simultaneous-trigger-precedence.gold for the
// convention spelled out with worked cases, and the ledger note for what
// hardware reference data would close this gap.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"

namespace droid {

class Flipflop : public Circuit {
public:
    void tick(EngineState& s) override {
        // "The flip flop starts its live with this value" (startvalue,
        // default 0): initialize lazily from startvalue on the very first
        // tick, since startvalue is itself a per-tick input (also read by
        // `clear`) rather than a load-time constant.
        if (!initialized_) {
            state_ = in("startvalue").value(s) >= kGateHighThreshold;
            initialized_ = true;
        }

        bool toggleFired = toggleGate_.risingEdge(in("toggle").value(s));
        bool setFired    = setGate_.risingEdge(in("set").value(s));
        bool resetFired  = resetGate_.risingEdge(in("reset").value(s));
        bool clearFired  = clearGate_.risingEdge(in("clear").value(s));
        bool loadFired   = loadGate_.risingEdge(in("load").value(s));

        if (toggleFired) state_ = !state_;
        if (setFired)    state_ = true;
        if (resetFired)  state_ = false;
        if (clearFired)  state_ = in("startvalue").value(s) >= kGateHighThreshold;
        if (loadFired)   state_ = in("loadvalue").value(s) >= kGateHighThreshold;

        out("output").set(s, state_ ? 1.0f : 0.0f);
    }

private:
    bool state_ = false;
    bool initialized_ = false;
    GateReader toggleGate_;
    GateReader setGate_;
    GateReader resetGate_;
    GateReader clearGate_;
    GateReader loadGate_;
};

DROID_REGISTER_CIRCUIT(flipflop, Flipflop)

} // namespace droid
