// fourstatebutton — Obsolete button that cycles through four states. Spec:
// manual/circuits/fourstatebutton.md. Superseded by `button`; the Forge flags it
// deprecated (an accepted crosscheck divergence).
//
// Each `button` rising edge advances the state 0 -> 1 -> 2 -> 3 -> 0. `output`
// copies value1..value4 for the current state; `led` shows state/3 (off in
// state 0, full in state 3). `reset` returns to state 0.
//
// Conventions / SPEC-GAPs:
//   * value1..value4 have no numeric defaults in the blue-7 jack table (unlike
//     `button`), so an unpatched value reads 0.
//   * startvalue (integer 0..3): when patched it sets the initial state and
//     disables SD persistence. Persistence is a headless no-op regardless; the
//     state seeds to startvalue-or-0 at load.
//   * simultaneous button+reset on one tick: `button` is applied first, then
//     `reset` (Inputs-table order), so reset wins and the state ends at 0.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>

namespace droid {

class FourStateButton : public Circuit {
public:
    void tick(EngineState& s) override {
        if (!inited_) {
            long v = std::lround(in("startvalue").value(s));
            if (v < 0) v = 0; else if (v > 3) v = 3;
            state_ = (int)v;
            inited_ = true;
        }

        bool nowHigh = in("button").value(s) >= kGateHighThreshold;
        bool rise = nowHigh && !prevHigh_;
        prevHigh_ = nowHigh;
        bool resetNow = in("reset").value(s) >= kGateHighThreshold;
        bool resetRise = resetNow && !resetPrev_;
        resetPrev_ = resetNow;

        if (rise) state_ = (state_ + 1) % 4;
        if (resetRise) state_ = 0;   // reset wins on a tie

        out("output").set(s, in("value", state_ + 1).value(s));
        out("led").set(s, float(state_) / 3.0f);
    }

private:
    bool inited_ = false;
    int  state_ = 0;
    bool prevHigh_ = false;
    bool resetPrev_ = false;
};

DROID_REGISTER_CIRCUIT(fourstatebutton, FourStateButton)

} // namespace droid
