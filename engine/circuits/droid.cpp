// droid — Global settings of the master. Spec: manual/circuits/droid.md.
// This circuit is a thin writer into EngineState::MasterSettings; the Engine
// consumes them (engine/src/engine.cpp: output conditioning post-stage and the
// clear/clearall broadcast — see the SPEC-GAP notes there and in signal.hpp).
//
// Emulated:
//   * maxslope1..8 / lpfilter1..8 — per-output smoothing, active only for
//     outputs whose jack is patched (the unpatched jack of an active pair uses
//     its table default). SPEC-GAP deviation: the hardware's always-on default
//     conditioning (0.25/0.25) is not emulated — engine outputs are
//     artifact-free floats and filtering them by default would change the
//     semantics of every patch.
//   * ledbrightness — stored as a render hint (Engine::ledBrightness()) for
//     the Rack adapter. L registers are NOT scaled: patches legitimately read
//     LED registers back as logic values (e.g. `select = L2.7`).
//   * clear / clearall — broadcast to every circuit whose own clear/clearall
//     input is unpatched (one-tick default-value pulse; Engine::tick).
//
// Accepted but inert in VCV (no hardware analog / maintenance-only):
// statusdump, calibrate, startcontrollerupgrade, startx7upgrade, uislowdown
// (the engine ticks every circuit every tick; the hardware's UI-rate
// optimization has no observable VCV equivalent), m4faderspeed,
// m4touchgain1..8, m4notchpower (motor-fader physics are not simulated).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <algorithm>

namespace droid {

class Droid : public Circuit {
public:
    void tick(EngineState& s) override {
        for (int k = 0; k < 8; k++) {
            bool cond = in("maxslope", k + 1).connected() || in("lpfilter", k + 1).connected();
            s.master.conditioned[k] = cond;
            if (!cond) continue;
            s.master.lpf[k] = std::min(std::max(in("lpfilter", k + 1).value(s), 0.0f), 1.0f);
            s.master.maxslope[k] = std::max(in("maxslope", k + 1).value(s), 0.0f);
        }
        s.master.ledBrightness = std::min(std::max(in("ledbrightness").value(s), 0.0f), 1.0f);
        if (risingEdge(clPrev_, in("clear").value(s))) s.master.requestClear = true;
        if (risingEdge(caPrev_, in("clearall").value(s))) s.master.requestClearAll = true;
        // remaining jacks: read-and-ignore (see header)
    }

private:
    bool clPrev_ = false, caPrev_ = false;
};

DROID_REGISTER_CIRCUIT(droid, Droid)

} // namespace droid
