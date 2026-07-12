// select — Copy a signal if selected. Spec: manual/circuits/select.md.
//
// Copies `input` to `output` ONLY while the circuit is selected; when not
// selected it leaves the output register UNTOUCHED (never calls set), so other
// circuits (or a previous value) keep it. This is the building block for UI
// overlays — an LED/output that reflects one "menu page" without a deselected
// page forcing it to 0.
//
// Selection rule:
//   * `selectat` patched  -> selected iff round(select) == round(selectat)
//     (exact integer match; e.g. selectat 0 selects when select is exactly 0).
//   * else `select` patched -> selected iff select reads a positive gate
//     (>= the shared gate-high threshold, 0.1 == 1 V) — the manual's "positive
//     gate signal".
//   * else (neither patched) -> smart default: always selected (acts like copy).
//
// The engine never clears output registers between ticks, so "leave untouched"
// is simply "do not set()".
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>

namespace droid {

class Select : public Circuit {
public:
    void tick(EngineState& s) override {
        bool selected;
        if (in("selectat").connected()) {
            long sel = std::lround(in("select").value(s));
            long at  = std::lround(in("selectat").value(s));
            selected = (sel == at);
        } else if (in("select").connected()) {
            selected = in("select").value(s) >= kGateHighThreshold;
        } else {
            selected = true;                       // smart default: always on
        }

        if (selected)
            out("output").set(s, in("input").value(s));
        // else: leave the output register untouched
    }
};

DROID_REGISTER_CIRCUIT(select, Select)

} // namespace droid
