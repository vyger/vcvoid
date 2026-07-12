// faderbank — a bank of up to 16 virtual motor faders. Spec:
// manual/circuits/faderbank.md. "It does not add any extra functionality to
// motorfader" — it is the same per-fader value/notch engine (fadercore.hpp)
// applied to a contiguous run of faders, so this file shares that engine and the
// motorfader orchestration (select/selectat overlay with motorized takeover,
// presets, touch buttons, panel LEDs) and only differs where the doc says:
//   * firstfader (not fader): the first fader of the bank; lanes are
//     firstfader + 0, +1, ... in global fader order.
//   * the number of faders is NOT set — it is the highest output<N>/button<N>
//     (or ledvalue<N>) jack used (faderbank.md "the number of faders ...
//     corresponds to the number of output jacks you use").
//   * notches and ledcolor are common to all faders; ledvalue1..16 are per
//     fader. There is no outputscale/outputoffset (faderbank has neither jack).
//   * 6 presets (motorfader has 8), each storing all lanes at once.
//
// SPEC-GAPs / conventions: identical to motorfader for everything shared — see
// motorfader.cpp / fadercore.hpp (notch value/position mapping, instant motor,
// force-feedback feel + LEDs panel-only, startvalue as a raw 0..1 position
// quantized through the notches). faderbank-specific:
//   * count: the max connected index among output/button/ledvalue (1..16). An
//     out-of-range firstfader+i leaves that lane's fader untouched (output still
//     tracks the internal value, button 0).
//   * select gates the WHOLE bank together; on the tick the bank becomes
//     selected each lane's motor drives its fader to that lane's stored value.
#include "../src/registry.hpp"
#include "../src/uihelpers.hpp"
#include "../src/fadercore.hpp"
#include <cmath>

namespace droid {

namespace fc = fadercore;

class FaderBank : public Circuit {
    static constexpr int kMaxFaders = 16;
    static constexpr int kPresets = 6;

public:
    void tick(EngineState& s) override {
        if (!inited_) init(s);
        int notches = readNotches(s);
        bool selected = ui::isSelected(*this, s);
        float ledColor = in("ledcolor").value(s);

        bool recall = handlePresets(s, notches);

        for (int i = 0; i < count_; i++) {
            int faderIdx = firstFader_ + i;
            FaderState* f = s.controllers.fader(faderIdx);
            bool touched = f ? f->touched : false;

            if (f && selected) {
                bool takeover = !wasSelected_ || recall;
                float src = takeover ? value_[i] : f->position;
                fc::Result r = fc::evaluate(src, notches, touched);
                value_[i] = r.position;
                s.controllers.commandFader(faderIdx, r.position);
                f->notches = notches;
                f->led = ledBrightness(s, i, notches);   // panel-only
                f->ledColor = ledColor;                  // panel-only
            }
            out("output", i + 1).set(s, fc::outputOf(value_[i], notches));
            out("button", i + 1).set(s, (selected && f && f->plate) ? 1.0f : 0.0f);
        }
        wasSelected_ = selected;
    }

    // Persisted: every lane's value + all 6 preset banks + current slot.
    // Serialized at the fixed maximum (16 lanes) for a version-stable length.
    int stateVersion() const override { return 1; }
    void saveState(StateWriter& w) const override {
        for (int i = 0; i < kMaxFaders; i++) w.f(value_[i]);
        for (int p = 0; p < kPresets; p++)
            for (int i = 0; i < kMaxFaders; i++) w.f(preset_[p][i]);
        w.n(prevPreset_);
    }
    void loadState(EngineState& s, int version, const std::vector<double>& in) override {
        if (version != 1 || in.size() != (size_t)(kMaxFaders + kPresets * kMaxFaders + 1)) return;
        if (!inited_) init(s);
        StateReader r{in};
        for (int i = 0; i < kMaxFaders; i++) value_[i] = (float)r.f();
        for (int p = 0; p < kPresets; p++)
            for (int i = 0; i < kMaxFaders; i++) preset_[p][i] = (float)r.f();
        prevPreset_ = (int)r.n();
        wasSelected_ = false;   // force motorized takeover to the loaded values
    }

private:
    void init(EngineState& s) {
        count_ = faderCount();
        long g = std::lround(in("firstfader").value(s));
        firstFader_ = g < 1 ? 1 : (int)g;
        int notches = readNotches(s);
        float seed = fc::seed(in("startvalue").value(s), notches);
        for (int i = 0; i < count_; i++) {
            value_[i] = seed;
            for (int p = 0; p < kPresets; p++) preset_[p][i] = seed;
        }
        prevPreset_ = ui::clampPreset(std::lround(in("preset").value(s)), kPresets - 1);
        inited_ = true;
    }

    // Bank size = highest connected output/button/ledvalue lane (manual).
    int faderCount() {
        int n = 0;
        for (int i = 1; i <= kMaxFaders; i++)
            if (out("output", i).connected() || out("button", i).connected() ||
                in("ledvalue", i).connected())
                n = i;
        return n;
    }

    int readNotches(EngineState& s) {
        long n = std::lround(in("notches").value(s));
        return n < 0 ? 0 : (int)n;
    }

    float ledBrightness(EngineState& s, int lane, int notches) {
        if (in("ledvalue", lane + 1).connected())
            return in("ledvalue", lane + 1).value(s);
        return fc::outputOf(value_[lane], notches);
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
            float seed = fc::seed(in("startvalue").value(s), notches);
            for (int i = 0; i < count_; i++) {
                value_[i] = seed;
                for (int p = 0; p < kPresets; p++) preset_[p][i] = seed;
            }
            recall = true;
        } else if (clr) {
            float seed = fc::seed(in("startvalue").value(s), notches);
            for (int i = 0; i < count_; i++) value_[i] = seed;
            if (immediate)
                for (int i = 0; i < count_; i++) preset_[prevPreset_][i] = seed;
            recall = true;
        }
        bool saveTrig = risingEdge(spPrev_, in("savepreset").value(s));
        bool loadTrig = risingEdge(lpPrev_, in("loadpreset").value(s));
        if (savePatched && saveTrig) {
            int b = ui::presetNum(*this, s, in("savepreset").value(s), presetPatched, kPresets - 1);
            for (int i = 0; i < count_; i++) preset_[b][i] = value_[i];
        }
        if (loadPatched && loadTrig) {
            int b = ui::presetNum(*this, s, in("loadpreset").value(s), presetPatched, kPresets - 1);
            for (int i = 0; i < count_; i++) value_[i] = preset_[b][i];
            recall = true;
        }
        if (immediate) {
            int cur = ui::clampPreset(std::lround(in("preset").value(s)), kPresets - 1);
            if (cur != prevPreset_) {
                for (int i = 0; i < count_; i++) {
                    preset_[prevPreset_][i] = value_[i];
                    value_[i] = preset_[cur][i];
                }
                prevPreset_ = cur;
                recall = true;
            }
        }
        return recall;
    }

    bool  inited_ = false;
    int   count_ = 0;
    int   firstFader_ = 1;
    float value_[kMaxFaders] = {};
    float preset_[kPresets][kMaxFaders] = {};
    int   prevPreset_ = 0;
    bool  wasSelected_ = false;
    bool  caPrev_ = false, clPrev_ = false, spPrev_ = false, lpPrev_ = false;
};

DROID_REGISTER_CIRCUIT(faderbank, FaderBank)

} // namespace droid
