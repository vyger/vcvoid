// matrixmixer — 4x4 unity-gain matrix mixer. Spec: manual/circuits/matrixmixer.md.
//
// Each of 16 toggle buttons (button<i><j>, i = input row, j = output column)
// enables mixing input i into output j. Each output is the sum of its enabled
// inputs (unity gain), or their maximum when `mixmax` = 1, or a linear blend in
// between; the four `auxin` are then added directly (used for cascading). LEDs
// mirror the toggle state. `startvalue` picks the initial matrix (0 clear /
// 1 diagonal / 2 all-set), also applied by `clear`; `clearall` also wipes the
// presets. 16 presets via preset/loadpreset/savepreset with the three standard
// modes (triggered, immediate on preset change, and value-carrying trigger when
// `preset` is unpatched). `select`/`selectat` gate button processing + LED
// writing for overlays (outputs always run).
//
// Conventions / SPEC-GAPs (manual silent):
//   * button toggles on the rising edge of each button gate; readers are advanced
//     every tick so a press made while deselected is consumed (no phantom edge on
//     reselect) but does not toggle.
//   * an enabled node whose input is unpatched contributes 0 to both sum and max
//     (a node is enabled by its button, independent of patching) — differs from
//     `mixer`, which excludes unpatched inputs from max; matrixmixer's manual
//     gives no such carve-out. max over an empty enabled set is 0.
//   * `auxin` is summed into the output after the mix/max blend (not folded into
//     the max), matching "mixed directly into the outputs".
//   * SD persistence (dontsave / load-on-boot) is a no-op headless; presets are
//     in-memory and seeded to the start state at load.
//   * simultaneous triggers apply in order clearall/clear, then savepreset, then
//     loadpreset (non-exclusive), so a same-tick load overrides a clear.
#include "../src/registry.hpp"
#include "../src/uihelpers.hpp"
#include "../src/gatereader.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace droid {

class MatrixMixer : public Circuit {
    static constexpr int N = 4;

public:
    void tick(EngineState& s) override {
        if (!inited_) init(s);

        bool selected = ui::isSelected(*this, s);

        // --- clear / clearall -----------------------------------------------
        bool clearAll = clearAllGate_.risingEdge(in("clearall").value(s));
        bool clr      = clearGate_.risingEdge(in("clear").value(s));

        bool loadPatched   = in("loadpreset").connected();
        bool savePatched   = in("savepreset").connected();
        bool presetPatched = in("preset").connected();
        bool immediate = presetPatched && !loadPatched && !savePatched;

        // read button gates every tick so edges stay tracked even when deselected
        bool btnEdge[N][N];
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++)
                btnEdge[i][j] = btnGate_[i][j].risingEdge(button(s, i, j));

        if (clearAll) {
            int sv = startValue(s);
            setStart(node_, sv);
            for (int p = 0; p < 16; p++) setStart(preset_[p], sv);
        } else if (clr) {
            int sv = startValue(s);
            setStart(node_, sv);
            if (immediate) setStart(preset_[prevPreset_], sv);   // manual: current preset cleared too
        }

        // --- presets --------------------------------------------------------
        bool saveTrig = saveGate_.risingEdge(in("savepreset").value(s));
        bool loadTrig = loadGate_.risingEdge(in("loadpreset").value(s));
        if (savePatched && saveTrig)
            copyMatrix(preset_[ui::presetNum(*this, s, in("savepreset").value(s), presetPatched, 15)], node_);
        if (loadPatched && loadTrig)
            copyMatrix(node_, preset_[ui::presetNum(*this, s, in("loadpreset").value(s), presetPatched, 15)]);

        if (immediate) {
            int cur = ui::clampPreset((long)std::lround(in("preset").value(s)), 15);
            if (cur != prevPreset_) {
                copyMatrix(preset_[prevPreset_], node_);   // auto-save current
                copyMatrix(node_, preset_[cur]);           // load new
                prevPreset_ = cur;
            }
        }

