#include "harness.hpp"
#include "src/notes.hpp"
#include "gen/scales.gen.hpp"
#include <vector>
using namespace droid;

// Compare a note list against an expected pitch-ordered set.
static bool eq(const std::vector<int>& got, std::vector<int> want) {
    if (got.size() != want.size()) return false;
    for (size_t i = 0; i < got.size(); i++) if (got[i] != want[i]) return false;
    return true;
}
#define CHECK_LIST(got, ...) CHECK(eq((got), __VA_ARGS__))

// ---- scale table spot checks (column order, vs manual/scales.md) ----------
TEST(notes_scale_table_columns) {
    // Column order stored: [I,II,III,IV,V,VI,VII,fill1..fill5].
    auto row = [](int d) { return std::vector<int>(gen::kScales[d], gen::kScales[d] + 12); };
    CHECK_LIST(row(0),   {0, 2, 4, 6, 7, 9, 11, 1, 3, 5, 8, 10});  // C Lydian
    CHECK_LIST(row(1),   {0, 2, 4, 5, 7, 9, 11, 1, 3, 6, 8, 10});  // C Ionian
    CHECK_LIST(row(7),   {0, 2, 3, 5, 7, 8, 10, 1, 4, 6, 9, 11});  // C Aeolian
    CHECK_LIST(row(3),   {0, 2, 5, 4, 7, 9, 10, 1, 3, 6, 8, 11});  // sus4 (III/IV swapped)
    CHECK_LIST(row(10),  {0, 2, 3, 5, 6, 8, 9, 11, 1, 4, 7, 10});  // C Diminished
    CHECK_LIST(row(24),  {0, 3, 1, 7, 5, 10, 8, 2, 4, 6, 9, 11});  // Slashchord D♭♯11/C
    CHECK_LIST(row(47),  {11, 0, 2, 4, 5, 7, 9, 1, 3, 6, 8, 10});  // B Locrian
    CHECK_LIST(row(107), {11, 0, 1, 4, 5, 7, 8, 2, 3, 6, 9, 10});  // dbl harm maj VII
}

// ---- pitch ordering (decision 2): non-monotonic columns sort by pitch ------
TEST(notes_pitch_ordered_scale) {
    NoteSelector ns;         // degree 0 lydian by default
    ns.setDegree(3);         // sus4: columns I..VII = {0,2,5,4,7,9,10}
    CHECK_LIST(ns.scaleNotes(), {0, 2, 4, 5, 7, 9, 10});   // sorted ascending
    CHECK_LIST(ns.selectedNotes(), {0, 2, 4, 5, 7, 9, 10}); // smart default = scale
}

// ---- select/fill masks incl. all-zero smart default -----------------------
TEST(notes_smart_default_all_seven) {
    NoteSelector ns;         // nothing selected -> all seven scale notes
    CHECK_LIST(ns.selectedNotes(), {0, 2, 4, 6, 7, 9, 11}); // C lydian degrees
}
TEST(notes_select_triad) {
    NoteSelector ns;         // degree 0 lydian
    ns.select[0] = ns.select[1] = ns.select[2] = true;  // select1(I),select3(III),select5(V)
    CHECK_LIST(ns.selectedNotes(), {0, 4, 7});
}
// ---- interval-ordered selection (chord voice order) -----------------------
TEST(notes_ordered_smart_default_stacked_thirds) {
    NoteSelector ns;         // degree 0 lydian, nothing selected
    // Interval order = select1,3,5,7,9,11,13 = root,3rd,5th,7th,9th,11th,13th.
    // For C lydian that is C,E,G,B,D,F#,A -> {0,4,7,11,2,6,9} (NOT pitch-sorted).
    CHECK_LIST(ns.orderedSelectedNotes(), {0, 4, 7, 11, 2, 6, 9});
}
TEST(notes_ordered_dminor_first_four) {
    NoteSelector ns;
    ns.setRoot(2); ns.setDegree(7);   // D natural minor
    // root,3rd,5th,7th,... = D,F,A,C,E,G,Bb -> first four = D minor 7.
    CHECK_LIST(ns.orderedSelectedNotes(), {2, 5, 9, 0, 4, 7, 10});
}
TEST(notes_ordered_triad_preserves_order) {
    NoteSelector ns;
    ns.select[2] = ns.select[0] = ns.select[1] = true;  // 5th,root,3rd all on
    // Order follows the fixed interval sequence, not the set-bit order:
    // select1(root),select3(3rd),select5(5th) -> C,E,G.
    CHECK_LIST(ns.orderedSelectedNotes(), {0, 4, 7});
}
TEST(notes_ordered_harmonicshift_filters) {
    NoteSelector ns;
    ns.setHarmonicShift(1);   // disable all fills (none selected -> no change to scale)
    CHECK_LIST(ns.orderedSelectedNotes(), {0, 4, 7, 11, 2, 6, 9});
    ns.setHarmonicShift(-1);  // disable the root -> root drops out, order preserved
    CHECK_LIST(ns.orderedSelectedNotes(), {4, 7, 11, 2, 6, 9});
}

