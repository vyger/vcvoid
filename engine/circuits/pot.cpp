// pot — Swiss-army UI helper for controller pots. Spec: manual/circuits/pot.md.
//
// Wraps a physical pot (`pot`, e.g. P1.1) with output scaling/offset, a center
// notch, a slope (resolution) curve, discrete (stepped) selection, six
// "hemisphere" outputs, and — when overlaid/preset/started — a *virtual* pot
// with the documented "pickup" tracking. Supersedes notchedpot and switchedpot.
//
// Value pipeline (continuous mode): the 0..1 source value V (physical position,
// or the virtual value in virtual mode) -> slope (V^slope) -> notch -> the main
// `output` = shaped*outputscale + outputoffset. The hemisphere outputs
// (bipolar/absbipolar/left|righthalf[inv]) are derived from the same shaped
// value scaled by outputscale ONLY (outputoffset is main-output-only, per the
// Inputs table). `onchange` pulses whenever the shaped value moves.
//
// Discrete mode (discrete >= 1): the source picks one of `discrete` integer
// values 0..discrete-1 (output = idx*outputscale + outputoffset). notch/slope
// are ignored and all hemisphere outputs are dead (0), per the Inputs table.
//
// Virtual mode is entered when any of select/selectat, preset/loadpreset/
// savepreset, or clear is connected (pot.md "Virtual pots"). In virtual mode
// the physical position no longer maps directly; instead the virtual value is
// nudged by potshape::pickup and only while this circuit is *selected*. A
// deselected virtual pot freezes its value but keeps emitting all outputs
// (pot.md: "even if the circuit is currently not selected, it will nevertheless
// work and process ... its outputs"). 16 presets in the three standard modes.
//
// Conventions / SPEC-GAPs (manual silent, prose-only, or hardware-visual):
//   * LED gauge: NOT modelled. It drives the MASTER's 16-LED matrix / a B32 /
//     the DB8E, none of which the headless engine renders. `ledgauge` is read
//     only to keep the jack live; its color/enable and the exact-0.5 inner-LED
//     indication are a visual detail (frontmatter: human spot-check). Likewise
//     display/header (DB8E) are not modelled.
//   * UI slowdown: pot.md notes UI circuits run at 12.5% (every 8th cycle). We
//     run every tick for deterministic goldens; the slowdown is a scheduling
//     optimization, not a behavioral spec, so bypassing it changes only update
//     latency, not values.
//   * slope vs notch order: the manual specifies each in isolation but not their
//     interaction. We apply slope first (V^slope) then the notch, so the flat
//     notch zone lands on the post-slope value and the center stays exactly 0.5.
//     (slope has no effect in discrete mode, where both are ignored.)
//   * pickup granularity: the algorithm is defined for continuous motion; a
//     golden `set P..` jump is applied in a single tick using the pre-jump
//     virtual/physical snapshot, which makes both reach the shared endpoint
//     together (the documented invariant). prevPhys is tracked every tick even
//     while deselected, so re-selecting a pot never causes a phantom jump.
//   * discrete + virtual: startvalue is a discrete band index; clear seeds the
//     virtual value to that band's center (band+0.5)/discrete so floor() lands
//     back on it. Presets store the raw 0..1 virtual value.
//   * onchange: a 10 ms trigger emitted when the shaped 0..1 value (or discrete
//     index) changes between ticks. No pulse on the very first tick.
//   * preset precedence on a tied tick: clearall > clear > savepreset >
//     loadpreset (same as the button family). SD persistence (dontsave / boot
//     load) is a headless no-op; state/presets seed to the start value at load.
#include "../src/registry.hpp"
#include "../src/uihelpers.hpp"
#include "../src/potshape.hpp"
#include <cmath>

