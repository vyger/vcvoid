// buttongroup — A group of connected buttons (radio buttons and friends).
// Spec: manual/circuits/buttongroup.md.
//
// Each button<i> is a trigger; a rising edge toggles that button, then the
// active count is forced back into [minactive, maxactive]: turning a button on
// past maxactive evicts the OLDEST-activated button; a toggle-off that would
// drop below minactive is refused (the button stays on). With min=max=1 this is
// classic radio-button behavior. `output` is the sum of the active buttons'
// values; buttonoutput<i> is each button's value (or 0 when off); led<i> mirrors
// the active state. startbutton seeds the selection (and `clear`); clearall also
// wipes the 16 presets. select/selectat gate button processing + led writing for
// overlays (buttonoutput and output keep running). buttonpress/longpress/
// selectionchanged/extrapress report press activity.
//
// Conventions / SPEC-GAPs (manual silent, prose-only, or self-contradictory):
//   * min/max enforcement: on turn-on over maxactive, evict oldest active (FIFO
//     activation order); on a turn-off that would fall below minactive, REFUSE
//     the deselection (pressed button stays on -> selection unchanged ->
//     extrapress). The manual's "another button might go on" (a *different*
//     button activating to hold minactive on deselect) is NOT implemented; we
//     keep the pressed button on, which is the intuitive radio behavior and is
//     deterministic. maxactive 0 is treated as 1; minactive is clamped to
//     [0, maxactive].
//   * value defaults DIFFER between the two consumers, per the manual: the
//     `output` sum uses value<i> default i-1 (0,1,2,...); buttonoutput<i> uses
//     value<i> default 1 ("If valueX is not defined ... the value 1 is output").
//     An explicit value<i> feeds both.
//   * startbutton: N (default 1) selects only button N; 0 selects none (needs
//     minactive 0); -1 fills the first maxactive buttons. After seeding, the
//     selection is filled from the lowest group indices up to minactive. The
//     "group" is the set of connected button jacks (falling back to indices
//     1..32 if none are connected).
//   * selectionchanged fires immediately on a selection change; the manual's
//     "delayed up to 25 ms due to burst detection" is NOT modelled (SPEC-GAP).
//     extrapress fires when a press left the selection unchanged. buttonpress
//     fires on every press, or (when longpress is connected) on release iff the
//     press was short. longpress fires once when a held button reaches
//     longpresstime (default 1.5 s). All four are 10 ms triggers. The selection
//     toggle itself always happens on the press edge (longpress only affects the
//     buttonpress/longpress trigger timing, not the toggle) -- literal reading.
//   * presets store the active-button bitmask; activation order is rebuilt
//     ascending on load (FIFO history is not persisted). 16 presets, three
//     standard modes (triggered / immediate auto-save+load on `preset` change /
//     value-carrying trigger when `preset` unpatched). SD persistence
//     (dontsave / boot load) is a headless no-op; state/presets seed to the
//     start selection at load. Precedence clearall > clear > savepreset >
//     loadpreset. display/header (DB8E) not modelled.
#include "../src/registry.hpp"
#include "../src/uihelpers.hpp"
#include <cmath>
#include <cstdint>
#include <vector>

namespace droid {

class ButtonGroup : public Circuit {
    static constexpr int   N = 32;

public:
    void tick(EngineState& s) override {
        if (!inited_) init(s);

        int minA, maxA;
        limits(s, minA, maxA);
        bool selected = ui::isSelected(*this, s);
        bool longUsed = out("longpress").connected();

        long thr = std::lround(in("longpresstime").value(s) * s.tickRateHz);
        if (thr < 1) thr = 1;
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
            applyStart(s, minA, maxA);
            uint32_t m = mask();
            for (int p = 0; p < 16; p++) preset_[p] = m;
        } else if (clr) {
            applyStart(s, minA, maxA);
            if (immediate) preset_[prevPreset_] = mask();
        }

        bool saveTrig = risingEdge(spPrev_, in("savepreset").value(s));
        bool loadTrig = risingEdge(lpPrev_, in("loadpreset").value(s));
        if (savePatched && saveTrig)
            preset_[ui::presetNum(*this, s, in("savepreset").value(s), presetPatched, 15)] = mask();
        if (loadPatched && loadTrig)
            setMask(preset_[ui::presetNum(*this, s, in("loadpreset").value(s), presetPatched, 15)]);
        if (immediate) {
            int cur = ui::clampPreset(std::lround(in("preset").value(s)), 15);
            if (cur != prevPreset_) {
                preset_[prevPreset_] = mask();
                setMask(preset_[cur]);
                prevPreset_ = cur;
            }
        }

        // --- per-button edge processing -------------------------------------
        bool emitPress = false, emitLong = false, emitSel = false, emitExtra = false;
        for (int i = 1; i <= N; i++) {
            bool nowHigh = in("button", i).value(s) >= kGateHighThreshold;
            bool rise = nowHigh && !prevHigh_[i];
            bool fall = !nowHigh && prevHigh_[i];
            prevHigh_[i] = nowHigh;

            if (selected) {
                if (rise) {
                    if (!longUsed) emitPress = true;
                    bool changed = toggle(i, minA, maxA);
                    if (changed) emitSel = true; else emitExtra = true;
                }
                if (fall && longUsed && heldTicks_[i] < thr) emitPress = true;

                if (nowHigh) {
                    heldTicks_[i]++;
                    if (longUsed && !longFired_[i] && heldTicks_[i] >= thr) {
                        emitLong = true; longFired_[i] = true;
                    }
                } else { heldTicks_[i] = 0; longFired_[i] = false; }
            } else { heldTicks_[i] = 0; longFired_[i] = false; }
        }
        if (emitPress) bpUntil_ = (long)s.tick + trig;
        if (emitLong)  lpUntil_ = (long)s.tick + trig;
        if (emitSel)   scUntil_ = (long)s.tick + trig;
        if (emitExtra) epUntil_ = (long)s.tick + trig;

