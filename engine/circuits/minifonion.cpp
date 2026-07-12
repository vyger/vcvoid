// minifonion — Musical quantizer. Spec: manual/circuits/minifonion.md.
// Gently moves an input pitch (1 V/oct) into selected notes of one of the 108
// Sinfonion scales. All the scale/selection/quantization math lives in the
// shared note core (engine/src/notes.*); this circuit is the wiring: read the
// jacks, drive a NoteSelector, quantize, shift, apply the tuning/transpose
// output stage, and emit the notechange trigger.
//
// The four approved literal readings (documented at length in notes.hpp) apply:
//   1. tie-up rounding (snap + direction=0 search resolve ties upward),
//   2. pitch-ordered note lists (noteshift/selectnoteshift walk by pitch),
//   3. root clamps 0..12 (12 ≡ C), degree clamps 0..107,
//   4. allowed notes are pitch classes replicated across octaves, ±10 V bounded.
//
// Further literal readings taken here (manual silent; SPEC-GAP):
//   * Priority when several modes are active: tuningmode wins over bypass wins
//     over normal quantization. tuningmode outputs tuningpitch "instead of the
//     actual pitch" (so it overrides everything); bypass is a direct input copy.
//   * notechange never fires in bypass (manual-stated) nor in tuningmode
//     (it is not a quantization); an in-flight 10 ms window is allowed to finish
//     but no new change is detected. Change is detected on the pre-transpose
//     quantized note, so a continuous `transpose` (vibrato) does not retrigger.
//   * Triggered mode mirrors quantizer.cpp: with `trigger` patched the output is
//     frozen and only recomputed on a rising edge; before the first edge the
//     held note is 0 V (output is then 0 + transpose).
//   * Shifts are applied after quantization, on the semitone lattice, then the
//     result is re-clamped to the ±10 V border.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include "../src/notes.hpp"
#include <cmath>
#include <cstdint>

namespace droid {

class Minifonion : public Circuit {
public:
    void tick(EngineState& s) override {
        bool  tuningMode  = gate("tuningmode", s);
        float tuningPitch = in("tuningpitch").value(s);
        float transpose   = in("transpose").value(s);

        // Highest priority: tuning mode outputs tuningpitch, no note detection.
        if (tuningMode) {
            out("output").set(s, tuningPitch);
            out("notechange").set(s, notechangeLevel(s));
            return;
        }
        // Next: bypass copies the input straight through, no notechange trigger.
        if (gate("bypass", s)) {
            out("output").set(s, in("input").value(s));
            out("notechange").set(s, 0.0f);
            return;
        }

        // Triggered mode: freeze the note between rising edges at `trigger`.
        bool trigConn  = in("trigger").connected();
        bool trigFired = trigConn && trigGate_.risingEdge(in("trigger").value(s));
        bool doQuant   = trigConn ? trigFired : true;

        if (doQuant) {
            NoteSelector ns;
            ns.setRoot((int)std::lround(in("root").value(s)));
            ns.setDegree((int)std::lround(in("degree").value(s)));
            ns.setHarmonicShift((int)std::lround(in("harmonicshift").value(s)));
            static const char* kSel[7] = {"select1", "select3", "select5", "select7",
                                          "select9", "select11", "select13"};
            for (int i = 0; i < 7; i++) ns.select[i] = gate(kSel[i], s);
            static const char* kFill[5] = {"selectfill1", "selectfill2", "selectfill3",
                                           "selectfill4", "selectfill5"};
            for (int i = 0; i < 5; i++) ns.fill[i] = gate(kFill[i], s);

            int direction = (int)std::lround(in("direction").value(s));
            float q = quantize(in("input").value(s), ns.selectedNotes(), direction);

            int sns = (int)std::lround(in("selectnoteshift").value(s));
            int nos = (int)std::lround(in("noteshift").value(s));
            if (sns != 0 || nos != 0) {
                int semi = (int)std::lround((double)q / (double)kSemitoneUnit);
                semi = ns.shiftNote(semi, sns, nos);
                if (semi > kPitchBorderSemis)  semi = kPitchBorderSemis;
                if (semi < -kPitchBorderSemis) semi = -kPitchBorderSemis;
                q = (float)((double)semi * (double)kSemitoneUnit);
            }
            note_ = q;
            hasNote_ = true;
        }

        float note = hasNote_ ? note_ : 0.0f;
        if (hasNote_ && hadPrev_ && note != prevNote_) {
            long dur = std::lround(0.01 * (double)s.tickRateHz);  // 10 ms
            if (dur < 1) dur = 1;
            notechangeUntil_ = s.tick + (uint64_t)dur;
        }
        if (hasNote_) { prevNote_ = note; hadPrev_ = true; }

        out("output").set(s, note + transpose);
        out("notechange").set(s, notechangeLevel(s));
    }

private:
    bool gate(const char* name, EngineState& s) {
        return in(name).value(s) >= kGateHighThreshold;
    }
    float notechangeLevel(EngineState& s) const {
        return s.tick < notechangeUntil_ ? 1.0f : 0.0f;
    }

    GateReader trigGate_;
    bool  hasNote_ = false, hadPrev_ = false;
    float note_ = 0.0f, prevNote_ = 0.0f;
    uint64_t notechangeUntil_ = 0;
};

DROID_REGISTER_CIRCUIT(minifonion, Minifonion)

} // namespace droid