TEST(notes_select_single_fill) {
    NoteSelector ns;
    ns.fill[0] = true;       // selectfill1 -> fill1 column (lydian fill1 = C♯ = 1)
    CHECK_LIST(ns.selectedNotes(), {1});
}
TEST(notes_select_all_columns) {
    NoteSelector ns;
    for (int i = 0; i < 7; i++) ns.select[i] = true;
    for (int i = 0; i < 5; i++) ns.fill[i] = true;
    CHECK_LIST(ns.selectedNotes(), {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});  // chromatic
}

// ---- root transposition incl. root = 12 ≡ 0 (decision 3) ------------------
TEST(notes_root_transpose_d_minor) {
    NoteSelector ns;
    ns.setRoot(2);           // D
    ns.setDegree(7);         // natural minor -> D natural minor
    CHECK_LIST(ns.selectedNotes(), {0, 2, 4, 5, 7, 9, 10});  // D E F G A A♯ C
}
TEST(notes_root_twelve_equals_zero) {
    NoteSelector a, b;
    a.setRoot(0);
    b.setRoot(12);
    CHECK_LIST(b.selectedNotes(), a.selectedNotes());
    CHECK(b.rootPc() == 0);
}
TEST(notes_root_degree_clamp) {
    NoteSelector ns;
    ns.setRoot(99);  CHECK(ns.root == 12);
    ns.setRoot(-5);  CHECK(ns.root == 0);
    ns.setDegree(999); CHECK(ns.degree == 107);
    ns.setDegree(-3);  CHECK(ns.degree == 0);
}

// ---- harmonicshift exact cumulative sets (all 12 selected, lydian) --------
// lydian columns: I=0 II=2 III=4 IV=6 V=7 VI=9 VII=11, fills={1,3,5,8,10}
TEST(notes_harmonicshift_positive) {
    NoteSelector ns;
    for (int i = 0; i < 7; i++) ns.select[i] = true;
    for (int i = 0; i < 5; i++) ns.fill[i] = true;
    ns.setHarmonicShift(1); CHECK_LIST(ns.selectedNotes(), {0, 2, 4, 6, 7, 9, 11}); // -fills
    ns.setHarmonicShift(2); CHECK_LIST(ns.selectedNotes(), {0, 2, 4, 7, 9, 11});    // -11th(6)
    ns.setHarmonicShift(3); CHECK_LIST(ns.selectedNotes(), {0, 2, 4, 7, 11});       // -13th(9)
    ns.setHarmonicShift(4); CHECK_LIST(ns.selectedNotes(), {0, 4, 7, 11});          // -9th(2)
    ns.setHarmonicShift(5); CHECK_LIST(ns.selectedNotes(), {0, 4, 7});              // -7th(11)
    ns.setHarmonicShift(6); CHECK_LIST(ns.selectedNotes(), {0, 7});                 // -3rd(4)
    ns.setHarmonicShift(7); CHECK_LIST(ns.selectedNotes(), {0});                    // -5th(7) root only
}
TEST(notes_harmonicshift_negative) {
    NoteSelector ns;
    for (int i = 0; i < 7; i++) ns.select[i] = true;
    for (int i = 0; i < 5; i++) ns.fill[i] = true;
    ns.setHarmonicShift(-1); CHECK_LIST(ns.selectedNotes(), {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}); // -root(0)
    ns.setHarmonicShift(-2); CHECK_LIST(ns.selectedNotes(), {1, 2, 3, 4, 5, 6, 8, 9, 10, 11});    // -5th(7)
    ns.setHarmonicShift(-3); CHECK_LIST(ns.selectedNotes(), {1, 2, 3, 5, 6, 8, 9, 10, 11});       // -3rd(4)
    ns.setHarmonicShift(-4); CHECK_LIST(ns.selectedNotes(), {1, 2, 3, 5, 6, 8, 9, 10});           // -7th(11)
    ns.setHarmonicShift(-5); CHECK_LIST(ns.selectedNotes(), {1, 3, 5, 6, 8, 9, 10});              // -9th(2)
    ns.setHarmonicShift(-6); CHECK_LIST(ns.selectedNotes(), {1, 3, 5, 6, 8, 10});                 // -13th(9)
    ns.setHarmonicShift(-7); CHECK_LIST(ns.selectedNotes(), {1, 3, 5, 8, 10});                    // fills only
}
TEST(notes_harmonicshift_clamp) {
    NoteSelector ns;
    for (int i = 0; i < 7; i++) ns.select[i] = true;
    ns.setHarmonicShift(99);  CHECK(ns.harmonicshift == 7);
    ns.setHarmonicShift(-99); CHECK(ns.harmonicshift == -7);
}

