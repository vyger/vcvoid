// fadermatrix — a matrix of up to 4x4 virtual motor faders where `rowcolumn`
// selects a row OR a column of the matrix onto the physical faders. Spec:
// manual/circuits/fadermatrix.md. It reuses the fader value/notch engine
// (fadercore.hpp) and the motorfader orchestration (motorized takeover, presets,
// touch buttons, panel LEDs); what is special is the row/column mapping.
//
// The matrix cell (r,c) (0-based) is `output{r+1}{c+1}` — e.g. output23 is
// row 2, column 3. `rowcolumn` maps four cells onto the four physical faders
// (firstfader + 0..3):
//   * rowcolumn 0..N-1 : select ROW = rowcolumn; cells (row, 0..N-1) go onto
//     faders 0..N-1.
//   * rowcolumn N..2N-1: select COLUMN = rowcolumn - N; cells (0..N-1, column)
//     go onto faders 0..N-1.
// For the documented 4x4 that is exactly the manual's table (0..3 rows,
// 4..7 columns). SPEC-GAP: for smaller square matrices the manual does not give
// the rowcolumn values, so we generalise the column offset to N (the matrix
// dimension) — a 3x3 uses 0..2 for rows and 3..5 for columns.
//
// Notches, startvalue and ledcolor are PER COLUMN (notches{c+1} etc., manual:
// "every parameter in the same column ... has the same number of notches").
// ledvalue{r+1}{c+1} is per cell. There is no outputscale/offset.
//
// A cell is "on a fader" only while (a) the whole circuit is selected
// (select/selectat, for the 8x8-via-four-matrices case) AND (b) rowcolumn maps
// it. A cell that is not on a fader freezes its value but still emits `output`;
// its `button` is 0. When a cell newly lands on a fader (rowcolumn changed, or a
// preset/clear recalled values) the motor drives that fader to the cell's stored
// value — the same pickup-free motorized takeover as motorfader.
//
// SPEC-GAPs / conventions: shared with motorfader/fadercore (instant motor,
// haptics + LEDs panel-only, startvalue as a raw 0..1 position quantized through
// the notches). 6 presets store all 16 cells at once.
#include "../src/registry.hpp"
#include "../src/uihelpers.hpp"
#include "../src/fadercore.hpp"
#include <cmath>

namespace droid {

namespace fc = fadercore;

class FaderMatrix : public Circuit {
    static constexpr int kMax = 4;
    static constexpr int kPresets = 6;

    // Stable per-index jack names (kMax is a compile-time constant, so these
    // are fixed literal tables, not per-instance state). The memo's fast path
    // keys on POINTER identity, so a name built fresh into a stack buffer
    // every call (the old std::snprintf(nm, ...) here) would defeat it —
    // these tables give every call site for a given r/c the SAME pointer on
    // every tick.
    static constexpr const char* kOutputName[kMax] = {"output1", "output2", "output3", "output4"};
    static constexpr const char* kButtonName[kMax] = {"button1", "button2", "button3", "button4"};
    static constexpr const char* kLedValueName[kMax] = {"ledvalue1", "ledvalue2", "ledvalue3", "ledvalue4"};

public:
    void tick(EngineState& s) override {
        if (!inited_) init(s);
        bool circuitSelected = ui::isSelected(*this, s);
        int rc = clampRowCol(std::lround(in("rowcolumn").value(s)));

        bool recall = handlePresets(s);

        for (int r = 0; r < n_; r++)
            for (int c = 0; c < n_; c++) {
                int notches = readNotches(s, c);       // per column
                int faderI = mappedFader(rc, r, c);    // -1 if not mapped
                bool onFader = circuitSelected && faderI >= 0;
                bool button = false;

                if (onFader) {
                    int gf = firstFader_ + faderI;
                    FaderState* f = s.controllers.fader(gf);
                    bool touched = f ? f->touched : false;
                    // Take over whenever this cell newly lands on a fader OR
                    // moves to a DIFFERENT fader (rowcolumn swap) OR a preset
                    // recalled its value -- otherwise it would read the target
                    // fader's stale position as user movement.
                    bool takeover = (wasFader_[r][c] != faderI) || recall;
                    float src = takeover ? value_[r][c]
                                         : (f ? f->position : value_[r][c]);
                    fc::Result res = fc::evaluate(src, notches, touched);
                    value_[r][c] = res.position;
                    s.controllers.commandFader(gf, res.position);
                    if (f) {
                        f->notches = notches;
                        f->led = ledBrightness(s, r, c, notches);   // panel-only
                        f->ledColor = in("ledcolor", c + 1).value(s);
                    }
                    button = f && f->plate;   // the plate below the fader, not the hold
                }
                wasFader_[r][c] = onFader ? faderI : -1;

                out(kOutputName[r], c + 1).set(s, fc::outputOf(value_[r][c], notches));
                out(kButtonName[r], c + 1).set(s, button ? 1.0f : 0.0f);
            }
    }

