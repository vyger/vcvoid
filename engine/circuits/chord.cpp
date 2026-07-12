// chord — Chord generator: up to four voices of a musical chord.
// Spec: manual/circuits/chord.md. Builds a chord from the shared Sinfonion note
// core (engine/src/notes.*): root/degree scale, select1..13/selectfill1..5
// interval selection (smart default = all seven scale notes), harmonicshift,
// noteshift/selectnoteshift, and the tuning/transpose output stage — all reused
// verbatim from minifonion/arpeggio. What is chord-specific:
//   * Voices are built from the notes in INTERVAL (select-input) order via
//     NoteSelector::orderedSelectedNotes() — so the default C lydian gives
//     root/3rd/5th/7th = C,E,G,B (a maj7), NOT the first four pitch-sorted
//     scale steps.
//   * Voice count V = the highest connected output (output4 -> 4-voice; omit it
//     for 3-voice; only output1/2 -> 2-voice; 2-voice reads just the first two
//     selected notes).
//   * Doubling fills V voices when fewer notes are selected (manual rules):
//     4-voice k1 -> [S0,S0,S0,S0]; k2 -> [S0,S1,S0,S1]; k3 -> [S0,S1,S2,S0];
//     k>=4 -> first four. 3-voice k1 -> [S0]x3; k2 -> [S0,S1,S0]; k>=3 -> first
//     three (no duplication). 2-voice k1 -> [S0,S0]; k>=2 -> [S0,S1].
//   * Voicing (octave placement): the lowest note is the chord note nearest at
//     or above `pitch`; each voice keeps its own pitch class. inversion 0 lets
//     any note be lowest (closest to pitch); inversion m forces the m-th
//     selected note lowest. spread sets the max lowest->highest span
//     (12 + spread semitones); with enough spread, doubled voices separate into
//     higher octaves.
//   * trigger is a sample & hold: with it patched the whole chord is recomputed
//     only on a rising edge and frozen between.
//
// SPEC-GAPs (manual silent / flagged by verification_note; literal readings,
// documented at each site): the exact even-spacing distribution across a >1-
// octave spread is NOT modelled — only octave-separation of doubled voices is
// (non-doubled voices stay in the base octave); output identity is tied to the
// select-order voice (not re-sorted ascending), matching the k2 doubling rule;
// inversion clamps to the number of selected notes; pre-first-trigger output is
// held 0 V; an empty allowed set (e.g. harmonicshift -7) places no voice, so
// each output holds 0 V (+ transpose).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include "../src/notes.hpp"
#include <cmath>
#include <cstdint>
#include <vector>

