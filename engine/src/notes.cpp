#include "notes.hpp"
#include "../gen/scales.gen.hpp"
#include <algorithm>
#include <cmath>

namespace droid {

namespace {

// select[] index -> storage column (kScales column order [I,II,III,IV,V,VI,VII,
// fill1..5] = 0..11). select1->I(0), select3->III(2), select5->V(4),
// select7->VII(6), select9->II(1), select11->IV(3), select13->VI(5).
constexpr int kSelectCol[7] = {0, 2, 4, 6, 1, 3, 5};

int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Disable columns per the harmonicshift table (manual/circuits/minifonion.md).
// harmonicshift only removes; it never adds. Positive values strip fills first,
// then the complex degrees (11th,13th,9th,7th,3rd,5th) leaving the root; negative
// values strip the simple degrees (root,5th,3rd,7th,9th,13th,11th) leaving fills.
void applyHarmonicShift(bool col[12], int h) {
    if (h > 0) {
        for (int i = 7; i < 12; i++) col[i] = false;           // h>=1: all fills
        // cumulative degree columns for h>=2: 11th,13th,9th,7th,3rd,5th
        static const int order[6] = {3, 5, 1, 6, 2, 4};        // IV,VI,II,VII,III,V
        for (int i = 0; i < h - 1 && i < 6; i++) col[order[i]] = false;
    } else if (h < 0) {
        // cumulative degree columns: root,5th,3rd,7th,9th,13th,11th (fills kept)
        static const int order[7] = {0, 4, 2, 6, 1, 5, 3};     // I,V,III,VII,II,VI,IV
        int cnt = -h;
        for (int i = 0; i < cnt && i < 7; i++) col[order[i]] = false;
    }
}

// Sort ascending + drop duplicates (defensive; a scale row's pcs are distinct).
std::vector<int> pitchOrder(std::vector<int> pcs) {
    std::sort(pcs.begin(), pcs.end());
    pcs.erase(std::unique(pcs.begin(), pcs.end()), pcs.end());
    return pcs;
}

// Shift a semitone value by `steps` positions along a pitch-ordered pc list,
// octave-replicated. steps==0 is a strict no-op (so noteshift/selectnoteshift
// default 0 never disturbs a note whose pitch class is off-list). When the pc
// is not a member of the list (SPEC-GAP: e.g. noteshift applied after
// selectnoteshift landed on a fill, which is not a scale note), the shift
// starts from the largest list member at or below the pc — the most literal
// "walk the list from here" reading.
long shiftAlong(long semitone, int steps, const std::vector<int>& list) {
    if (steps == 0 || list.empty()) return semitone;
    int k = (int)list.size();
    int pc = (int)(((semitone % 12) + 12) % 12);
    long base = semitone - pc;   // octave base (a multiple of 12)
    int idx = -1;
    for (int i = 0; i < k; i++) if (list[i] == pc) { idx = i; break; }
    if (idx < 0) {
        int f = -1;
        for (int i = 0; i < k; i++) if (list[i] <= pc) f = i;
        if (f < 0) { idx = k - 1; base -= 12; }   // pc below all members
        else idx = f;
    }
    long newIndex = (long)idx + steps;
    long octave = (long)std::floor((double)newIndex / (double)k);
    int within = (int)(newIndex - octave * k);
    return base + octave * 12 + list[within];
}

// Build the 12 "column on" flags shared by selectedNotes()/orderedSelectedNotes():
// the smart default (nothing selected -> the seven scale columns) followed by
// harmonicshift filtering.
void buildColumns(const NoteSelector& ns, bool col[12]) {
    for (int c = 0; c < 12; c++) col[c] = false;
    bool any = false;
    for (int i = 0; i < 7; i++) if (ns.select[i]) any = true;
    for (int i = 0; i < 5; i++) if (ns.fill[i]) any = true;
    if (!any) {
        for (int c = 0; c < 7; c++) col[c] = true;   // smart default: 7 scale notes
    } else {
        for (int i = 0; i < 7; i++) if (ns.select[i]) col[kSelectCol[i]] = true;
        for (int i = 0; i < 5; i++) if (ns.fill[i])   col[7 + i] = true;
    }
    applyHarmonicShift(col, ns.harmonicshift);
}

} // namespace

void NoteSelector::setRoot(int r)          { root = clampInt(r, 0, 12); }
void NoteSelector::setDegree(int d)        { degree = clampInt(d, 0, 107); }
void NoteSelector::setHarmonicShift(int h) { harmonicshift = clampInt(h, -7, 7); }

std::vector<int> NoteSelector::selectedNotes() const {
    bool col[12];
    buildColumns(*this, col);
    int rp = rootPc();
    std::vector<int> pcs;
    for (int c = 0; c < 12; c++)
        if (col[c]) pcs.push_back((gen::kScales[degree][c] + rp) % 12);
    return pitchOrder(std::move(pcs));
}

std::vector<int> NoteSelector::orderedSelectedNotes() const {
    bool col[12];
    buildColumns(*this, col);
    int rp = rootPc();
    // Interval / select-input order: select1,3,5,7,9,11,13 map to columns
    // I,III,V,VII,II,IV,VI = {0,2,4,6,1,3,5}; then the five fills = 7..11.
    static const int kOrder[12] = {0, 2, 4, 6, 1, 3, 5, 7, 8, 9, 10, 11};
    std::vector<int> pcs;
    for (int i = 0; i < 12; i++) {
        int c = kOrder[i];
        if (col[c]) pcs.push_back((gen::kScales[degree][c] + rp) % 12);
    }
    return pcs;   // interval order preserved; not sorted, not deduped
}

std::vector<int> NoteSelector::scaleNotes() const {
    int rp = rootPc();
    std::vector<int> pcs;
    for (int c = 0; c < 7; c++) pcs.push_back((gen::kScales[degree][c] + rp) % 12);
    return pitchOrder(std::move(pcs));
}

int NoteSelector::shiftNote(int semitone, int sns, int nos) const {
    sns = clampInt(sns, -24, 24);
    nos = clampInt(nos, -24, 24);
    long s = semitone;
    s = shiftAlong(s, sns, selectedNotes());   // selected notes first
    s = shiftAlong(s, nos, scaleNotes());       // then scale notes
    return (int)s;
}

float quantize(float pitchCv, const std::vector<int>& allowed, int direction) {
    const int B = kPitchBorderSemis;
    double n = (double)pitchCv / (double)kSemitoneUnit;
    long s0 = (long)std::floor(n + 0.5);         // nearest semitone, ties up
    s0 = s0 > B ? B : (s0 < -B ? -B : s0);        // stay in ±10 V

    auto pcOf   = [](long s) { return (int)(((s % 12) + 12) % 12); };
    auto isOk   = [&](long s) {
        int pc = pcOf(s);
        for (int a : allowed) if (a == pc) return true;
        return false;
    };
    auto volts  = [](long s) { return (float)((double)s * (double)kSemitoneUnit); };

    if (allowed.empty()) return volts(s0);        // nothing allowed -> snapped/clamped
    if (isOk(s0)) return volts(s0);

    if (direction == 1) {                         // search upward
        for (long s = s0 + 1; s <= B; s++) if (isOk(s)) return volts(s);
        return volts(B);                          // border voltage
    }
    if (direction == 2) {                         // search downward
        for (long s = s0 - 1; s >= -B; s--) if (isOk(s)) return volts(s);
        return volts(-B);
    }
    // direction 0: nearest, ties up (check the upward candidate first).
    for (long d = 1; ; d++) {
        long up = s0 + d, dn = s0 - d;
        bool upIn = up <= B, dnIn = dn >= -B;
        if (!upIn && !dnIn) break;
        if (upIn && isOk(up)) return volts(up);
        if (dnIn && isOk(dn)) return volts(dn);
    }
    return volts(s0);                             // unreachable for non-empty in-range
}

float applyTuning(bool tuningMode, float tuningPitch, float quantized, float transpose) {
    return tuningMode ? tuningPitch : quantized + transpose;
}

} // namespace droid
