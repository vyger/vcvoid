// motorfader — one virtual value on an M4 motor fader. Spec:
// manual/circuits/motorfader.md. The value/notch engine lives in
// engine/src/fadercore.hpp (shared with faderbank / fadermatrix); this file adds
// the motorfader-only orchestration: startvalue seeding, select/selectat overlay
// with pickup-free motorized takeover, 8 presets, outputscale/offset, the touch
// `button` output, ledvalue (incl. the negative magic mode), and sharewithnext.
//
// Fader handle: `fader` is a 1-based GLOBAL fader number (motorfader.md: "All
// faders are simply enumerated"), resolved once at load. An out-of-range fader
// leaves the circuit inert on the hardware side (no motor command, button 0) but
// its output math still runs off the internal value.
//
// The fader value is bidirectional (see controllerstate.hpp FaderState):
//   * While SELECTED, this circuit owns the fader. It reads the user's position
//     from ControllerState, snaps it through the notch config, stores it as the
//     virtual value and commands the motor to hold that (snapped) position.
//   * On the tick it BECOMES selected — or when a preset/clear moved the virtual
//     value — it instead drives the motor to the stored value (pickup-free
//     motorized takeover: unlike a pot, the motor physically moves to show the
//     value, so no pickup tracking is needed; motorfader.md "the motor faders
//     act as a display for showing you the current values").
//   * While DESELECTED it freezes the value and does NOT touch the fader, but
//     still emits `output` (motorfader.md: "even if the circuit is currently not
//     selected, it will nevertheless ... process ... its outputs").
//
// Conventions / SPEC-GAPs (manual silent, qualitative, or hardware-only):
//   * notch value/position mapping and pitch-bend auto-return — see fadercore.hpp.
//     motorfader.md caps notches at 25; the value quantization uses the raw
//     count regardless (the >25 "force feedback off" note is haptic-only).
//   * motor speed is instant (fadercore.hpp / controllerstate.hpp) — no spec.
//   * ledvalue / ledcolor drive the LED below the fader: panel-only, like pot's
//     LED gauge and the encoder ring — NOT golden-observable. The negative-value
//     "magic mode" (full brightness when the setting matches startvalue, else
//     |ledvalue|) is computed into FaderState.led for the future Rack adapter but
//     is not readable headless; verification is requires-human anyway.
//   * touch `button`: select-gated (0 while deselected), reads the fader's touch
//     plate from ControllerState (set by the harness `touch` verb / Rack).
//   * sharewithnext: the circuit suppresses its own `output` and hands its value
//     to the NEXT motorfader circuit. Exactly one of a shared group is selected
//     at a time (manual); the group's single output (on the last, non-sharing
//     circuit) tracks whichever member is currently selected. Resolved once at
//     load; the value passed downstream is the fully-scaled output (mixing
//     different outputscale across a shared group is unsupported, matching the
//     manual's "same number of notches" caveat).
//   * presets: 8 slots holding the raw settled position; three standard modes
//     (triggered save/load, immediate preset switch, value-carrying trigger);
//     precedence clearall > clear > savepreset > loadpreset. SD persistence is a
//     headless no-op; state/presets seed to startvalue at load.
//   * display / header (DB8E) are not modelled (existing convention).
#include "../src/registry.hpp"
#include "../src/uihelpers.hpp"
#include "../src/fadercore.hpp"
#include <cmath>
#include <string>

namespace droid {

namespace fc = fadercore;

class Motorfader : public Circuit {
    static constexpr int kPresets = 8;

public:
    void tick(EngineState& s) override {
        if (!inited_) init(s);
        int notches = readNotches(s);
        bool selected = ui::isSelected(*this, s);
        FaderState* f = s.controllers.fader(faderIdx_);
        bool touched = f ? f->touched : false;

        // --- clear / presets (always run) -----------------------------------
        bool recall = handlePresets(s, notches);

        // --- value update & motor command -----------------------------------
        if (f && selected) {
            bool takeover = !wasSelected_ || recall;
            float src = takeover ? value_ : f->position;
            fc::Result r = fc::evaluate(src, notches, touched);
            value_ = r.position;
            s.controllers.commandFader(faderIdx_, r.position);
            f->notches = notches;                        // haptic feel (panel)
            f->led = ledBrightness(s, notches);          // panel-only
            f->ledColor = in("ledcolor").value(s);       // panel-only
        }
        // else: deselected / no fader -> value_ frozen, fader left alone.
        wasSelected_ = selected;

        float emit = fc::outputOf(value_, notches) * in("outputscale").value(s) +
                     in("outputoffset").value(s);

        // --- output (with sharewithnext coupling) ---------------------------
        if (shareTarget_) {
            // Feeder: don't drive our own output; forward the active value down
            // the shared group (selected member wins; otherwise pass an active
            // upstream through).
            if (selected)               { shareTarget_->pushShared(emit); }
            else if (sharedActive_)     { shareTarget_->pushShared(sharedEmit_); }
            sharedActive_ = false;
        } else {
            // Output owner: an active upstream feeder (ticked earlier this tick)
            // overrides our own value; otherwise emit our own.
            float outVal = (sharedActive_ && !selected) ? sharedEmit_ : emit;
            sharedActive_ = false;
            if (!shareSuppressOutput_) out("output").set(s, outVal);
        }

        // --- touch button (select-gated; the plate below the fader, not the
        // fader hold) ---------------------------------------------------------
        out("button").set(s, (selected && f && f->plate) ? 1.0f : 0.0f);
    }