namespace droid {

class Chord : public Circuit {
public:
    void tick(EngineState& s) override {
        int V = voiceCount();

        bool tuning = gateHigh("tuningmode", s);
        float transpose = in("transpose").value(s);

        // Triggered mode: recompute the chord only on a rising edge at `trigger`.
        bool trigConn  = in("trigger").connected();
        bool trigFired = trigConn && trigGate_.risingEdge(in("trigger").value(s));
        bool recompute = trigConn ? trigFired : true;

        if (recompute && V > 0) computeVoices(s, V);

        // Emit. tuningmode overrides every output with tuningpitch; otherwise the
        // held per-voice note + transpose. Unconnected outputs are simply not set.
        float tp = in("tuningpitch").value(s);
        for (int v = 0; v < 4; v++) {
            if (!out("output", v + 1).connected()) continue;
            float note = (v < (int)held_.size()) ? held_[v] : 0.0f;
            out("output", v + 1).set(s, tuning ? tp : note + transpose);
        }
    }

private:
    static int clampInt(int v, int lo, int hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
    bool gateHigh(const char* name, EngineState& s) {
        return in(name).value(s) >= kGateHighThreshold;
    }

    // Voice count from the highest connected output jack (1..4), 0 if none.
    int voiceCount() {
        int v = 0;
        for (int i = 1; i <= 4; i++) if (out("output", i).connected()) v = i;
        return v;
    }

    // Doubling: expand the ordered selected notes S (length k) to exactly V
    // voice pitch classes, per the manual's rules.
    static std::vector<int> doubleVoices(const std::vector<int>& S, int V) {
        int k = (int)S.size();
        std::vector<int> P;
        if (k == 0 || V == 0) return P;
        auto at = [&](int i) { return S[i]; };
        if (V >= 4) {
            if (k == 1)      P = {at(0), at(0), at(0), at(0)};
            else if (k == 2) P = {at(0), at(1), at(0), at(1)};
            else if (k == 3) P = {at(0), at(1), at(2), at(0)};
            else             P = {at(0), at(1), at(2), at(3)};
        } else if (V == 3) {
            if (k == 1)      P = {at(0), at(0), at(0)};
            else if (k == 2) P = {at(0), at(1), at(0)};   // SPEC-GAP: manual silent
            else             P = {at(0), at(1), at(2)};
        } else if (V == 2) {
            if (k == 1)      P = {at(0), at(0)};
            else             P = {at(0), at(1)};
        } else {             // V == 1
            P = {at(0)};
        }
        return P;
    }

    void computeVoices(EngineState& s, int V) {
        NoteSelector ns;
        ns.setRoot((int)std::lround(in("root").value(s)));
        ns.setDegree((int)std::lround(in("degree").value(s)));
        ns.setHarmonicShift((int)std::lround(in("harmonicshift").value(s)));
        static const char* kSel[7] = {"select1", "select3", "select5", "select7",
                                      "select9", "select11", "select13"};
        for (int i = 0; i < 7; i++) ns.select[i] = gateHigh(kSel[i], s);
        static const char* kFill[5] = {"selectfill1", "selectfill2", "selectfill3",
                                       "selectfill4", "selectfill5"};
        for (int i = 0; i < 5; i++) ns.fill[i] = gateHigh(kFill[i], s);

        std::vector<int> S = ns.orderedSelectedNotes();
        std::vector<int> P = doubleVoices(S, V);
        held_.assign(V, 0.0f);
        if (P.empty()) return;   // no allowed note -> hold 0 V (+ transpose later)

        double pitchSemi = (double)in("pitch").value(s) / (double)kSemitoneUnit;
        float spreadV = in("spread").value(s);
        if (spreadV < 0.0f) spreadV = 0.0f;
        double allowed = 12.0 + (double)spreadV / (double)kSemitoneUnit; // span
        const double eps = 1e-6;

        // nearest semitone at or above `pitchSemi` with pitch class `pc`.
        auto nearestAbove = [&](int pc) {
            double d = std::ceil((pitchSemi - eps - (double)pc) / 12.0);
            long n = (long)std::llround(d) * 12 + pc;
            while ((double)n < pitchSemi - eps) n += 12;
            return n;
        };

        // Lowest pitch class: inversion 0 -> whichever gives the lowest note;
        // inversion m -> the m-th selected note (clamped to what is available).
        int inversion = (int)std::lround(in("inversion").value(s));
        int Lpc;
        long low;
        if (inversion > 0) {
            int idx = clampInt(inversion, 1, (int)P.size()) - 1;
            Lpc = P[idx];
            low = nearestAbove(Lpc);
        } else {
            Lpc = P[0];
            low = nearestAbove(P[0]);
            for (size_t i = 1; i < P.size(); i++) {
                long n = nearestAbove(P[i]);
                if (n < low) { low = n; Lpc = P[i]; }
            }
        }

        // Place each voice within [low, low+allowed]. A voice sits at the first
        // octave of its pitch class at or above `low`; if that exact semitone is
        // already taken (a doubled voice) it rises an octave, but only while
        // another octave still fits inside the allowed span (so at spread 0 the
        // doubles stay as unisons within one octave).
        std::vector<long> used;
        const int B = kPitchBorderSemis;
        for (int v = 0; v < V; v++) {
            long base = low + (((long)P[v] - Lpc) % 12 + 12) % 12;
            long semi = base;
            auto taken = [&](long x) {
                for (long u : used) if (u == x) return true;
                return false;
            };
            while (taken(semi) && (double)(semi - low + 12) < allowed - 1e-9)
                semi += 12;
            used.push_back(semi);

            // noteshift / selectnoteshift walk the shared lattice, then clamp.
            int sns = (int)std::lround(in("selectnoteshift").value(s));
            int nos = (int)std::lround(in("noteshift").value(s));
            if (sns != 0 || nos != 0) semi = ns.shiftNote((int)semi, sns, nos);
            if (semi > B)  semi = B;
            if (semi < -B) semi = -B;
            held_[v] = (float)((double)semi * (double)kSemitoneUnit);
        }
    }

    GateReader trigGate_;
    std::vector<float> held_;   // per-voice held pitch (pre-transpose)
};

DROID_REGISTER_CIRCUIT(chord, Chord)

} // namespace droid