// ---- shift walking (selected first, then scale; octave crossings; clamp) ---
TEST(notes_shift_selected_scale_notes) {
    NoteSelector ns;  // default: selected == scale == {0,2,4,6,7,9,11}
    CHECK(ns.shiftNote(0, 1, 0) == 2);    // +1 selected step: C -> D
    CHECK(ns.shiftNote(0, 7, 0) == 12);   // +7 selected steps wraps one octave up
    CHECK(ns.shiftNote(0, -1, 0) == -1);  // -1 selected step: C -> B below (pc 11)
    CHECK(ns.shiftNote(0, 0, 1) == 2);    // +1 scale step behaves the same here
}
TEST(notes_shift_order_selected_then_scale) {
    NoteSelector ns;  // triad selected {0,4,7}; scale {0,2,4,6,7,9,11}
    ns.select[0] = ns.select[1] = ns.select[2] = true;
    CHECK(ns.shiftNote(0, 1, 0) == 4);    // selected: C -> E
    CHECK(ns.shiftNote(4, 0, 1) == 6);    // scale: E -> F♯
    // combined: selectnoteshift first (0->4), then noteshift (4->6) == 6.
    // Reversed order would give 4 (0->2 scale, then floor-to-0 selected +1 ->4).
    CHECK(ns.shiftNote(0, 1, 1) == 6);
}
TEST(notes_shift_clamp_24) {
    NoteSelector ns;
    CHECK(ns.shiftNote(0, 100, 0) == ns.shiftNote(0, 24, 0));    // sns clamps to +24
    CHECK(ns.shiftNote(0, 0, -100) == ns.shiftNote(0, 0, -24));  // nos clamps to -24
}

// ---- quantize: snap tie-up, direction search, border rule ------------------
static const std::vector<int> kLydian = {0, 2, 4, 6, 7, 9, 11};
static const std::vector<int> kChromatic = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const std::vector<int> kTriad = {0, 4, 7};

TEST(notes_quantize_snap_tie_up) {
    // exact half-semitone snaps upward (decision 1); chromatic -> no search.
    CHECK_NEAR(quantize(0.5f * kSemitoneUnit, kChromatic, 1), 1.0f * kSemitoneUnit, 1e-6);
    CHECK_NEAR(quantize(0.4f * kSemitoneUnit, kChromatic, 1), 0.0f, 1e-6);
    CHECK_NEAR(quantize(0.6f * kSemitoneUnit, kChromatic, 1), 1.0f * kSemitoneUnit, 1e-6);
}
TEST(notes_quantize_direction_up_down) {
    // C♯ (semitone 1, pc1) is not in lydian; up -> D(2), down -> C(0).
    float in = 1.0f * kSemitoneUnit;
    CHECK_NEAR(quantize(in, kLydian, 1), 2.0f * kSemitoneUnit, 1e-6);
    CHECK_NEAR(quantize(in, kLydian, 2), 0.0f, 1e-6);
    CHECK_NEAR(quantize(in, kLydian, 0), 2.0f * kSemitoneUnit, 1e-6); // equidistant, tie up
}
TEST(notes_quantize_direction_nearest_asymmetric) {
    // triad {0,4,7}: nearest picks the genuinely closer allowed note.
    CHECK_NEAR(quantize(1.0f * kSemitoneUnit, kTriad, 0), 0.0f, 1e-6);            // 1: down to 0
    CHECK_NEAR(quantize(5.0f * kSemitoneUnit, kTriad, 0), 4.0f * kSemitoneUnit, 1e-6); // 5: down to 4
    CHECK_NEAR(quantize(3.0f * kSemitoneUnit, kTriad, 0), 4.0f * kSemitoneUnit, 1e-6); // 3: up to 4
    CHECK_NEAR(quantize(2.0f * kSemitoneUnit, kTriad, 0), 4.0f * kSemitoneUnit, 1e-6); // 2: tie up to 4
}
TEST(notes_quantize_passthrough_when_allowed) {
    CHECK_NEAR(quantize(0.0f, kLydian, 1), 0.0f, 1e-6);
    CHECK_NEAR(quantize(2.0f * kSemitoneUnit, kLydian, 1), 2.0f * kSemitoneUnit, 1e-6);
}
TEST(notes_quantize_border_rule) {
    // allowed pc5 only; searching up from semitone 116 finds no pc5 before +120,
    // so the +10 V border voltage is output; likewise downward -> -10 V.
    std::vector<int> onlyF = {5};
    CHECK_NEAR(quantize(116.0f * kSemitoneUnit, onlyF, 1), 1.0f, 1e-6);
    CHECK_NEAR(quantize(-116.0f * kSemitoneUnit, onlyF, 2), -1.0f, 1e-6);
}
TEST(notes_quantize_empty_allowed) {
    // no allowed notes -> snapped semitone, clamped to the border.
    std::vector<int> none;
    CHECK_NEAR(quantize(2.4f * kSemitoneUnit, none, 1), 2.0f * kSemitoneUnit, 1e-6);
}

// ---- output stage ---------------------------------------------------------
TEST(notes_apply_tuning) {
    CHECK_NEAR(applyTuning(false, 0.3f, 0.1f, 0.02f), 0.12f, 1e-6);  // quant + transpose
    CHECK_NEAR(applyTuning(true, 0.3f, 0.1f, 0.02f), 0.3f, 1e-6);    // tuningpitch only
}
