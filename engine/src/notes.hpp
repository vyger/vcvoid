#pragma once
// notes.hpp — shared scale / note-selection core for the Sinfonion-family
// quantizers (minifonion, arpeggio, chord). Spec: manual/circuits/minifonion.md
// + manual/scales.md. The 108-scale table it reads lives in
// engine/gen/scales.gen.cpp (from manual/scales.md).
//
// Four approved literal readings apply throughout (SPEC-GAP — the manual is
// silent on each exact case; the design records the chosen resolution):
//   1. Tie-breaking rounds UP — both the snap-to-nearest-semitone step and the
//      direction=0 nearest-allowed-note search resolve exact ties upward.
//   2. Note lists are PITCH-ordered (ascending within the octave), not degree-
//      column ordered. Some scales' columns are non-monotonic (e.g. scale 3
//      sus4, the slashchords), so selectedNotes()/scaleNotes() sort by pitch;
//      noteshift/selectnoteshift walk those pitch-ordered lists.
//   3. Out-of-range root/degree CLAMP: degree to 0..107, root to 0..12 (root 12
//      is C per the manual's root table, i.e. behaves as 0 after mod 12).
//   4. Allowed notes are PITCH CLASSES replicated across every octave;
//      quantization and shifting operate on that infinite lattice, clamped only
//      at the ±10 V (±1.0 engine) border.
//
// Engine units: 1.0 == 10 V; 1 octave (1 V/oct) = 0.1; 1 semitone = 0.1/12
// (matching octave.cpp / the rest of the engine's pitch math).
#include <cstdint>
#include <vector>

namespace droid {

constexpr float kSemitoneUnit = 0.1f / 12.0f; // 1 semitone in engine units
constexpr float kOctaveUnit   = 0.1f;         // 1 octave (1 V) in engine units
constexpr int   kPitchBorderSemis = 120;      // ±10 V == ±120 semitones

// Note selection = root + degree (scale) + which of the 12 columns are on
// (select1..13 / selectfill1..5) + harmonicshift (staged after selection,
// only ever removes notes).
struct NoteSelector {
    int  root = 0;             // 0..12; pitch class is root % 12 (12 -> C)
    int  degree = 0;           // 0..107 scale index
    bool select[7] = {false, false, false, false, false, false, false};
                               // select1,3,5,7,9,11,13 -> columns I,III,V,VII,II,IV,VI
    bool fill[5]   = {false, false, false, false, false};  // selectfill1..5 -> fill1..5
    int  harmonicshift = 0;    // -7..7

    // Clamping setters (decision 3).
    void setRoot(int r);       // clamps to 0..12
    void setDegree(int d);     // clamps to 0..107
    void setHarmonicShift(int h); // clamps to -7..7
    int  rootPc() const { return ((root % 12) + 12) % 12; }

    // Pitch classes 0..11, ascending (decision 2), after root transposition.
    //  - selectedNotes(): the selected columns (with the "all select/selectfill
    //    zero -> all seven scale notes" smart default), then harmonicshift
    //    filtering. May be empty (e.g. harmonicshift -7 with no fills selected).
    //  - scaleNotes(): the seven scale degrees I..VII regardless of selection.
    std::vector<int> selectedNotes() const;
    std::vector<int> scaleNotes() const;

    // Pitch classes in INTERVAL (select-input) order — select1,3,5,7,9,11,13
    // then selectfill1..5 — with the smart default and harmonicshift applied,
    // but NOT pitch-sorted and NOT deduplicated. `chord` builds its voices from
    // "the first N notes" in this chord-theory (stacked-interval) order, so the
    // default C lydian yields root/3rd/5th/7th = C,E,G,B (a maj7) rather than
    // the first four scale steps. minifonion/arpeggio use the pitch-ordered
    // selectedNotes() instead.
    std::vector<int> orderedSelectedNotes() const;

    // Shift a semitone value along the octave-replicated lattice: first by
    // `sns` selected-note steps, then by `nos` scale-note steps (order per the
    // manual). Each step count is clamped to ±24. Returns a semitone offset
    // (octave crossings included), NOT just a pitch class.
    int shiftNote(int semitone, int sns, int nos) const;
};

// Quantize an engine-unit pitch to the allowed pitch classes.
//   Step 1: snap to the nearest semitone (ties up, decision 1).
//   Step 2: if that semitone's pitch class is allowed, done; else search per
//   `direction` (0 nearest/ties-up, 1 up, 2 down) across the octave-replicated
//   lattice, never leaving ±10 V. If the border is reached with no allowed
//   note, the border voltage itself is output (manual rule).
float quantize(float pitchCv, const std::vector<int>& allowed, int direction);

// Shared output stage: while tuningMode is high, output tuningPitch; otherwise
// output quantized + transpose (manual: transpose is added only when not tuning).
float applyTuning(bool tuningMode, float tuningPitch, float quantized, float transpose);

} // namespace droid
