// nudge — Modify a value up/down in fixed steps with two buttons. Spec:
// manual/circuits/nudge.md.
//
// buttonup / buttondown each add / subtract `amount` on a rising edge; the value
// is optionally clamped to [minimum, maximum] (either bound may be left
// unwired for an unbounded side) and optionally `wrap`s past a bound to the
// other one. Pressing BOTH buttons at once resets to `startvalue`. `output` is
// value + `offset` (e.g. an octave/transpose offset). ledup / leddown light as a
// gradient showing how far above / below center the value sits. `changed` pulses
// whenever a button action actually moved the value. select/selectat overlays
// gate button + LED processing; 16 presets in the three standard modes;
// startvalue/clear seed the value, clearall wipes presets.
//
// Conventions / SPEC-GAPs (manual silent, prose-only, or hardware-visual):
//   * "both buttons at the same time" -> reset on the rising edge of (up AND
//     down high). There is no debounce window in the manual; near-simultaneous
//     presses that land on the same/adjacent ticks both count because reset
//     fires the moment both gates are high. When both rise together the reset
//     takes precedence over the individual up/down nudges.
//   * clamp vs wrap: an over-max step clamps to `maximum` (or wraps to `minimum`
//     when `wrap` and both bounds are set); symmetric for under-min. `wrap` does
//     nothing unless both bounds are set (per the Inputs table). `startvalue`
//     (and every reset/preset-independent seed) is clamped into the bound range,
//     so e.g. min=3 max=7 startvalue=0 starts at 3 (matches the euklid example).
//   * `changed` fires ONLY for button actions (up/down/both-reset) that changed
//     the value -- not for clear/preset loads (the manual scopes it to nudging
//     and both-press reset). 10 ms trigger.
//   * LED gradient needs both bounds: center=(min+max)/2; above center ledup
//     ramps 0..1 to `maximum` (leddown 0), below center leddown ramps 0..1 to
//     `minimum` (ledup 0), exactly at center both are 1. With an unbounded side
//     the gradient is undefined, so both LEDs stay 0 (SPEC-GAP). LEDs are only
//     written while selected (overlay-safe), like the button family.
//   * DB8E display/header not modelled. select gates button+LED processing;
//     output/changed keep running while deselected. Edge state advances every
//     tick so a press made while deselected causes no phantom edge on reselect.
//   * preset precedence on a tied tick: clearall > clear > savepreset >
//     loadpreset. SD persistence (dontsave / boot load) is a headless no-op;
//     state/presets seed to the clamped startvalue at load.
#include "../src/registry.hpp"
#include "../src/uihelpers.hpp"
#include <cmath>

