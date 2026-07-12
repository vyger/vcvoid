// arpeggio — Arpeggiator: pattern-based melody generator.
// Spec: manual/circuits/arpeggio.md. Each clock advances a rule-based walk over
// a pool of "allowed notes" and emits its 1 V/oct pitch. The scale/root/interval
// selection, harmonicshift, note-shifting and the tuning/transpose output stage
// are the shared Sinfonion-family core (engine/src/notes.*), identical to
// minifonion and chord; this circuit adds the pattern engine, the pitch/range
// window, direction/pingpong/butterfly/drop/octaves, startnote and autoreset.
//
// Note pool: the allowed pitch classes (selectedNotes(), octave-replicated) are
// realised as concrete semitones inside the window [pitch, pitch+range]. The
// first note is the lowest allowed note at or above `pitch`; the window's upper
// bound is inclusive (matching the manual's 15-note two-octave example, C thrice
// at 0/12/24). `range` is clamped to [0, 0.8] (8 octaves). A range of 0 leaves a
// single note. Notes are bounded to the +/-10 V (+/-120 semitone) border.
//
// Ordering of transforms (all before the pattern walks): selection -> range
// window (ascending) -> drop (skip scheme) -> butterfly (end-interleave). The
// pattern indexes into that final ordered list `seq`.
//
// Patterns 0-4 are deterministic index-delta cycles; on a step that would leave
// [0, M-1] the walk either restarts at the start note (no pingpong) or reverses
// (pingpong). Patterns 5 (random +/-1 walk) and 6 (random jump to another note)
// draw from the shared engine RNG, once per note, only on a clock edge.
//
// Literal readings taken where the manual is silent (SPEC-GAP), documented at
// each site: pool boundary inclusivity; direction/startnote/pingpong expressed
// in `seq` index space (so they interact literally with butterfly/drop); the
// exact restart point when a multi-step delta overshoots the end; autoreset
// counts main notes only (octave copies don't count); reset is processed before
// clock within a tick; tuningmode overrides only the output (the pattern keeps
// its state); an empty allowed set emits no note (output holds 0 V + transpose).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include "../src/notes.hpp"
#include "../src/rng.hpp"
#include "../gen/scales.gen.hpp"
#include <cmath>
#include <cstdint>
#include <climits>
#include <vector>

