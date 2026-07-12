// togglebutton — Obsolete two-state on/off latch. Spec:
// manual/circuits/togglebutton.md. Superseded by `button`; the DROID Forge flags
// it as deprecated (an accepted crosscheck divergence).
//
// A rising edge of `button` toggles the boolean state; `reset` forces it off.
// led is 1/0, output is onvalue/offvalue, inverted swaps them, negated is 1 iff
// off. `doubleclickmode` makes a lone press momentarily invert the state while
// held and only a double press toggles the latched state.
//
// Conventions / SPEC-GAPs:
//   * blue-7 firmware jacks only: the manual's "up to four layers" feature
//     (`switch`, `output1..4`) is NOT part of the blue-7 jack table (the Forge
//     rejects those params), so this circuit is two-state only. Legacy manual
//     text describing layers does not apply to this firmware.
//   * double-click window is UNSPECIFIED by the manual -> 0.5 s (same choice as
//     `button`). A second rising edge within the window toggles; a lone press
//     momentarily inverts the persisted state while held.
//   * startvalue (gate): when patched it forces the initial state and disables
//     SD persistence. Persistence is a headless no-op regardless; the state
//     seeds to startvalue-or-off at load.
//   * simultaneous button+reset on one tick: `button` is applied first, then
//     `reset` (Inputs-table order), so reset wins and the state ends off.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>

namespace droid {

class ToggleButton : public Circuit {
    static constexpr float kDoubleClickSeconds = 0.5f; // SPEC-GAP

public:
    void tick(EngineState& s) override {
        if (!inited_) {
            state_ = in("startvalue").connected() &&
                     in("startvalue").value(s) >= kGateHighThreshold;
            inited_ = true;
        }

        bool doubleClick = in("doubleclickmode").value(s) >= kGateHighThreshold;
        long dclickTicks = std::lround(kDoubleClickSeconds * s.tickRateHz);
        if (dclickTicks < 1) dclickTicks = 1;

        bool nowHigh = in("button").value(s) >= kGateHighThreshold;
        bool rise = nowHigh && !prevHigh_;
        prevHigh_ = nowHigh;
        bool resetNow = in("reset").value(s) >= kGateHighThreshold;
        bool resetRise = resetNow && !resetPrev_;
        resetPrev_ = resetNow;

        if (rise) {
            if (doubleClick) {
                if (recentPress_ && sincePress_ <= dclickTicks) {
                    state_ = !state_;
                    recentPress_ = false;
                } else {
                    recentPress_ = true; sincePress_ = 0;
                }
            } else {
                state_ = !state_;
            }
        }
        if (resetRise) state_ = false;   // reset wins on a tie
        if (recentPress_) {
            sincePress_++;
            if (sincePress_ > dclickTicks) recentPress_ = false;
        }

        // Effective state: momentary inversion while held in double-click mode.
        bool E = (doubleClick && nowHigh) ? !state_ : state_;

        float onv = in("onvalue").value(s), offv = in("offvalue").value(s);
        out("led").set(s, E ? 1.0f : 0.0f);
        out("output").set(s, E ? onv : offv);
        out("inverted").set(s, E ? offv : onv);
        out("negated").set(s, E ? 0.0f : 1.0f);
    }

private:
    bool inited_ = false;
    bool state_ = false;
    bool prevHigh_ = false;
    bool resetPrev_ = false;
    bool recentPress_ = false;
    long sincePress_ = 0;
};

DROID_REGISTER_CIRCUIT(togglebutton, ToggleButton)

} // namespace droid