    // Called by an upstream feeder (earlier in patch order) before this circuit
    // ticks: hands the currently-active shared value down the group.
    void pushShared(float v) { sharedEmit_ = v; sharedActive_ = true; }

    // Persisted: the settled fader value + all 8 presets + current preset slot.
    int stateVersion() const override { return 1; }
    void saveState(StateWriter& w) const override {
        w.f(value_);
        for (int p = 0; p < kPresets; p++) w.f(preset_[p]);
        w.n(prevPreset_);
    }
    void loadState(EngineState& s, int version, const std::vector<double>& in) override {
        if (version != 1 || in.size() != (size_t)(kPresets + 2)) return;
        if (!inited_) init(s);
        StateReader r{in};
        value_ = (float)r.f();
        for (int p = 0; p < kPresets; p++) preset_[p] = (float)r.f();
        prevPreset_ = (int)r.n();
        wasSelected_ = false;   // force motorized takeover to the loaded value
    }

private:
    void init(EngineState& s) {
        // fader handle (bare global number; default 1).
        long g = std::lround(in("fader").value(s));
        faderIdx_ = s.controllers.validateFader((int)g) ? (int)g : 0;

        int notches = readNotches(s);
        value_ = fc::seed(in("startvalue").value(s), notches);
        for (int p = 0; p < kPresets; p++) preset_[p] = value_;
        prevPreset_ = ui::clampPreset(std::lround(in("preset").value(s)), kPresets - 1);

        // sharewithnext: resolve the downstream motorfader once (static config).
        if (in("sharewithnext").value(s) >= kGateHighThreshold) {
            shareSuppressOutput_ = true;
            Circuit* nxt = nextPeer();
            if (nxt && nxt->def && std::string(nxt->def->name) == "motorfader")
                shareTarget_ = static_cast<Motorfader*>(nxt);
        }
        inited_ = true;
    }

    int readNotches(EngineState& s) {
        long n = std::lround(in("notches").value(s));
        return n < 0 ? 0 : (int)n;
    }

    // LED brightness below the fader (panel-only). Negative ledvalue = magic
    // mode: full brightness when the setting matches startvalue, else |ledvalue|.
    float ledBrightness(EngineState& s, int notches) {
        if (!in("ledvalue").connected()) return fc::outputOf(value_, notches);
        float lv = in("ledvalue").value(s);
        if (lv >= 0.0f) return lv;
        float start = fc::seed(in("startvalue").value(s), notches);
        return (std::fabs(value_ - start) < 1e-6f) ? 1.0f : -lv;
    }

    bool handlePresets(EngineState& s, int notches) {
        bool clearAll = risingEdge(caPrev_, in("clearall").value(s));
        bool clr      = risingEdge(clPrev_, in("clear").value(s));
        bool loadPatched   = in("loadpreset").connected();
        bool savePatched   = in("savepreset").connected();
        bool presetPatched = in("preset").connected();
        bool immediate = presetPatched && !loadPatched && !savePatched;
        bool recall = false;

        if (clearAll) {
            value_ = fc::seed(in("startvalue").value(s), notches);
            for (int p = 0; p < kPresets; p++) preset_[p] = value_;
            recall = true;
        } else if (clr) {
            value_ = fc::seed(in("startvalue").value(s), notches);
            if (immediate) preset_[prevPreset_] = value_;
            recall = true;
        }
        bool saveTrig = risingEdge(spPrev_, in("savepreset").value(s));
        bool loadTrig = risingEdge(lpPrev_, in("loadpreset").value(s));
        if (savePatched && saveTrig)
            preset_[ui::presetNum(*this, s, in("savepreset").value(s), presetPatched, kPresets - 1)] = value_;
        if (loadPatched && loadTrig) {
            value_ = preset_[ui::presetNum(*this, s, in("loadpreset").value(s), presetPatched, kPresets - 1)];
            recall = true;
        }
        if (immediate) {
            int cur = ui::clampPreset(std::lround(in("preset").value(s)), kPresets - 1);
            if (cur != prevPreset_) {
                preset_[prevPreset_] = value_;
                value_ = preset_[cur];
                prevPreset_ = cur;
                recall = true;
            }
        }
        return recall;
    }

    bool  inited_ = false;
    int   faderIdx_ = 0;
    float value_ = 0.0f;
    float preset_[kPresets] = {};
    int   prevPreset_ = 0;
    bool  wasSelected_ = false;
    // sharewithnext coupling
    bool  shareSuppressOutput_ = false;
    Motorfader* shareTarget_ = nullptr;
    float sharedEmit_ = 0.0f;
    bool  sharedActive_ = false;
    bool  caPrev_ = false, clPrev_ = false, spPrev_ = false, lpPrev_ = false;
};

DROID_REGISTER_CIRCUIT(motorfader, Motorfader)

} // namespace droid
