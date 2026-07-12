// button — Multi-mode latch/momentary button UI circuit. Spec:
// manual/circuits/button.md.
//
// A push button (rising edge of the `button` gate) drives a persisted state P in
// [0, states-1]. states=2 is a toggle, states 3/4 cycle through the values, and
// states=1 is a pure momentary button. `output` emits the value of the current
// state (value1..value4, aliases offvalue=value1 / onvalue=value2, numeric
// defaults 0/1/2/3); `led` mirrors the state as a brightness (E/(states-1)).
// `inverted` mirrors the value order, `negated` is 1 iff state 0. Double-click
// mode, long/short press, select/selectat overlays, and 16 presets are all
// supported. `startvalue` seeds P (and `clear`); `clearall` also wipes presets.
//
// Conventions / SPEC-GAPs (manual silent or self-contradictory):
//   * value aliasing: the Inputs table's per-row wording is internally reversed
//     ("onvalue = alternative name for value1" then "value1 is the same as
//     offvalue"). We follow the consistent reading given by the prose (§"three
//     or four states") AND the numeric defaults: offvalue == value1 (default 0,
//     state 0/off) and onvalue == value2 (default 1, state 1/on). value3/value4
//     default 2/3. An explicit valueN wins over its alias if both are patched.
//   * states auto-derivation: if `states` is unpatched, patching value4 -> 4
//     states, else value3 -> 3, else 2 ("If you specify value3 or value4, states
//     is automatically set"). An explicit `states` always wins.
//   * momentary (states=1): E = 1 while the button is held (and selected), else
//     0 -> output=value2 held / value1 released, led=E. (Manual: "output ...
//     always is high as long as your finger is on the button".)
//   * double-click window is UNSPECIFIED by the manual (no ms given, unlike
//     buttongroup's 25 ms burst). We use 0.5 s; a second rising edge within that
//     window toggles P, and a lone press momentarily inverts P while held.
//     Double-click is only honored when states==2 (manual: "only makes sense if
//     the number of states is 2").
//   * long/short press: `longpress` connected defers the toggle to release and
//     suppresses it if the hold reached longpresstime (default 1.5 s); the
//     longpress gate is high from that moment until release. `shortpress` fires
//     a 10 ms trigger on press, or (when longpress is also connected) on release
//     iff the press was short. Combining longpress with double-click is
//     unspecified; longpress takes precedence.
//   * select/selectat gate button processing AND led writing (overlays). While
//     deselected the button is not processed (no toggle, no long/short press, no
//     momentary inversion) and led is left untouched, but output/inverted/
//     negated keep emitting the latched state. Edge state is tracked every tick
//     so a press made while deselected causes no phantom edge on reselect.
//   * presets: 16 slots holding the integer state, with the three standard
//     modes (triggered, immediate auto-save-old/load-new on `preset` change, and
//     value-carrying trigger when `preset` is unpatched). SD persistence
//     (dontsave / boot load) is a no-op headless; state/presets seed to
//     startvalue at load. Simultaneous triggers apply in order clear(all),
//     then savepreset, then loadpreset, non-exclusively — so a same-tick
//     loadpreset overrides a clear's state (clearall still wins: it wipes
//     the presets first, so the load reads the cleared value).
//   * display/header (DB8E) not modelled headless.
#include "../src/registry.hpp"
#include "../src/uihelpers.hpp"
#include <cmath>