namespace droid {

class Nudge : public Circuit {
public:
    void tick(EngineState& s) override {
        bool minSet = in("minimum").connected();
        bool maxSet = in("maximum").connected();
        float mn = in("minimum").value(s), mx = in("maximum").value(s);
        float amount = in("amount").value(s);
        bool wrap = in("wrap").value(s) >= kGateHighThreshold;
        float sv = clampToRange(in("startvalue").value(s), minSet, mn, maxSet, mx);

        if (!inited_) {
            value_ = sv;
            for (int p = 0; p < 16; p++) preset_[p] = value_;
            prevPreset_ = ui::clampPreset(std::lround(in("preset").value(s)), 15);
            inited_ = true;
        }

        bool selected = ui::isSelected(*this, s);
        long trig = std::lround(0.01 * s.tickRateHz);
        if (trig < 1) trig = 1;

        // --- clear / clearall / presets (always; not scoped to `changed`) ----
        bool clearAll = risingEdge(caPrev_, in("clearall").value(s));
        bool clr      = risingEdge(clPrev_, in("clear").value(s));
        bool loadPatched   = in("loadpreset").connected();
        bool savePatched   = in("savepreset").connected();
        bool presetPatched = in("preset").connected();
        bool immediate = presetPatched && !loadPatched && !savePatched;

        if (clearAll) {
            value_ = sv;
            for (int p = 0; p < 16; p++) preset_[p] = value_;
        } else if (clr) {
            value_ = sv;
            if (immediate) preset_[prevPreset_] = value_;
        }
        bool saveTrig = risingEdge(spPrev_, in("savepreset").value(s));
        bool loadTrig = risingEdge(lpPrev_, in("loadpreset").value(s));
        if (savePatched && saveTrig)
            preset_[ui::presetNum(*this, s, in("savepreset").value(s), presetPatched, 15)] = value_;
        if (loadPatched && loadTrig)
            value_ = preset_[ui::presetNum(*this, s, in("loadpreset").value(s), presetPatched, 15)];
        if (immediate) {
            int cur = ui::clampPreset(std::lround(in("preset").value(s)), 15);
            if (cur != prevPreset_) {
                preset_[prevPreset_] = value_;
                value_ = preset_[cur];
                prevPreset_ = cur;
            }
        }

        // --- button edges (advanced every tick) ------------------------------
        bool upNow   = in("buttonup").value(s) >= kGateHighThreshold;
        bool downNow = in("buttondown").value(s) >= kGateHighThreshold;
        bool upRise   = upNow && !prevUp_;
        bool downRise = downNow && !prevDown_;
        bool bothNow  = upNow && downNow;
        bool bothRise = bothNow && !prevBoth_;
        prevUp_ = upNow; prevDown_ = downNow; prevBoth_ = bothNow;

        // --- button actions (only while selected) ----------------------------
        bool changed = false;
        if (selected) {
            if (bothRise) {
                if (value_ != sv) { value_ = sv; changed = true; }
            } else {
                if (upRise) {
                    float nv = nudge(value_, amount, minSet, mn, maxSet, mx, wrap);
                    if (nv != value_) { value_ = nv; changed = true; }
                }
                if (downRise) {
                    float nv = nudge(value_, -amount, minSet, mn, maxSet, mx, wrap);
                    if (nv != value_) { value_ = nv; changed = true; }
                }
            }
        }
        if (changed) changedUntil_ = (long)s.tick + trig;

        // --- outputs ---------------------------------------------------------
        out("output").set(s, value_ + in("offset").value(s));
        out("changed").set(s, (long)s.tick < changedUntil_ ? 1.0f : 0.0f);
        if (selected) {
            float up = 0.0f, down = 0.0f;
            if (minSet && maxSet && mx > mn) {
                float center = 0.5f * (mn + mx);
                if (value_ > center)      up   = clamp01((value_ - center) / (mx - center));
                else if (value_ < center) down = clamp01((center - value_) / (center - mn));
                else                      { up = 1.0f; down = 1.0f; }
            }
            out("ledup").set(s, up);
            out("leddown").set(s, down);
        }
    }

    // Persisted: the nudged value + all 16 presets + current preset slot.
    int stateVersion() const override { return 1; }
    void saveState(StateWriter& w) const override {
        w.f(value_);
        for (int p = 0; p < 16; p++) w.f(preset_[p]);
        w.n(prevPreset_);
    }
    void loadState(EngineState&, int version, const std::vector<double>& in) override {
        if (version != 1 || in.size() != 18) return;
        StateReader r{in};
        value_ = (float)r.f();
        for (int p = 0; p < 16; p++) preset_[p] = (float)r.f();
        prevPreset_ = (int)r.n();
        inited_ = true;   // skip the in-tick seed; persisted values win
    }

private:
    // Step by `delta` (signed), honoring bounds and wrap. Over-max -> max (or
    // wrap to min if wrap+both bounds); under-min -> min (or wrap to max).
    static float nudge(float v, float delta, bool minSet, float mn,
                       bool maxSet, float mx, bool wrap) {
        float nv = v + delta;
        if (delta > 0.0f && maxSet && nv > mx + 1e-6f)
            nv = (wrap && minSet) ? mn : mx;
        else if (delta < 0.0f && minSet && nv < mn - 1e-6f)
            nv = (wrap && maxSet) ? mx : mn;
        return nv;
    }
    static float clampToRange(float v, bool minSet, float mn, bool maxSet, float mx) {
        if (minSet && v < mn) v = mn;
        if (maxSet && v > mx) v = mx;
        return v;
    }
    static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    bool  inited_ = false;
    float value_ = 0.0f;
    float preset_[16] = {};
    int   prevPreset_ = 0;
    bool  prevUp_ = false, prevDown_ = false, prevBoth_ = false;
    long  changedUntil_ = 0;
    bool  caPrev_ = false, clPrev_ = false, spPrev_ = false, lpPrev_ = false;
};

DROID_REGISTER_CIRCUIT(nudge, Nudge)

} // namespace droid