        // --- outputs ---------------------------------------------------------
        float sum = 0.0f;
        for (int i = 1; i <= N; i++) {
            if (active_[i]) sum += valueOut(s, i);
            out("buttonoutput", i).set(s, active_[i] ? valueButton(s, i) : 0.0f);
        }
        out("output").set(s, sum);
        if (selected)
            for (int i = 1; i <= N; i++)
                out("led", i).set(s, active_[i] ? 1.0f : 0.0f);

        out("buttonpress").set(s,      (long)s.tick < bpUntil_ ? 1.0f : 0.0f);
        out("longpress").set(s,        (long)s.tick < lpUntil_ ? 1.0f : 0.0f);
        out("selectionchanged").set(s, (long)s.tick < scUntil_ ? 1.0f : 0.0f);
        out("extrapress").set(s,       (long)s.tick < epUntil_ ? 1.0f : 0.0f);
    }

    // Persisted: the active-button mask + all 16 preset masks + current slot.
    // (FIFO activation order is rebuilt ascending on load, like setMask/preset.)
    int stateVersion() const override { return 1; }
    void saveState(StateWriter& w) const override {
        w.n((long)mask());
        for (int p = 0; p < 16; p++) w.n((long)preset_[p]);
        w.n(prevPreset_);
    }
    void loadState(EngineState& s, int version, const std::vector<double>& in) override {
        if (version != 1 || in.size() != 18) return;
        if (!inited_) init(s);
        StateReader r{in};
        setMask((uint32_t)r.n());
        for (int p = 0; p < 16; p++) preset_[p] = (uint32_t)r.n();
        prevPreset_ = (int)r.n();
    }

private:
    void init(EngineState& s) {
        int minA, maxA; limits(s, minA, maxA);
        applyStart(s, minA, maxA);
        uint32_t m = mask();
        for (int p = 0; p < 16; p++) preset_[p] = m;
        prevPreset_ = ui::clampPreset(std::lround(in("preset").value(s)), 15);
        inited_ = true;
    }

    void limits(EngineState& s, int& minA, int& maxA) {
        maxA = (int)std::lround(in("maxactive").value(s));
        if (maxA < 1) maxA = 1; else if (maxA > N) maxA = N;
        minA = (int)std::lround(in("minactive").value(s));
        if (minA < 0) minA = 0; else if (minA > maxA) minA = maxA;
    }

    // Candidate button indices: connected buttons ascending, else 1..N.
    std::vector<int> group() {
        std::vector<int> g;
        for (int i = 1; i <= N; i++) if (in("button", i).connected()) g.push_back(i);
        if (g.empty()) for (int i = 1; i <= N; i++) g.push_back(i);
        return g;
    }

    void applyStart(EngineState& s, int minA, int maxA) {
        for (int i = 0; i <= N; i++) active_[i] = false;
        order_.clear();
        std::vector<int> g = group();
        long sb = std::lround(in("startbutton").value(s));
        if (sb == -1) {
            for (int idx : g) { if ((int)order_.size() >= maxA) break; activate(idx); }
        } else if (sb == 0) {
            // none
        } else if (sb >= 1 && sb <= N) {
            activate((int)sb);
        }
        // fill up to minactive from the lowest group indices
        for (int idx : g) { if ((int)order_.size() >= minA) break; if (!active_[idx]) activate(idx); }
    }

    void activate(int i) { if (!active_[i]) { active_[i] = true; order_.push_back(i); } }

    // Toggle button i honoring min/max; returns true if the selection changed.
    bool toggle(int i, int minA, int maxA) {
        if (active_[i]) {
            if (count() - 1 >= minA) { active_[i] = false; eraseOrder(i); return true; }
            return false;   // deselect refused (would drop below minactive)
        }
        activate(i);
        while (count() > maxA) {
            int victim = order_.front();
            order_.erase(order_.begin());
            active_[victim] = false;
        }
        return true;
    }

    int count() const { int c = 0; for (int i = 1; i <= N; i++) if (active_[i]) c++; return c; }
    void eraseOrder(int i) {
        for (auto it = order_.begin(); it != order_.end(); ++it)
            if (*it == i) { order_.erase(it); return; }
    }
    uint32_t mask() const { uint32_t m = 0; for (int i = 1; i <= N; i++) if (active_[i]) m |= (1u << (i - 1)); return m; }
    void setMask(uint32_t m) {
        order_.clear();
        for (int i = 1; i <= N; i++) { active_[i] = (m >> (i - 1)) & 1u; if (active_[i]) order_.push_back(i); }
    }

    float valueOut(EngineState& s, int i) {
        return in("value", i).connected() ? in("value", i).value(s) : float(i - 1);
    }
    float valueButton(EngineState& s, int i) {
        return in("value", i).connected() ? in("value", i).value(s) : 1.0f;
    }

    bool inited_ = false;
    bool active_[N + 1] = {};
    uint32_t preset_[16] = {};
    int prevPreset_ = 0;
    std::vector<int> order_;
    bool prevHigh_[N + 1] = {};
    long heldTicks_[N + 1] = {};
    bool longFired_[N + 1] = {};
    long bpUntil_ = 0, lpUntil_ = 0, scUntil_ = 0, epUntil_ = 0;
    bool caPrev_ = false, clPrev_ = false, spPrev_ = false, lpPrev_ = false;
};

DROID_REGISTER_CIRCUIT(buttongroup, ButtonGroup)

} // namespace droid