        // --- button toggles (only while selected) --------------------------
        if (selected)
            for (int i = 0; i < N; i++)
                for (int j = 0; j < N; j++)
                    if (btnEdge[i][j]) node_[i][j] = !node_[i][j];

        // --- compute outputs ------------------------------------------------
        float mixmax = in("mixmax").value(s);
        if (mixmax < 0.0f) mixmax = 0.0f; else if (mixmax > 1.0f) mixmax = 1.0f;
        for (int j = 0; j < N; j++) {
            float sum = 0.0f, mx = 0.0f; bool any = false;
            for (int i = 0; i < N; i++) {
                if (!node_[i][j]) continue;
                float v = in("input", i + 1).value(s);
                sum += v;
                if (!any || v > mx) mx = v;
                any = true;
            }
            float blended = (1.0f - mixmax) * sum + mixmax * mx;
            out("output", j + 1).set(s, blended + in("auxin", j + 1).value(s));
        }

        // --- LEDs (only while selected) -------------------------------------
        if (selected)
            for (int i = 0; i < N; i++) {
                char ln[12]; std::snprintf(ln, sizeof ln, "led%d", i + 1);
                for (int j = 0; j < N; j++)
                    out(ln, j + 1).set(s, node_[i][j] ? 1.0f : 0.0f);
            }
    }

    // Persisted: the 4x4 matrix + all 16 presets (each a 16-bit mask) + slot.
    int stateVersion() const override { return 1; }
    void saveState(StateWriter& w) const override {
        w.n((long)packMatrix(node_));
        for (int p = 0; p < 16; p++) w.n((long)packMatrix(preset_[p]));
        w.n(prevPreset_);
    }
    void loadState(EngineState& s, int version, const std::vector<double>& in) override {
        if (version != 1 || in.size() != 18) return;
        if (!inited_) init(s);
        StateReader r{in};
        unpackMatrix(node_, (uint16_t)r.n());
        for (int p = 0; p < 16; p++) unpackMatrix(preset_[p], (uint16_t)r.n());
        prevPreset_ = (int)r.n();
    }

private:
    static uint16_t packMatrix(const bool m[N][N]) {
        uint16_t bits = 0;
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++)
                if (m[i][j]) bits |= (uint16_t)(1u << (i * N + j));
        return bits;
    }
    static void unpackMatrix(bool m[N][N], uint16_t bits) {
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++)
                m[i][j] = (bits >> (i * N + j)) & 1u;
    }

    void init(EngineState& s) {
        int sv = startValue(s);
        setStart(node_, sv);
        for (int p = 0; p < 16; p++) setStart(preset_[p], sv);
        prevPreset_ = ui::clampPreset((long)std::lround(in("preset").value(s)), 15);
        inited_ = true;
    }

    float button(EngineState& s, int i, int j) {
        char bn[12]; std::snprintf(bn, sizeof bn, "button%d", i + 1);
        return in(bn, j + 1).value(s);
    }

    int startValue(EngineState& s) {
        long v = std::lround(in("startvalue").value(s));
        if (v < 0) v = 0; else if (v > 2) v = 2;
        return (int)v;
    }
    static void setStart(bool m[N][N], int sv) {
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++)
                m[i][j] = (sv == 2) ? true : (sv == 1 ? (i == j) : false);
    }
    static void copyMatrix(bool dst[N][N], const bool src[N][N]) {
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++) dst[i][j] = src[i][j];
    }

    bool inited_ = false;
    int  prevPreset_ = 0;
    bool node_[N][N] = {};
    bool preset_[16][N][N] = {};
    GateReader btnGate_[N][N];
    GateReader clearGate_, clearAllGate_, loadGate_, saveGate_;
};

DROID_REGISTER_CIRCUIT(matrixmixer, MatrixMixer)

} // namespace droid