    // Persisted: every cell's value + all 6 preset banks + current slot.
    // Serialized at the fixed maximum (4x4) for a version-stable length.
    int stateVersion() const override { return 1; }
    void saveState(StateWriter& w) const override {
        for (int r = 0; r < kMax; r++)
            for (int c = 0; c < kMax; c++) w.f(value_[r][c]);
        for (int p = 0; p < kPresets; p++)
            for (int r = 0; r < kMax; r++)
                for (int c = 0; c < kMax; c++) w.f(preset_[p][r][c]);
        w.n(prevPreset_);
    }
    void loadState(EngineState& s, int version, const std::vector<double>& in) override {
        if (version != 1 || in.size() != (size_t)(kMax * kMax + kPresets * kMax * kMax + 1)) return;
        if (!inited_) init(s);
        StateReader rd{in};
        for (int r = 0; r < kMax; r++)
            for (int c = 0; c < kMax; c++) value_[r][c] = (float)rd.f();
        for (int p = 0; p < kPresets; p++)
            for (int r = 0; r < kMax; r++)
                for (int c = 0; c < kMax; c++) preset_[p][r][c] = (float)rd.f();
        prevPreset_ = (int)rd.n();
        for (int r = 0; r < kMax; r++)
            for (int c = 0; c < kMax; c++) wasFader_[r][c] = -1;   // force takeover
    }

private:
    void init(EngineState& s) {
        long g = std::lround(in("firstfader").value(s));
        firstFader_ = g < 1 ? 1 : (int)g;
        n_ = matrixDim();
        for (int r = 0; r < kMax; r++)
            for (int c = 0; c < kMax; c++) wasFader_[r][c] = -1;
        for (int r = 0; r < n_; r++)
            for (int c = 0; c < n_; c++) {
                float seed = fc::seed(in("startvalue", c + 1).value(s), readNotches(s, c));
                value_[r][c] = seed;
                for (int p = 0; p < kPresets; p++) preset_[p][r][c] = seed;
            }
        prevPreset_ = ui::clampPreset(std::lround(in("preset").value(s)), kPresets - 1);
        inited_ = true;
    }

    // Matrix dimension N: the highest row/column with a connected output/button.
    int matrixDim() {
        int n = 0;
        for (int r = 1; r <= kMax; r++) {
            for (int c = 1; c <= kMax; c++) {
                bool used = out(kOutputName[r - 1], c).connected();
                used = used || out(kButtonName[r - 1], c).connected();
                if (used) { if (r > n) n = r; if (c > n) n = c; }
            }
        }
        return n < 1 ? 1 : n;
    }

    // Fader index (0..N-1) a cell is mapped to for the current rowcolumn, or -1.
    int mappedFader(int rc, int r, int c) const {
        if (rc < n_) return (r == rc) ? c : -1;           // row select
        int col = rc - n_;                                // column select
        return (c == col) ? r : -1;
    }

    int clampRowCol(long rc) const {
        long hi = 2L * n_ - 1;
        return rc < 0 ? 0 : (rc > hi ? (int)hi : (int)rc);
    }

    int readNotches(EngineState& s, int col) {
        long n = std::lround(in("notches", col + 1).value(s));
        return n < 0 ? 0 : (int)n;
    }

    float ledBrightness(EngineState& s, int r, int c, int notches) {
        const char* nm = kLedValueName[r];
        if (in(nm, c + 1).connected()) return in(nm, c + 1).value(s);
        return fc::outputOf(value_[r][c], notches);
    }

    bool handlePresets(EngineState& s) {
        bool clearAll = risingEdge(caPrev_, in("clearall").value(s));
        bool clr      = risingEdge(clPrev_, in("clear").value(s));
        bool loadPatched   = in("loadpreset").connected();
        bool savePatched   = in("savepreset").connected();
        bool presetPatched = in("preset").connected();
        bool immediate = presetPatched && !loadPatched && !savePatched;
        bool recall = false;

        if (clearAll) {
            seedAll(s);
            for (int r = 0; r < n_; r++)
                for (int c = 0; c < n_; c++)
                    for (int p = 0; p < kPresets; p++) preset_[p][r][c] = value_[r][c];
            recall = true;
        } else if (clr) {
            seedAll(s);
            if (immediate)
                for (int r = 0; r < n_; r++)
                    for (int c = 0; c < n_; c++) preset_[prevPreset_][r][c] = value_[r][c];
            recall = true;
        }
        bool saveTrig = risingEdge(spPrev_, in("savepreset").value(s));
        bool loadTrig = risingEdge(lpPrev_, in("loadpreset").value(s));
        if (savePatched && saveTrig) {
            int b = ui::presetNum(*this, s, in("savepreset").value(s), presetPatched, kPresets - 1);
            for (int r = 0; r < n_; r++)
                for (int c = 0; c < n_; c++) preset_[b][r][c] = value_[r][c];
        }
        if (loadPatched && loadTrig) {
            int b = ui::presetNum(*this, s, in("loadpreset").value(s), presetPatched, kPresets - 1);
            for (int r = 0; r < n_; r++)
                for (int c = 0; c < n_; c++) value_[r][c] = preset_[b][r][c];
            recall = true;
        }
        if (immediate) {
            int cur = ui::clampPreset(std::lround(in("preset").value(s)), kPresets - 1);
            if (cur != prevPreset_) {
                for (int r = 0; r < n_; r++)
                    for (int c = 0; c < n_; c++) {
                        preset_[prevPreset_][r][c] = value_[r][c];
                        value_[r][c] = preset_[cur][r][c];
                    }
                prevPreset_ = cur;
                recall = true;
            }
        }
        return recall;
    }

    void seedAll(EngineState& s) {
        for (int r = 0; r < n_; r++)
            for (int c = 0; c < n_; c++)
                value_[r][c] = fc::seed(in("startvalue", c + 1).value(s), readNotches(s, c));
    }

    bool  inited_ = false;
    int   n_ = 4;
    int   firstFader_ = 1;
    float value_[kMax][kMax] = {};
    float preset_[kPresets][kMax][kMax] = {};
    int   wasFader_[kMax][kMax];   // fader index each cell was on last tick (-1 = none)
    int   prevPreset_ = 0;
    bool  caPrev_ = false, clPrev_ = false, spPrev_ = false, lpPrev_ = false;
};

DROID_REGISTER_CIRCUIT(fadermatrix, FaderMatrix)

} // namespace droid