namespace droid {

class Arpeggio : public Circuit {
public:
    void tick(EngineState& s) override {
        // Reset is processed before the clock within a tick (SPEC-GAP: only the
        // general "reset restarts" behaviour is documented). A simultaneous
        // reset+clock therefore lands on the start note. Reset also zeroes the
        // autoreset counter (manual-stated).
        if (in("reset").connected() &&
            resetGate_.risingEdge(in("reset").value(s))) {
            started_ = false;
            arCount_ = 0;
            phase_ = 0;
            pong_ = 1;
            emitOctave_ = false;
        }

        int octaves = clampInt((int)std::lround(in("octaves").value(s)), 0, 2);

        bool clockFired = in("clock").connected() &&
                          clockGate_.risingEdge(in("clock").value(s));
        if (clockFired) {
            if (octaves != 0 && emitOctave_) {
                // Octave copy of the last main note. Ignores `range`, does not
                // advance the pattern, and does not count toward autoreset.
                float oct = (octaves == 1) ? kOctaveUnit : -kOctaveUnit;
                heldVolts_ = clampVolts(octaveBaseVolts_ + oct);
                emitOctave_ = false;
            } else {
                NoteSelector ns;
                std::vector<int> seq = buildSeq(s, ns);
                if (!seq.empty()) {
                    bool dirDown = gateHigh("direction", s);
                    int startIdx = computeStartIdx(s, seq, ns, dirDown);
                    int pattern = clampInt(
                        (int)std::lround(in("pattern").value(s)), 0, 6);
                    bool pingpong = gateHigh("pingpong", s);
                    mainStep(s, seq, startIdx, pattern, pingpong, dirDown);

                    int semi = seq[mainIdx_];
                    int sns = (int)std::lround(in("selectnoteshift").value(s));
                    int nos = (int)std::lround(in("noteshift").value(s));
                    if (sns != 0 || nos != 0) semi = ns.shiftNote(semi, sns, nos);
                    if (semi > kPitchBorderSemis)  semi = kPitchBorderSemis;
                    if (semi < -kPitchBorderSemis) semi = -kPitchBorderSemis;

                    float v = (float)((double)semi * (double)kSemitoneUnit);
                    heldVolts_ = v;
                    octaveBaseVolts_ = v;
                    if (octaves != 0) emitOctave_ = true;
                }
                // Empty pool (e.g. harmonicshift -7 with no fills): no note is
                // emitted; heldVolts_ keeps its previous value (0 initially).
            }
        }

        // Shared output stage (see notes.hpp / minifonion): tuningmode outputs
        // tuningpitch, overriding everything; otherwise held note + transpose,
        // both applied live so `transpose` works between clocks (vibrato).
        if (gateHigh("tuningmode", s))
            out("output").set(s, in("tuningpitch").value(s));
        else
            out("output").set(s, heldVolts_ + in("transpose").value(s));
    }

private:
    static int clampInt(int v, int lo, int hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
    static float clampVolts(float v) {
        const float b = kPitchBorderSemis * kSemitoneUnit;
        return v < -b ? -b : (v > b ? b : v);
    }
    bool gateHigh(const char* name, EngineState& s) {
        return in(name).value(s) >= kGateHighThreshold;
    }

    // Build the ordered note list the pattern walks: selection -> window ->
    // drop -> butterfly. Returns concrete semitone values (ascending before
    // butterfly). Also fills `ns` for startnote pitch-class and note shifting.
    std::vector<int> buildSeq(EngineState& s, NoteSelector& ns) {
        ns.setRoot((int)std::lround(in("root").value(s)));
        ns.setDegree((int)std::lround(in("degree").value(s)));
        ns.setHarmonicShift((int)std::lround(in("harmonicshift").value(s)));
        static const char* kSel[7] = {"select1", "select3", "select5", "select7",
                                      "select9", "select11", "select13"};
        for (int i = 0; i < 7; i++) ns.select[i] = gateHigh(kSel[i], s);
        static const char* kFill[5] = {"selectfill1", "selectfill2", "selectfill3",
                                       "selectfill4", "selectfill5"};
        for (int i = 0; i < 5; i++) ns.fill[i] = gateHigh(kFill[i], s);
        std::vector<int> pcs = ns.selectedNotes();

        float rangeV = in("range").value(s);
        if (rangeV < 0.0f)  rangeV = 0.0f;
        if (rangeV > 0.8f)  rangeV = 0.8f;   // manual: max range 8 octaves
        double lowB  = (double)in("pitch").value(s) / (double)kSemitoneUnit;
        double highB = lowB + (double)rangeV / (double)kSemitoneUnit;
        const double eps = 0.01;             // guards float error at boundaries
        const int B = kPitchBorderSemis;

        auto pcIn = [&](int n) {
            int pc = ((n % 12) + 12) % 12;
            for (int a : pcs) if (a == pc) return true;
            return false;
        };

        std::vector<int> pool;
        if (!pcs.empty()) {
            // First note: lowest allowed lattice note at or above `pitch`.
            int scanStart = (int)std::floor(lowB - eps) - 12;
            if (scanStart < -B) scanStart = -B;
            int firstNote = INT_MIN;
            for (int n = scanStart; n <= B; n++)
                if (pcIn(n) && (double)n >= lowB - eps) { firstNote = n; break; }
            if (firstNote != INT_MIN) {
                double top = highB > (double)firstNote ? highB : (double)firstNote;
                for (int n = firstNote; n <= B && (double)n <= top + eps; n++)
                    if (pcIn(n)) pool.push_back(n);
            }
        }

        int drop = clampInt((int)std::lround(in("drop").value(s)), 0, 3);
        std::vector<int> dropped;
        for (size_t i = 0; i < pool.size(); i++) {
            bool keep = true;
            switch (drop) {
                case 1: keep = (i % 2) == 0; break;   // skip every 2nd
                case 2: keep = (i % 3) != 2; break;   // skip every 3rd
                case 3: keep = (i % 3) == 0; break;   // skip 2nd & 3rd of each 3
                default: break;
            }
            if (keep) dropped.push_back(pool[i]);
        }

        if (gateHigh("butterfly", s) && !dropped.empty()) {
            int M0 = (int)dropped.size();
            std::vector<int> seq;
            seq.reserve(M0);
            for (int i = 0; i < M0; i++) {
                int idx = (i % 2 == 0) ? i / 2 : M0 - 1 - i / 2;
                seq.push_back(dropped[idx]);
            }
            return seq;
        }
        return dropped;
    }

    int computeStartIdx(EngineState& s, const std::vector<int>& seq,
                        const NoteSelector& ns, bool dirDown) {
        int M = (int)seq.size();
        if (M == 0) return 0;
        int startnote = clampInt((int)std::lround(in("startnote").value(s)), 0, 7);
        if (startnote == 0) return dirDown ? M - 1 : 0;
        // Pitch class of the startnote-th scale degree (I..VII, degree-order).
        int pc = ((gen::kScales[ns.degree][startnote - 1] + ns.rootPc()) % 12 + 12) % 12;
        if (dirDown) {
            for (int i = M - 1; i >= 0; i--)
                if (((seq[i] % 12) + 12) % 12 == pc) return i;
            return M - 1;
        }
        for (int i = 0; i < M; i++)
            if (((seq[i] % 12) + 12) % 12 == pc) return i;
        return 0;
    }

    void mainStep(EngineState& s, const std::vector<int>& seq, int startIdx,
                  int pattern, bool pingpong, bool dirDown) {
        int M = (int)seq.size();
        if (M <= 0) return;
        int g = dirDown ? -1 : 1;
        int autoreset = (int)std::lround(in("autoreset").value(s));
        if (autoreset < 0) autoreset = 0;

        auto clampIdx = [&]() {
            if (mainIdx_ >= M) mainIdx_ = M - 1;
            if (mainIdx_ < 0)  mainIdx_ = 0;
        };

        if (!started_) {
            started_ = true;
            mainIdx_ = startIdx; phase_ = 0; pong_ = 1; arCount_ = 1;
            clampIdx();
            return;
        }
        clampIdx();   // selection may have shrunk the pool since last step

        arCount_++;
        if (autoreset > 0 && arCount_ > autoreset) {
            mainIdx_ = startIdx; phase_ = 0; pong_ = 1; arCount_ = 1;
            clampIdx();
            return;
        }
        if (M == 1) { mainIdx_ = 0; return; }

        // Deterministic patterns: index-delta cycles (base = upward structure).
        static const int kDeltas[5][4] = {
            {+1,  0,  0,  0},   // 0: step forward
            {+1, +1, -1,  0},   // 1: two forward, one back
            {+2, -1,  0,  0},   // 2: double forward, one back
            {+2, -2, +1,  0},   // 3: double fwd, double back, single fwd
            {+2, +1, -2, +1},   // 4: double fwd, single fwd, double back, single fwd
        };
        static const int kLen[5] = {1, 3, 2, 3, 4};

        if (pattern >= 0 && pattern <= 4) {
            int delta = kDeltas[pattern][phase_] * g * pong_;
            int nxt = mainIdx_ + delta;
            if (nxt < 0 || nxt > M - 1) {
                if (pingpong) {
                    pong_ = -pong_;   // reverse at the boundary (triangle)
                    int d2 = kDeltas[pattern][phase_] * g * pong_;
                    nxt = mainIdx_ + d2;
                    if (nxt < 0) nxt = 0;
                    if (nxt > M - 1) nxt = M - 1;
                    mainIdx_ = nxt;
                    phase_ = (phase_ + 1) % kLen[pattern];
                } else {
                    // Restart at the start note. A multi-step delta may overshoot
                    // the end and restart early (SPEC-GAP: literal reading). Does
                    // not touch arCount_ (a natural pattern loop still counts).
                    mainIdx_ = startIdx; phase_ = 0; pong_ = 1;
                    clampIdx();
                }
            } else {
                mainIdx_ = nxt;
                phase_ = (phase_ + 1) % kLen[pattern];
            }
        } else if (pattern == 5) {
            // Random +/-1 walk; direction/pingpong have no effect. One draw per
            // note. At a boundary the step reflects (bounces back).
            int step = (randUniform(s.rngState) < 0.5f) ? -1 : 1;
            int nxt = mainIdx_ + step;
            if (nxt < 0 || nxt > M - 1) nxt = mainIdx_ - step;
            if (nxt < 0) nxt = 0;
            if (nxt > M - 1) nxt = M - 1;
            mainIdx_ = nxt;
        } else {
            // Pattern 6: random jump to any OTHER allowed note. One draw per note.
            uint32_t r = nextRand(s.rngState);
            int j = (int)(r % (uint32_t)(M - 1));
            if (j >= mainIdx_) j++;   // skip the current index
            mainIdx_ = j;
        }
    }

    GateReader clockGate_, resetGate_;
    bool  started_ = false;
    int   mainIdx_ = 0;
    int   phase_ = 0;
    int   pong_ = 1;             // pingpong sign (+1/-1)
    long  arCount_ = 0;          // autoreset: main notes since last reset
    bool  emitOctave_ = false;   // next clock emits the octave copy
    float octaveBaseVolts_ = 0.0f;
    float heldVolts_ = 0.0f;     // last emitted note (pre-transpose)
};

DROID_REGISTER_CIRCUIT(arpeggio, Arpeggio)

} // namespace droid
