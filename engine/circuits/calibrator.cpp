// calibrator — Per-octave VCO pitch correction. Spec: manual/circuits/
// calibrator.md. Property-faithful SPEC-GAP inventions where the manual is
// silent, unverified vs hardware.
//
// Correction model: nine correction points at 0..8 V (integer volts), each in
// SEMITONES (tune inputs: value 1 = 100 cents). Effective point k =
// nudged state + tune_k input, clamped to +-12 semitones (manual: max +-1
// octave). output = input + correction(v)/120; `correction` emits semitones.
//
// SPEC-GAP pins (manual describes them only qualitatively):
//   * LINEAR interpolation between adjacent integer-volt points (the DB8E
//     draws the correction as a piecewise curve across octaves).
//   * tails: v < 0 V -> point0 + tunelowtail*(0 - v_oct); v > 8 V ->
//     point8 + tunehightail*(v_oct - 8) ("per octave" per the Inputs table).
//   * a nudge adds nudgeamount to BOTH adjacent points (so the correction at
//     the played pitch changes by exactly nudgeamount); at an exact integer
//     volt (eps 1e-4 oct) only that point. clearhere zeroes the same point(s)
//     ("might affect a range of up to two octaves"). nudgeup+nudgedown edges
//     in the same tick = reset of the current octave.
//   * ledup/leddown show sign of the current correction (binary 0/1).
//   * select gates nudgeup/nudgedown/clearhere and the LED writes; the pitch
//     path always runs. 4 presets snapshot the nudged points; standard
//     preset/clear semantics (button.cpp conventions). Persistence
//     enumeration, display/header/forcedisplay, dontsave: inert headless.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include "../src/uihelpers.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace droid {

class Calibrator : public Circuit {
    static constexpr int kPoints = 9;
    static constexpr int kPresets = 4;
    static constexpr double kIntEps = 1e-4;   // octaves; integer-volt snap

public:
    void tick(EngineState& s) override {
        bool selected = ui::isSelected(*this, s);
        double v = double(in("input").value(s)) * 10.0;   // octaves (volts)

        handlePresetsAndClear(s);

        // --- nudging (gated by select) ---------------------------------
        bool up = risingEdge(upPrev_, in("nudgeup").value(s));
        bool down = risingEdge(downPrev_, in("nudgedown").value(s));
        bool here = risingEdge(herePrev_, in("clearhere").value(s));
        if (selected) {
            int lo, hi;
            adjacent(v, lo, hi);
            if ((up && down) || here) {
                corr_[lo] = 0;
                corr_[hi] = 0;
            } else if (up || down) {
                float amt = in("nudgeamount").value(s) * (up ? 1.0f : -1.0f);
                corr_[lo] += amt;
                if (hi != lo) corr_[hi] += amt;
            }
        }

        // --- pitch path (always runs) -----------------------------------
        double c = correction(s, v);
        out("output").set(s, in("input").value(s) + float(c / 120.0));
        out("correction").set(s, float(c));
        if (selected) {
            out("ledup").set(s, c > 1e-9 ? 1.0f : 0.0f);
            out("leddown").set(s, c < -1e-9 ? 1.0f : 0.0f);
        }
    }

    // Persisted: the 9 nudged correction points + the 4 preset snapshots + slot.
    int stateVersion() const override { return 1; }
    void saveState(StateWriter& w) const override {
        for (int k = 0; k < kPoints; k++) w.f(corr_[k]);
        for (int p = 0; p < kPresets; p++)
            for (int k = 0; k < kPoints; k++) w.f(presets_[p][k]);
        w.n(prevPreset_);
    }
    void loadState(EngineState&, int version, const std::vector<double>& in) override {
        if (version != 1 || in.size() != (size_t)(kPoints + kPresets * kPoints + 1)) return;
        StateReader r{in};
        for (int k = 0; k < kPoints; k++) corr_[k] = (float)r.f();
        for (int p = 0; p < kPresets; p++)
            for (int k = 0; k < kPoints; k++) presets_[p][k] = (float)r.f();
        prevPreset_ = (int)r.n();
    }

private:
    // Adjacent correction points for pitch v (octaves): one point at integer
    // volts (within eps) or clamped tails, else the two neighbours.
    void adjacent(double v, int& lo, int& hi) {
        if (v <= 0) { lo = hi = 0; return; }
        if (v >= 8) { lo = hi = 8; return; }
        double r = std::floor(v + 0.5);
        if (std::fabs(v - r) < kIntEps) { lo = hi = (int)r; return; }
        lo = (int)std::floor(v);
        hi = lo + 1;
    }

    double point(EngineState& s, int k) {
        double p = corr_[k] + in("tune", k + 1).value(s);
        return std::min(std::max(p, -12.0), 12.0);   // max +-1 octave
    }

    double correction(EngineState& s, double v) {
        if (v < 0) return point(s, 0) + double(in("tunelowtail").value(s)) * (0.0 - v);
        if (v > 8) return point(s, 8) + double(in("tunehightail").value(s)) * (v - 8.0);
        int lo, hi;
        adjacent(v, lo, hi);
        if (lo == hi) return point(s, lo);
        double f = v - lo;
        return (1.0 - f) * point(s, lo) + f * point(s, hi);
    }

    void handlePresetsAndClear(EngineState& s) {
        bool presetPatched = in("preset").connected();
        bool loadPatched = in("loadpreset").connected();
        bool savePatched = in("savepreset").connected();
        bool immediate = presetPatched && !loadPatched && !savePatched;

        bool clearAll = risingEdge(caPrev_, in("clearall").value(s));
        bool clr = risingEdge(clPrev_, in("clear").value(s));
        if (clearAll) {
            std::memset(corr_, 0, sizeof(corr_));
            std::memset(presets_, 0, sizeof(presets_));
        } else if (clr) {
            std::memset(corr_, 0, sizeof(corr_));
            if (immediate) std::memset(presets_[prevPreset_], 0, sizeof(presets_[0]));
        }

        bool saveTrig = risingEdge(spPrev_, in("savepreset").value(s));
        bool loadTrig = risingEdge(lpPrev_, in("loadpreset").value(s));
        if (savePatched && saveTrig)
            std::memcpy(presets_[ui::presetNum(*this, s, in("savepreset").value(s), presetPatched, kPresets - 1)],
                        corr_, sizeof(corr_));
        if (loadPatched && loadTrig)
            std::memcpy(corr_,
                        presets_[ui::presetNum(*this, s, in("loadpreset").value(s), presetPatched, kPresets - 1)],
                        sizeof(corr_));
        if (immediate) {
            int p = ui::clampPreset(lround(in("preset").value(s)), kPresets - 1);
            if (p != prevPreset_) {
                std::memcpy(presets_[prevPreset_], corr_, sizeof(corr_));
                std::memcpy(corr_, presets_[p], sizeof(corr_));
                prevPreset_ = p;
            }
        } else if (presetPatched) {
            prevPreset_ = ui::clampPreset(lround(in("preset").value(s)), kPresets - 1);
        }
    }

    float corr_[kPoints] = {};
    float presets_[kPresets][kPoints] = {};
    int prevPreset_ = 0;
    bool upPrev_ = false, downPrev_ = false, herePrev_ = false;
    bool caPrev_ = false, clPrev_ = false, spPrev_ = false, lpPrev_ = false;
};

DROID_REGISTER_CIRCUIT(calibrator, Calibrator)

} // namespace droid
