// notebuttons — 12 radio-style note-selection buttons -> a chromatic note.
// Spec: manual/circuits/notebuttons.md.
//
// Twelve buttons act as radio buttons: the rising edge of button<i> selects note
// i-1 (button1 = C = 0 ... button12 = B = 11). The selection PERSISTS after
// release (it is a selector, not a momentary button). `output` is the selected
// note 0..11; `semitone` is that divided by 120 (a 1 V/oct semitone: note/120
// engine units, since 1.0 == 10 V and one semitone == 1/12 V == 1/120); `gate`
// is high while a button is held. `led<i>` lights the selected note. `startnote`
// seeds the selection (and `clear`); `clearall` also wipes the 16 presets.
// `clock`, if patched, quantizes presses in time: the note only changes on the
// next clock pulse. select/selectat gate the button + LED handling (and `gate`,
// which reads the buttons) for overlays; output/semitone keep emitting the
// latched note while deselected.
//
// engine/src/notes.hpp was evaluated and deliberately NOT reused: its
// NoteSelector/quantize core models diatonic SCALE selection (root + degree +
// column masks). notebuttons is purely chromatic (12 fixed semitones, no scale),
// so the scale core does not fit; the only overlap is the semitone size
// (1/120 engine unit), which is trivially computed inline here.
//
// Conventions / SPEC-GAPs (manual silent):
//   * LED persistence: the manual calls these "radio buttons" and refers to
//     buttongroup; by that analogy led<selected> is held on persistently (not
//     just while the finger is down) and every other LED is off. LEDs are
//     written only while the circuit is selected (overlay handoff).
//   * gate + select: `gate` reads the physical buttons, so it is treated as a
//     button-dependent output -> high only while the circuit is selected AND a
//     button is held (0 while deselected, so an overlaid circuit's presses do
//     not leak through). output/semitone keep emitting the latched note.
//   * clock quantization: a press while `clock` is patched stores a PENDING note
//     (the last press wins if several arrive before the clock); the pending note
//     is applied to output/semitone/led on the next clock rising edge. `gate`
//     still follows the physical hold immediately. A clock with no pending press
//     is a no-op. A press and clock in the SAME tick flush immediately (the
//     press is staged, then the clock later in the tick applies it).
//   * presets: 16 slots holding the selected note, with the three standard modes
//     (triggered / immediate auto-save-old + load-new on `preset` change /
//     value-carrying trigger when `preset` is unpatched). Simultaneous-trigger
//     precedence clearall > clear > savepreset > loadpreset. SD persistence
//     (dontsave / boot load) and display/header (DB8E) are headless no-ops.
#include "../src/registry.hpp"
#include "../src/uihelpers.hpp"
#include <cmath>

namespace droid {

class NoteButtons : public Circuit {
    static constexpr int   N = 12;

public:
    void tick(EngineState& s) override {
        if (!inited_) init(s);

        bool selected  = ui::isSelected(*this, s);
        bool clockUsed = in("clock").connected();

        // --- clear / clearall ------------------------------------------------
        bool clearAll = risingEdge(caPrev_, in("clearall").value(s));
        bool clr      = risingEdge(clPrev_, in("clear").value(s));

        bool loadPatched   = in("loadpreset").connected();
        bool savePatched   = in("savepreset").connected();
        bool presetPatched = in("preset").connected();
        bool immediate = presetPatched && !loadPatched && !savePatched;

        int sn = startNote(s);
        if (clearAll) {
            note_ = sn; pending_ = -1;
            for (int p = 0; p < 16; p++) preset_[p] = sn;
        } else if (clr) {
            note_ = sn; pending_ = -1;
            if (immediate) preset_[prevPreset_] = sn;
        }

        // --- presets ---------------------------------------------------------
        bool saveTrig = risingEdge(spPrev_, in("savepreset").value(s));
        bool loadTrig = risingEdge(lpPrev_, in("loadpreset").value(s));
        if (savePatched && saveTrig)
            preset_[ui::presetNum(*this, s, in("savepreset").value(s), presetPatched, 15)] = note_;
        if (loadPatched && loadTrig)
            note_ = preset_[ui::presetNum(*this, s, in("loadpreset").value(s), presetPatched, 15)];
        if (immediate) {
            int cur = ui::clampPreset(std::lround(in("preset").value(s)), 15);
            if (cur != prevPreset_) {
                preset_[prevPreset_] = note_;   // auto-save old
                note_ = preset_[cur];           // load new
                prevPreset_ = cur;
            }
        }

        // --- button edges (advanced every tick, even while deselected) -------
        bool anyHeld = false;
        int  pressedNote = -1;   // most recent rising edge this tick (later wins)
        for (int i = 1; i <= N; i++) {
            bool nowHigh = in("button", i).value(s) >= kGateHighThreshold;
            bool rise = nowHigh && !prevHigh_[i];
            prevHigh_[i] = nowHigh;
            if (nowHigh) anyHeld = true;
            if (selected && rise) pressedNote = i - 1;
        }

        // --- apply a press: immediately, or pend until the next clock --------
        if (selected && pressedNote >= 0) {
            if (clockUsed) pending_ = pressedNote;
            else           note_ = pressedNote;
        }
        bool clockRise = risingEdge(ckPrev_, in("clock").value(s));
        if (clockUsed && clockRise && pending_ >= 0) {
            note_ = pending_;
            pending_ = -1;
        }

        if (note_ < 0) note_ = 0; else if (note_ > N - 1) note_ = N - 1;

        // --- outputs ---------------------------------------------------------
        out("output").set(s, float(note_));
        out("semitone").set(s, float(note_) / 120.0f);
        out("gate").set(s, (selected && anyHeld) ? 1.0f : 0.0f);
        if (selected)
            for (int i = 1; i <= N; i++)
                out("led", i).set(s, (i - 1 == note_) ? 1.0f : 0.0f);
    }

    // Persisted: the latched note + all 16 presets + current preset slot.
    // (pending_ is a runtime clock-quantization latch, not manual state.)
    int stateVersion() const override { return 1; }
    void saveState(StateWriter& w) const override {
        w.n(note_);
        for (int p = 0; p < 16; p++) w.n(preset_[p]);
        w.n(prevPreset_);
    }
    void loadState(EngineState& s, int version, const std::vector<double>& in) override {
        if (version != 1 || in.size() != 18) return;
        if (!inited_) init(s);
        StateReader r{in};
        note_ = (int)r.n();
        for (int p = 0; p < 16; p++) preset_[p] = (int)r.n();
        prevPreset_ = (int)r.n();
    }

private:
    void init(EngineState& s) {
        note_ = startNote(s);
        for (int p = 0; p < 16; p++) preset_[p] = note_;
        prevPreset_ = ui::clampPreset(std::lround(in("preset").value(s)), 15);
        pending_ = -1;
        inited_ = true;
    }

    int startNote(EngineState& s) {
        long v = std::lround(in("startnote").value(s));
        if (v < 0) v = 0; else if (v > N - 1) v = N - 1;
        return (int)v;
    }

    bool inited_ = false;
    int  note_ = 0;
    int  pending_ = -1;
    int  preset_[16] = {};
    int  prevPreset_ = 0;
    bool prevHigh_[N + 1] = {};
    bool caPrev_ = false, clPrev_ = false, spPrev_ = false, lpPrev_ = false, ckPrev_ = false;
};

DROID_REGISTER_CIRCUIT(notebuttons, NoteButtons)

} // namespace droid