namespace droid {

class Button : public Circuit {
    static constexpr float kDoubleClickSeconds = 0.5f; // SPEC-GAP (see header)

public:
    void tick(EngineState& s) override {
        if (!inited_) init(s);

        int st = numStates(s);
        bool selected = ui::isSelected(*this, s);
        bool doubleClick = in("doubleclickmode").value(s) >= kGateHighThreshold;
        bool longUsed = out("longpress").connected();
        bool shortUsed = out("shortpress").connected();

        long thrTicks = lround(in("longpresstime").value(s) * s.tickRateHz);
        if (thrTicks < 1) thrTicks = 1;
        long trigTicks = lround(0.01 * s.tickRateHz);
        if (trigTicks < 1) trigTicks = 1;
        long dclickTicks = lround(kDoubleClickSeconds * s.tickRateHz);
        if (dclickTicks < 1) dclickTicks = 1;

        // --- button edges (advanced every tick, even when deselected) --------
        bool nowHigh = in("button").value(s) >= kGateHighThreshold;
        bool rise = nowHigh && !prevHigh_;
        bool fall = !nowHigh && prevHigh_;
        prevHigh_ = nowHigh;

        // --- clear / clearall ------------------------------------------------
        bool clearAll = risingEdge(caPrev_, in("clearall").value(s));
        bool clr      = risingEdge(clPrev_, in("clear").value(s));

        bool loadPatched   = in("loadpreset").connected();
        bool savePatched   = in("savepreset").connected();
        bool presetPatched = in("preset").connected();
        bool immediate = presetPatched && !loadPatched && !savePatched;

        int sv = startValue(s, st);
        if (clearAll) {
            state_ = sv;
            for (int p = 0; p < 16; p++) preset_[p] = sv;
        } else if (clr) {
            state_ = sv;
            if (immediate) preset_[prevPreset_] = sv;
        }

        // --- presets ---------------------------------------------------------
        bool saveTrig = risingEdge(spPrev_, in("savepreset").value(s));
        bool loadTrig = risingEdge(lpPrev_, in("loadpreset").value(s));
        if (savePatched && saveTrig)
            preset_[ui::presetNum(*this, s, in("savepreset").value(s), presetPatched, 15)] = state_;
        if (loadPatched && loadTrig)
            state_ = preset_[ui::presetNum(*this, s, in("loadpreset").value(s), presetPatched, 15)];
        if (immediate) {
            int cur = ui::clampPreset(lround(in("preset").value(s)), 15);
            if (cur != prevPreset_) {
                preset_[prevPreset_] = state_;   // auto-save old
                state_ = preset_[cur];           // load new
                prevPreset_ = cur;
            }
        }
        if (state_ >= st) state_ = st - 1;
        if (state_ < 0) state_ = 0;

        // --- button processing (only while selected) ------------------------
        if (selected) {
            long heldBefore = heldTicks_;   // hold length up to (excl) this tick
            if (rise) {
                if (longUsed) {
                    // toggle deferred to release
                } else if (doubleClick && st == 2) {
                    if (recentPress_ && sincePress_ <= dclickTicks) {
                        advance(st);              // completed double-click
                        recentPress_ = false;
                    } else {
                        recentPress_ = true; sincePress_ = 0;
                    }
                    if (shortUsed) shortUntil_ = s.tick + trigTicks;
                } else {
                    advance(st);
                    if (shortUsed) shortUntil_ = s.tick + trigTicks;
                }
            }
            if (fall && longUsed) {
                bool wasLong = heldBefore >= thrTicks;
                if (!wasLong) {
                    advance(st);
                    if (shortUsed) shortUntil_ = s.tick + trigTicks;
                }
            }
            if (nowHigh) heldTicks_++; else heldTicks_ = 0;
            if (recentPress_) {
                sincePress_++;
                if (sincePress_ > dclickTicks) recentPress_ = false;
            }
        } else {
            heldTicks_ = 0;   // finger control handed to another circuit
        }

        // --- effective state E ----------------------------------------------
        int P = state_;
        bool press = selected && nowHigh;
        int E;
        if (st == 1)                         E = press ? 1 : 0;
        else if (doubleClick && st == 2)     E = press ? (1 - P) : P;
        else                                 E = P;

        // --- outputs (output/inverted/negated always; led only if selected) --
        out("output").set(s, valueFor(s, E + 1));
        int sN = st < 2 ? 2 : st;
        out("inverted").set(s, valueFor(s, (sN - 1 - E) + 1));   // mirror order
        out("negated").set(s, E == 0 ? 1.0f : 0.0f);

        if (selected) {
            float led = (st == 1) ? float(E) : float(E) / float(st - 1);
            out("led").set(s, led);
        }

        bool longGate = longUsed && selected && nowHigh && heldTicks_ >= thrTicks;
        out("longpress").set(s, longGate ? 1.0f : 0.0f);
        out("shortpress").set(s, (long)s.tick < shortUntil_ ? 1.0f : 0.0f);
    }

    // Persisted: the latched state + all 16 presets + current preset slot.
    int stateVersion() const override { return 1; }
    void saveState(StateWriter& w) const override {
        w.n(state_);
        for (int p = 0; p < 16; p++) w.n(preset_[p]);
        w.n(prevPreset_);
    }
    void loadState(EngineState& s, int version, const std::vector<double>& in) override {
        if (version != 1 || in.size() != 18) return;   // unrecognized: keep defaults
        if (!inited_) init(s);
        StateReader r{in};
        state_ = (int)r.n();
        for (int p = 0; p < 16; p++) preset_[p] = (int)r.n();
        prevPreset_ = (int)r.n();
    }

private:
    void init(EngineState& s) {
        int st = numStates(s);
        state_ = startValue(s, st);
        for (int p = 0; p < 16; p++) preset_[p] = state_;
        prevPreset_ = ui::clampPreset(lround(in("preset").value(s)), 15);
        inited_ = true;
    }

    void advance(int st) { state_ = (st > 0) ? (state_ + 1) % st : 0; }

    // Effective value for state index (1-based): explicit valueN, else its
    // alias (value1=offvalue, value2=onvalue), else numeric default N-1.
    float valueFor(EngineState& s, int idx) {
        if (in("value", idx).connected()) return in("value", idx).value(s);
        if (idx == 1 && in("offvalue").connected()) return in("offvalue").value(s);
        if (idx == 2 && in("onvalue").connected())  return in("onvalue").value(s);
        return float(idx - 1);
    }

    int numStates(EngineState& s) {
        int st;
        if (in("states").connected())        st = (int)lround(in("states").value(s));
        else if (in("value", 4).connected()) st = 4;
        else if (in("value", 3).connected()) st = 3;
        else                                 st = 2;
        if (st < 1) st = 1; else if (st > 4) st = 4;
        return st;
    }

    int startValue(EngineState& s, int st) {
        long v = lround(in("startvalue").value(s));
        if (v < 0) v = 0; else if (v > st - 1) v = st - 1;
        return (int)v;
    }

    static long lround(float v) { return std::lround(v); }
    static long lround(double v) { return std::lround(v); }

    bool inited_ = false;
    int  state_ = 0;
    int  preset_[16] = {};
    int  prevPreset_ = 0;
    bool prevHigh_ = false;
    long heldTicks_ = 0;
    long shortUntil_ = 0;
    bool recentPress_ = false;
    long sincePress_ = 0;
    bool caPrev_ = false, clPrev_ = false, spPrev_ = false, lpPrev_ = false;
};

DROID_REGISTER_CIRCUIT(button, Button)

} // namespace droid