namespace droid {

class Pot : public Circuit {
public:
    void tick(EngineState& s) override {
        if (!inited_) init(s);

        int disc = discreteCount(s);          // 0 == continuous
        bool virt = virtualMode();
        bool selected = ui::isSelected(*this, s);
        float scale = in("outputscale").value(s);
        float offset = in("outputoffset").value(s);
        long trig = std::lround(0.01 * s.tickRateHz);
        if (trig < 1) trig = 1;

        // --- clear / clearall / presets (always) ----------------------------
        bool clearAll = risingEdge(caPrev_, in("clearall").value(s));
        bool clr      = risingEdge(clPrev_, in("clear").value(s));
        bool loadPatched   = in("loadpreset").connected();
        bool savePatched   = in("savepreset").connected();
        bool presetPatched = in("preset").connected();
        bool immediate = presetPatched && !loadPatched && !savePatched;

        if (clearAll) {
            virtualPos_ = startPos(s, disc);
            for (int p = 0; p < 16; p++) preset_[p] = virtualPos_;
        } else if (clr) {
            virtualPos_ = startPos(s, disc);
            if (immediate) preset_[prevPreset_] = virtualPos_;
        }
        bool saveTrig = risingEdge(spPrev_, in("savepreset").value(s));
        bool loadTrig = risingEdge(lpPrev_, in("loadpreset").value(s));
        if (savePatched && saveTrig)
            preset_[ui::presetNum(*this, s, in("savepreset").value(s), presetPatched, 15)] = virtualPos_;
        if (loadPatched && loadTrig)
            virtualPos_ = preset_[ui::presetNum(*this, s, in("loadpreset").value(s), presetPatched, 15)];
        if (immediate) {
            int cur = ui::clampPreset(std::lround(in("preset").value(s)), 15);
            if (cur != prevPreset_) {
                preset_[prevPreset_] = virtualPos_;
                virtualPos_ = preset_[cur];
                prevPreset_ = cur;
            }
        }

        // --- source value V in [0,1] ----------------------------------------
        float phys = in("pot").value(s);
        if (phys < 0.0f) phys = 0.0f; else if (phys > 1.0f) phys = 1.0f;
        float V;
        if (virt) {
            if (selected) virtualPos_ = potshape::pickup(virtualPos_, prevPhys_, phys);
            prevPhys_ = phys;      // tracked even while deselected (no phantom jump)
            V = virtualPos_;
        } else {
            V = phys;
        }

        // --- compute outputs -------------------------------------------------
        float shaped;             // the 0..1 value that drives onchange
        if (disc >= 1) {
            int idx = (int)std::floor(V * (float)disc);
            if (idx < 0) idx = 0; else if (idx > disc - 1) idx = disc - 1;
            shaped = (float)idx;
            out("output").set(s, (float)idx * scale + offset);
            // all hemisphere outputs dead in discrete mode
            out("bipolar").set(s, 0.0f);
            out("absbipolar").set(s, 0.0f);
            out("lefthalf").set(s, 0.0f);
            out("righthalf").set(s, 0.0f);
            out("lefthalfinv").set(s, 0.0f);
            out("righthalfinv").set(s, 0.0f);
        } else {
            float slope = in("slope").value(s);
            if (slope < 0.001f) slope = 0.001f;
            float sv = std::pow(V, slope);
            float nv = potshape::applyNotch(sv, in("notch").value(s));
            shaped = nv;
            out("output").set(s, nv * scale + offset);
            potshape::Hemispheres h = potshape::hemispheres(nv, scale);
            out("bipolar").set(s, h.bipolar);
            out("absbipolar").set(s, h.absbipolar);
            out("lefthalf").set(s, h.lefthalf);
            out("righthalf").set(s, h.righthalf);
            out("lefthalfinv").set(s, h.lefthalfinv);
            out("righthalfinv").set(s, h.righthalfinv);
        }

        if (std::fabs(shaped - prevShaped_) > 1e-6f) onchangeUntil_ = (long)s.tick + trig;
        prevShaped_ = shaped;
        out("onchange").set(s, (long)s.tick < onchangeUntil_ ? 1.0f : 0.0f);
    }

    // Persisted: the virtual-pot position + all 16 presets + current slot.
    int stateVersion() const override { return 1; }
    void saveState(StateWriter& w) const override {
        w.f(virtualPos_);
        for (int p = 0; p < 16; p++) w.f(preset_[p]);
        w.n(prevPreset_);
    }
    void loadState(EngineState& s, int version, const std::vector<double>& in) override {
        if (version != 1 || in.size() != 18) return;
        if (!inited_) init(s);
        StateReader r{in};
        virtualPos_ = (float)r.f();
        for (int p = 0; p < 16; p++) preset_[p] = (float)r.f();
        prevPreset_ = (int)r.n();
    }

private:
    void init(EngineState& s) {
        int disc = discreteCount(s);
        virtualPos_ = startPos(s, disc);
        for (int p = 0; p < 16; p++) preset_[p] = virtualPos_;
        prevPreset_ = ui::clampPreset(std::lround(in("preset").value(s)), 15);
        float phys = in("pot").value(s);
        if (phys < 0.0f) phys = 0.0f; else if (phys > 1.0f) phys = 1.0f;
        prevPhys_ = phys;
        // seed prevShaped_ to the first-tick value so tick 0 emits no onchange
        float V = virtualMode() ? virtualPos_ : phys;
        if (disc >= 1) {
            int idx = (int)std::floor(V * (float)disc);
            if (idx < 0) idx = 0; else if (idx > disc - 1) idx = disc - 1;
            prevShaped_ = (float)idx;
        } else {
            float slope = in("slope").value(s);
            if (slope < 0.001f) slope = 0.001f;
            prevShaped_ = potshape::applyNotch(std::pow(V, slope), in("notch").value(s));
        }
        inited_ = true;
    }

    int discreteCount(EngineState& s) {
        if (!in("discrete").connected()) return 0;
        int d = (int)std::lround(in("discrete").value(s));
        if (d < 1) return 0;
        if (d > 16) d = 16;
        return d;
    }

    bool virtualMode() {
        return in("select").connected() || in("selectat").connected() ||
               in("preset").connected() || in("loadpreset").connected() ||
               in("savepreset").connected() || in("clear").connected();
    }

    // Start virtual position (0..1). In discrete mode startvalue is a band index;
    // seed to the band center so floor() reproduces it.
    float startPos(EngineState& s, int disc) {
        float sv = in("startvalue").value(s);
        if (disc >= 1) {
            long band = std::lround(sv);
            if (band < 0) band = 0; else if (band > disc - 1) band = disc - 1;
            return ((float)band + 0.5f) / (float)disc;
        }
        if (sv < 0.0f) sv = 0.0f; else if (sv > 1.0f) sv = 1.0f;
        return sv;
    }

    bool  inited_ = false;
    float virtualPos_ = 0.5f;
    float preset_[16] = {};
    int   prevPreset_ = 0;
    float prevPhys_ = 0.0f;
    float prevShaped_ = 0.0f;
    long  onchangeUntil_ = 0;
    bool  caPrev_ = false, clPrev_ = false, spPrev_ = false, lpPrev_ = false;
};

DROID_REGISTER_CIRCUIT(pot, Pot)

} // namespace droid
