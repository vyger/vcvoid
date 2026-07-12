// algoquencer — Algorithmic performance sequencer (Turing-machine random CVs +
// button-pattern drum sequencer with fills/morphs/branches/rolls). Spec:
// manual/circuits/algoquencer.md. Property-faithful design: match every
// manual-pinned invariant and invent the unspecified internals; executable
// reference for every invented choice is the algoref.py replica cited by
// tests/golden/algoquencer/*.gold.
//
// Random model: all deterministic decisions come from a splitmix-style hash
// h(seed, salt, step, kind, gen). seed = pattern input + an offset moved by
// nextpattern/prevpattern (+/-1) and reroll (:= engine-RNG draw & 0xFFFF).
// A step is deterministic iff hUniform(seed,0,step,K_CLASS,0) < dejavu —
// monotone in dejavu, so sweeping 1->0 flips steps to chaotic in a fixed
// order. Chaotic steps draw 6 uniforms from the engine RNG once per bar
// (ascending step, kinds PITCH,VARREM,VARADD,ACT,FILL,ROLL) into a frozen
// table; the bar's beat table is then a PURE function of the current inputs
// and is recomputed at every clock edge (params act immediately).
//
// Bar pipeline: base = buttons XOR alternate page (on alternate bars) ->
// variation (move round(v*beats), count preserved) -> activity (<0.5 keep
// round(2a*beats); >0.5 add round((2a-1)*empties); no buttons: target
// round(a*len)) -> fills -> per-beat roll flags. Weighted placement uses
// Efraimidis–Spirakis keys u^(1/w), w = offbeats/distribution multiplicative
// tilts (0.5 = flat), tie-break ascending step; removal takes lowest u.
//
// Conventions / SPEC-GAPs (manual silent; goldens pin the replica's choice):
//   * the PRNG hash itself, the dejavu classification, the E-S placement
//     scheme, and pitch quantization floor(u*res)/(res-1) (random.cpp's rule).
//   * morphs: accumulator rate = max((10m)^2/100, 1/64) per step (resolves the
//     manual's 1-per-100-table vs 1-per-64-prose conflict); selection among
//     deterministic steps by hash keyed 0x100+bar; a morphed step's gen counter
//     bumps (re-rolled decisions stay re-rolled). With branches, morphs land
//     only at branch-cycle starts. Seed changes zero the gen counters.
//   * branches: bar b -> 1 (even) or 2+min(ctz((b+1)/2), branches-1) (ruler
//     sequence A B A C ...); the branch salts the hash iff dejavu > 0.
//   * fills: extra = max(1, round(fills*factor*len/4)); factors — fillorder 0:
//     every bar 1.0; 1: every 2nd bar 1.0; 2: bar2 0.5 / bar4 1.0; 3: bars 2,6
//     0.25 / bar4 0.5 / bar8 1.0 (1-based bars). End-bias: weight *= (j+1)/len.
//     If empties run out, existing beats are flagged as fill beats instead.
//   * rolls: n = rollcount*rollsteps pre-beats spread over the rollsteps ticks
//     before the beat; velocity v_i = rsv + (1-rsv)*i/n (main beat 1.0); gate
//     width gatelength*period/n. Scheduled one step ahead (pre-beats earlier
//     than one step are dropped for rollsteps > 1); no roll leads into the
//     very first bar.
//   * gate: no gate until the clock period is measured; widths are >= 1 tick;
//     gatelength >= 1 gives legato via abutting windows.
//   * LEDs: normal view = max(step bit, 0.5*cursor); cursor = most recently
//     played step, dark after reset until the next clock (manual pins the reset
//     rule); length/accent/alternate views show pure page bits; length mode
//     lights steps < length. barled_i = (bar % 4 == i). unmuteled blink = 2 Hz
//     square (quarter-second segments of the tick counter).
//   * select gates the step/length/accent/alternate buttons, clearpage and
//     led1..16; mutebutton/unmutebutton + their LEDs stay live (manual
//     exception); all other outputs always run.
//   * length precedence: buttons count -> `length` input -> interactive length
//     (latched permanently at the first step-button press in length mode).
//   * presets: 16 slots {steps, accents, alternate, interactive length, mute,
//     pattern offset}; the three standard access modes (button.cpp semantics);
//     trigger order clear(all) -> savepreset -> loadpreset. dontsave/display/
//     header are accepted but inert headless.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include "../src/rng.hpp"
#include "../src/uihelpers.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace droid {

namespace {

inline uint32_t hmix(uint32_t x) {
    x ^= x >> 16; x *= 0x7FEB352DU; x ^= x >> 15; x *= 0x846CA68BU; x ^= x >> 16;
    return x;
}
inline uint32_t hval(uint32_t seed, uint32_t salt, uint32_t step, uint32_t kind, uint32_t gen) {
    uint32_t x = hmix(seed ^ 0x9E3779B9U);
    x = hmix(x ^ (salt * 0x85EBCA6BU));
    x = hmix(x ^ (step * 0xC2B2AE35U));
    x = hmix(x ^ (kind * 0x27D4EB2FU));
    x = hmix(x ^ (gen  * 0x165667B1U));
    return x;
}
inline float hUniform(uint32_t seed, uint32_t salt, uint32_t step, uint32_t kind, uint32_t gen) {
    return (hval(seed, salt, step, kind, gen) >> 8) * (1.0f / 16777216.0f);
}

enum : uint32_t { K_CLASS = 1, K_PITCH = 2, K_BEAT = 3, K_MORPHSEL = 4,
                  K_VARREM = 5, K_VARADD = 6, K_ACT = 7, K_FILL = 8, K_ROLL = 9 };
constexpr uint32_t kChaoticKinds[6] = { K_PITCH, K_VARREM, K_VARADD, K_ACT, K_FILL, K_ROLL };

inline long rndHalfUp(double x) { return (long)std::floor(x + 0.5); }

inline int ctz32(uint32_t x) {
    int n = 0;
    while ((x & 1u) == 0) { x >>= 1; n++; }
    return n;
}

inline int branchFor(long bar, int branches) {
    if (branches <= 0) return 1;
    if (bar % 2 == 0) return 1;
    return 2 + std::min(ctz32((uint32_t)((bar + 1) / 2)), branches - 1);
}

constexpr int kMaxSteps = 64;
constexpr int kButtons = 16;
constexpr int kPresets = 16;

} // namespace

class Algoquencer : public Circuit {
    struct Snapshot {
        bool steps[kButtons] = {};
        bool accents[kButtons] = {};
        bool alt[kButtons] = {};
        int iLen = 16;
        bool lenTouched = false;
        bool muted = false;
        int patternOffset = 0;
    };

public:
    void tick(EngineState& s) override {
        if (!inited_) init(s);

        bool selected = ui::isSelected(*this, s);
        long trigTicks = std::max(1L, lround(0.01 * s.tickRateHz));

        readSeedTriggers(s);
        handlePresetsAndClear(s);
        handleButtons(s, selected);

        // ---- transport --------------------------------------------------
        bool resetTrig = resetG_.risingEdge(in("reset").value(s));
        if (resetTrig) {
            armed_ = true; pos_ = -1; bar_ = 0; altPos_ = -1; altBar_ = 0;
            cursorDark_ = true; nextPrepared_ = false; barPrepared_ = false;
        }
        bool clockEdge = clockG_.risingEdge(in("clock").value(s));
        if (clockEdge) {
            if (haveClock_) period_ = double(s.tick - lastClock_);
            lastClock_ = s.tick; haveClock_ = true;

            int barLen = barLenNow(s);
            int altBarLen = altBarLenNow(s);
            pos_++;
            if (pos_ >= barLen) { pos_ = 0; bar_++; }
            altPos_++;
            if (altPos_ >= altBarLen) { altPos_ = 0; altBar_++; }
            cursorDark_ = false;
            armed_ = false;

            if (pos_ == 0) enterBar(s, trigTicks);
            fireStep(s, trigTicks, barLen);
            if (pos_ == barLen - 1) prepareNext(s, barLen);
            scheduleRolls(s, barLen);
        }

        fireDueRolls(s, trigTicks);
        writeOutputs(s, selected, trigTicks);
    }

    // --- persistent state (DROIDSTA.BIN contract) ---------------------------
    // The edited pattern (steps/accents/alternate/length/mute/offset) + the 16
    // presets + slot. Pattern generation (gen_, morphs, rolls, transport) is
    // runtime and regenerated from the seed; not saved.
    static constexpr size_t kSnapLen = 3 * (size_t)kButtons + 4;
    int stateVersion() const override { return 1; }
    void saveState(StateWriter& w) const override {
        writeSnap(w, cur_);
        for (int p = 0; p < kPresets; p++) writeSnap(w, presets_[p]);
        w.n(prevPreset_);
    }
    void loadState(EngineState& s, int version, const std::vector<double>& in) override {
        if (version != 1 || in.size() != kSnapLen * (kPresets + 1) + 1) return;
        if (!inited_) init(s);
        StateReader r{in};
        readSnap(r, cur_);
        for (int p = 0; p < kPresets; p++) readSnap(r, presets_[p]);
        prevPreset_ = (int)r.n();
    }
    static void writeSnap(StateWriter& w, const Snapshot& d) {
        for (int i = 0; i < kButtons; i++) w.b(d.steps[i]);
        for (int i = 0; i < kButtons; i++) w.b(d.accents[i]);
        for (int i = 0; i < kButtons; i++) w.b(d.alt[i]);
        w.n(d.iLen); w.b(d.lenTouched); w.b(d.muted); w.n(d.patternOffset);
    }
    static void readSnap(StateReader& r, Snapshot& d) {
        for (int i = 0; i < kButtons; i++) d.steps[i]   = r.b();
        for (int i = 0; i < kButtons; i++) d.accents[i] = r.b();
        for (int i = 0; i < kButtons; i++) d.alt[i]     = r.b();
        d.iLen = (int)r.n(); d.lenTouched = r.b(); d.muted = r.b();
        d.patternOffset = (int)r.n();
    }

private:
    // ---- init ------------------------------------------------------------
    void init(EngineState& s) {
        inited_ = true;
        nButtons_ = 0;
        for (int i = 1; i <= kButtons; i++)
            if (in("button", i).connected()) nButtons_++;
        cur_ = defaultState(s);
        for (int p = 0; p < kPresets; p++) presets_[p] = defaultState(s);
        prevPreset_ = ui::clampPreset(lround(in("preset").value(s)), kPresets - 1);
    }

    Snapshot defaultState(EngineState& s) {
        Snapshot d;
        int sa = (int)lround(in("startaccents").value(s));
        for (int i = 0; i < kButtons; i++)
            d.accents[i] = (sa == 1) || (sa == 2 && i % 2 == 0);
        return d;
    }

    // ---- config helpers ----------------------------------------------------
    int effLen(EngineState& s) {
        if (cur_.lenTouched) return std::min(std::max(cur_.iLen, 1), kMaxSteps);
        if (in("length").connected()) {
            long l = lround(in("length").value(s));
            return (int)std::min(std::max(l, 1L), (long)kMaxSteps);
        }
        return nButtons_ > 0 ? nButtons_ : 16;
    }
    int repeatsNow(EngineState& s) { return std::max(1, (int)lround(in("repeats").value(s))); }
    int barLenNow(EngineState& s) { return std::min(effLen(s) * repeatsNow(s), kMaxSteps); }
    int altBarLenNow(EngineState& s) {
        int rp = in("alternaterepeats").connected()
                     ? std::max(1, (int)lround(in("alternaterepeats").value(s)))
                     : repeatsNow(s);
        return std::min(effLen(s) * rp, kMaxSteps);
    }
    bool altBarNow(EngineState& s) {
        int ab = std::max(1, (int)lround(in("alternatebars").value(s)));
        return altBar_ % ab == ab - 1;
    }
    float activityNow(EngineState& s) {
        if (!in("activity").connected()) return 0.5f;
        return std::min(std::max(in("activity").value(s), 0.0f), 1.0f);
    }
    float frac(EngineState& s, const char* name) {
        return std::min(std::max(in(name).value(s), 0.0f), 1.0f);
    }
    uint32_t seedNow(EngineState& s) {
        return (uint32_t)(lround(in("pattern").value(s)) + cur_.patternOffset);
    }
    int branchesNow(EngineState& s) {
        return std::min(std::max((int)lround(in("branches").value(s)), 0), 20);
    }
    int saltFor(EngineState& s, long bar) {
        int br = branchesNow(s);
        return (br > 0 && frac(s, "dejavu") > 0.0f) ? branchFor(bar, br) : 1;
    }
    bool isDet(EngineState& s, int j) {
        return hUniform(seedNow(s), 0, (uint32_t)j, K_CLASS, 0) < frac(s, "dejavu");
    }

    // ---- seed triggers -----------------------------------------------------
    void readSeedTriggers(EngineState& s) {
        if (risingEdge(npPrev_, in("nextpattern").value(s))) cur_.patternOffset++;
        if (risingEdge(ppPrev_, in("prevpattern").value(s))) cur_.patternOffset--;
        if (risingEdge(rrPrev_, in("reroll").value(s)))
            cur_.patternOffset = (int)(nextRand(s.rngState) & 0xFFFF);
        uint32_t seed = seedNow(s);
        if (seed != prevSeed_) {
            std::memset(gen_, 0, sizeof(gen_));
            prevSeed_ = seed;
        }
    }

    // ---- presets / clear ---------------------------------------------------
    void handlePresetsAndClear(EngineState& s) {
        bool presetPatched = in("preset").connected();
        bool loadPatched = in("loadpreset").connected();
        bool savePatched = in("savepreset").connected();
        bool immediate = presetPatched && !loadPatched && !savePatched;

        bool clearAll = risingEdge(caPrev_, in("clearall").value(s));
        bool clr = risingEdge(clPrev_, in("clear").value(s));
        if (clearAll) {
            cur_ = defaultState(s);
            for (int p = 0; p < kPresets; p++) presets_[p] = defaultState(s);
            std::memset(gen_, 0, sizeof(gen_)); morphAcc_ = 0;
        } else if (clr) {
            cur_ = defaultState(s);
            std::memset(gen_, 0, sizeof(gen_)); morphAcc_ = 0;
            if (immediate) presets_[prevPreset_] = cur_;
        }

        bool saveTrig = risingEdge(spPrev_, in("savepreset").value(s));
        bool loadTrig = risingEdge(lpPrev_, in("loadpreset").value(s));
        if (savePatched && saveTrig)
            presets_[ui::presetNum(*this, s, in("savepreset").value(s), presetPatched, kPresets - 1)] = cur_;
        if (loadPatched && loadTrig)
            cur_ = presets_[ui::presetNum(*this, s, in("loadpreset").value(s), presetPatched, kPresets - 1)];
        if (immediate) {
            int p = ui::clampPreset(lround(in("preset").value(s)), kPresets - 1);
            if (p != prevPreset_) {
                presets_[prevPreset_] = cur_;
                cur_ = presets_[p];
                prevPreset_ = p;
            }
        } else if (presetPatched) {
            prevPreset_ = ui::clampPreset(lround(in("preset").value(s)), kPresets - 1);
        }
    }

    // ---- buttons / pages ---------------------------------------------------
    void handleButtons(EngineState& s, bool selected) {
        // mute/unmute: always live (even deselected)
        if (risingEdge(mbPrev_, in("mutebutton").value(s))) cur_.muted = !cur_.muted;
        if (risingEdge(ubPrev_, in("unmutebutton").value(s))) unmuteArmed_ = true;

        bool lenMode = in("lengthbutton").value(s) >= kGateHighThreshold;
        bool accMode = in("accentbutton").value(s) >= kGateHighThreshold;
        bool altMode = in("alternatebutton").value(s) >= kGateHighThreshold;

        // step-button edges advance every tick; processed only while selected
        for (int i = 0; i < kButtons; i++) {
            bool rose = risingEdge(btnPrev_[i], in("button", i + 1).value(s));
            if (!rose || !selected) continue;
            if (lenMode) {
                cur_.iLen = i + 1;
                cur_.lenTouched = true;
            } else if (accMode) {
                cur_.accents[i] = !cur_.accents[i];
            } else if (altMode) {
                cur_.alt[i] = !cur_.alt[i];
            } else {
                cur_.steps[i] = !cur_.steps[i];
            }
        }

        bool cpTrig = risingEdge(cpPrev_, in("clearpage").value(s));
        if (cpTrig && selected) {
            bool* page = lenMode ? nullptr
                        : accMode ? cur_.accents
                        : altMode ? cur_.alt
                                  : cur_.steps;
            if (page) std::memset(page, 0, kButtons * sizeof(bool));
        }
        lenMode_ = lenMode; accMode_ = accMode; altMode_ = altMode;
    }

    // ---- per-bar preparation (morphs + chaotic draws) -----------------------
    void prepareInto(EngineState& s, long bar, float (*chao)[10], std::vector<int>& pendingMorphs,
                     int& morphCount, int barLen) {
        morphCount = 0;
        pendingMorphs.clear();
        float m = frac(s, "morphs");
        int br = branchesNow(s);
        if (m > 0.0f && bar > 0) {
            bool allowed = br > 0 ? (bar % (1L << br) == 0) : true;
            if (allowed) {
                double rate = std::max(std::pow(10.0 * m, 2.0) / 100.0, 1.0 / 64.0);
                morphAcc_ += rate * barLen;
                int k = (int)morphAcc_; morphAcc_ -= k;
                if (k > 0) {
                    struct Cand { float key; int j; };
                    std::vector<Cand> det;
                    for (int j = 0; j < barLen; j++)
                        if (isDet(s, j))
                            det.push_back({hUniform(seedNow(s), (uint32_t)(0x100 + bar),
                                                    (uint32_t)j, K_MORPHSEL, 0), j});
                    std::sort(det.begin(), det.end(), [](const Cand& a, const Cand& b) {
                        return a.key != b.key ? a.key > b.key : a.j < b.j;
                    });
                    for (int i = 0; i < (int)det.size() && i < k; i++)
                        pendingMorphs.push_back(det[i].j);
                    morphCount = (int)pendingMorphs.size();
                }
            }
        }
        for (int j = 0; j < barLen; j++) {
            if (isDet(s, j)) continue;
            for (uint32_t kind : kChaoticKinds)
                chao[j][kind] = randUniform(s.rngState);
        }
    }

    void enterBar(EngineState& s, long trigTicks) {
        int barLen = barLenNow(s);
        if (nextPrepared_) {
            std::memcpy(chaoCur_, chaoNext_, sizeof(chaoCur_));
            for (int j : nextMorphs_) gen_[j]++;
            morphsThisBar_ = nextMorphCount_;
            nextPrepared_ = false;
        } else {
            std::vector<int> pend;
            prepareInto(s, bar_, chaoCur_, pend, morphsThisBar_, barLen);
            for (int j : pend) gen_[j]++;
        }
        barPrepared_ = true;
        if (morphsThisBar_ > 0) morphledUntil_ = s.tick + trigTicks;
        sobUntil_ = s.tick + trigTicks;
    }

    void prepareNext(EngineState& s, int barLen) {
        prepareInto(s, bar_ + 1, chaoNext_, nextMorphs_, nextMorphCount_, barLen);
        nextPrepared_ = true;
    }

    // ---- pure bar computation ------------------------------------------------
    float u(EngineState& s, int j, uint32_t kind, int salt, const float (*chao)[10], int genExtra) {
        if (isDet(s, j))
            return hUniform(seedNow(s), (uint32_t)salt, (uint32_t)j, kind, gen_[j] + (uint32_t)genExtra);
        return chao[j][kind];
    }

    double weight(EngineState& s, int j, int barLen, bool endbias) {
        float ob = frac(s, "offbeats"), di = frac(s, "distribution");
        double w = ((j + 1) % 2 == 0) ? ob : 1.0 - ob;
        w *= (j * 2 >= barLen) ? di : 1.0 - di;
        if (endbias) w *= double(j + 1) / barLen;
        return w;
    }

    void pickWeighted(EngineState& s, const std::vector<int>& cands, int k, uint32_t kind,
                      int salt, const float (*chao)[10], int genExtra, bool endbias,
                      int barLen, std::vector<int>& out) {
        struct Cand { double key; int j; };
        std::vector<Cand> sc;
        sc.reserve(cands.size());
        for (int j : cands) {
            double uu = u(s, j, kind, salt, chao, genExtra);
            double w = std::max(weight(s, j, barLen, endbias), 1e-6);
            sc.push_back({std::pow(uu, 1.0 / w), j});
        }
        std::sort(sc.begin(), sc.end(), [](const Cand& a, const Cand& b) {
            return a.key != b.key ? a.key > b.key : a.j < b.j;
        });
        out.clear();
        for (int i = 0; i < (int)sc.size() && i < k; i++) out.push_back(sc[i].j);
    }

    void removeLowest(EngineState& s, const std::vector<int>& beats, int k, uint32_t kind,
                      int salt, const float (*chao)[10], int genExtra, std::vector<int>& out) {
        struct Cand { double key; int j; };
        std::vector<Cand> sc;
        sc.reserve(beats.size());
        for (int j : beats) sc.push_back({u(s, j, kind, salt, chao, genExtra), j});
        std::sort(sc.begin(), sc.end(), [](const Cand& a, const Cand& b) {
            return a.key != b.key ? a.key < b.key : a.j < b.j;
        });
        out.clear();
        for (int i = 0; i < (int)sc.size() && i < k; i++) out.push_back(sc[i].j);
    }

    struct BarTable {
        bool beat[kMaxSteps] = {};
        bool fill[kMaxSteps] = {};
        bool roll[kMaxSteps] = {};
    };

    void computeTable(EngineState& s, long bar, bool altOn, const float (*chao)[10],
                      int genExtra, BarTable& t) {
        int L = effLen(s);
        int barLen = std::min(L * repeatsNow(s), kMaxSteps);
        int salt = saltFor(s, bar);
        std::vector<int> tmp;

        for (int j = 0; j < barLen; j++) {
            bool b = false;
            if (nButtons_ > 0) {
                int bi = (j % L) % kButtons;
                b = cur_.steps[bi];
                if (altOn) b ^= cur_.alt[bi];
            }
            t.beat[j] = b;
            t.fill[j] = t.roll[j] = false;
        }

        auto beatsVec = [&]() { std::vector<int> v; for (int j = 0; j < barLen; j++) if (t.beat[j]) v.push_back(j); return v; };
        auto emptiesVec = [&]() { std::vector<int> v; for (int j = 0; j < barLen; j++) if (!t.beat[j]) v.push_back(j); return v; };

        // variation: move beats, count preserved
        int nb = (int)beatsVec().size();
        int kv = (int)rndHalfUp(frac(s, "variation") * nb);
        if (kv > 0) {
            removeLowest(s, beatsVec(), kv, K_VARREM, salt, chao, genExtra, tmp);
            for (int j : tmp) t.beat[j] = false;
            pickWeighted(s, emptiesVec(), kv, K_VARADD, salt, chao, genExtra, false, barLen, tmp);
            for (int j : tmp) t.beat[j] = true;
        }

        // activity
        float a = activityNow(s);
        nb = (int)beatsVec().size();
        if (nButtons_ == 0) {
            int target = (int)rndHalfUp(a * barLen);
            if (target > nb) {
                pickWeighted(s, emptiesVec(), target - nb, K_ACT, salt, chao, genExtra, false, barLen, tmp);
                for (int j : tmp) t.beat[j] = true;
            } else if (target < nb) {
                removeLowest(s, beatsVec(), nb - target, K_ACT, salt, chao, genExtra, tmp);
                for (int j : tmp) t.beat[j] = false;
            }
        } else if (a < 0.5f) {
            int keep = (int)rndHalfUp(2.0 * a * nb);
            if (keep < nb) {
                removeLowest(s, beatsVec(), nb - keep, K_ACT, salt, chao, genExtra, tmp);
                for (int j : tmp) t.beat[j] = false;
            }
        } else if (a > 0.5f) {
            int addn = (int)rndHalfUp((2.0 * a - 1.0) * (barLen - nb));
            if (addn > 0) {
                pickWeighted(s, emptiesVec(), addn, K_ACT, salt, chao, genExtra, false, barLen, tmp);
                for (int j : tmp) t.beat[j] = true;
            }
        }

        // fills
        float f = frac(s, "fills");
        if (f > 0.0f) {
            int fo = (int)lround(in("fillorder").value(s));
            double factor = 0.0;
            if (fo <= 0) factor = 1.0;
            else if (fo == 1) factor = (bar % 2 == 1) ? 1.0 : 0.0;
            else if (fo == 2) factor = (bar % 4 == 1) ? 0.5 : (bar % 4 == 3) ? 1.0 : 0.0;
            else factor = (bar % 8 == 1 || bar % 8 == 5) ? 0.25
                        : (bar % 8 == 3) ? 0.5 : (bar % 8 == 7) ? 1.0 : 0.0;
            if (factor > 0.0) {
                int extra = std::max(1, (int)rndHalfUp(f * factor * barLen / 4.0));
                std::vector<int> empties = emptiesVec();
                int addn = std::min(extra, (int)empties.size());
                pickWeighted(s, empties, addn, K_FILL, salt, chao, genExtra, true, barLen, tmp);
                for (int j : tmp) { t.beat[j] = true; t.fill[j] = true; }
                int shortBy = extra - addn;
                if (shortBy > 0) {
                    std::vector<int> conv;
                    for (int j = 0; j < barLen; j++) if (t.beat[j] && !t.fill[j]) conv.push_back(j);
                    pickWeighted(s, conv, shortBy, K_FILL, salt, chao, genExtra, true, barLen, tmp);
                    for (int j : tmp) t.fill[j] = true;
                }
            }
        }

        // rolls
        float rl = frac(s, "rolls");
        int rc = std::max(0, (int)lround(in("rollcount").value(s)));
        if (rl > 0.0f && rc > 0) {
            for (int j = 0; j < barLen; j++)
                if (t.beat[j] && u(s, j, K_ROLL, salt, chao, genExtra) < rl)
                    t.roll[j] = true;
        }
    }

    float pitchAt(EngineState& s, int j, long bar, const float (*chao)[10], int genExtra) {
        int L = effLen(s);
        // wired pitch1..16 -> melody mode (cycled past 16)
        bool anyPitch = false;
        for (int i = 1; i <= kButtons; i++)
            if (in("pitch", i).connected()) { anyPitch = true; break; }
        if (anyPitch) {
            int pi = (j % L) % kButtons;
            return in("pitch", pi + 1).value(s);
        }
        int salt = saltFor(s, bar);
        float uu = u(s, j, K_PITCH, salt, chao, genExtra);
        float pl = in("pitchlow").value(s), ph = in("pitchhigh").value(s);
        int res = (int)lround(in("pitchresolution").value(s));
        if (res >= 2) {
            int q = std::min((int)(uu * res), res - 1);
            return pl + (float(q) / (res - 1)) * (ph - pl);
        }
        if (res == 1) return (pl + ph) * 0.5f;
        return pl + uu * (ph - pl);
    }

    // ---- playback -------------------------------------------------------------
    void fireStep(EngineState& s, long trigTicks, int barLen) {
        // unmute lands exactly at bar starts
        if (pos_ == 0 && unmuteArmed_) { cur_.muted = false; unmuteArmed_ = false; }

        BarTable t;
        computeTable(s, bar_, altBarNow(s), chaoCur_, 0, t);
        cursorStep_ = (pos_ % effLen(s)) % kButtons;

        bool beat = pos_ < barLen && t.beat[pos_];
        accentHigh_ = false;
        if (beat) {
            lastPitch_ = pitchAt(s, pos_, bar_, chaoCur_, 0);
            int bi = (pos_ % effLen(s)) % kButtons;
            accentHigh_ = cur_.accents[bi];
            if (!cur_.muted) {
                trigUntil_ = s.tick + trigTicks;
                rollVelo_ = 1.0f;
                if (haveClock_ && period_ > 0) {
                    long len = std::max(1L, lround(frac(s, "gatelength") * period_));
                    gateWin_.push_back({s.tick, s.tick + (uint64_t)len});
                }
                if (t.fill[pos_]) fillledUntil_ = s.tick + trigTicks;
            }
        }
    }

    void scheduleRolls(EngineState& s, int barLen) {
        if (!haveClock_ || period_ <= 0 || cur_.muted) return;
        float rl = frac(s, "rolls");
        int rc = std::max(0, (int)lround(in("rollcount").value(s)));
        int rs = std::max(1, (int)lround(in("rollsteps").value(s)));
        if (rl <= 0.0f || rc <= 0) return;

        // look one step ahead (into the pre-materialized next bar if needed)
        int nj = pos_ + 1;
        long nbar = bar_;
        const float (*chao)[10] = chaoCur_;
        int genExtra = 0;
        bool altOn = altBarNow(s);
        if (nj >= barLen) {
            if (!nextPrepared_) return;   // SPEC-GAP: no roll into an unprepared bar
            nj = 0; nbar = bar_ + 1; chao = chaoNext_;
            // next bar's pending morphs act as gen bumps for lookahead
            genExtra = 0;  // gens bumps are per-step; handled via nextMorphs_ below
            int ab = std::max(1, (int)lround(in("alternatebars").value(s)));
            int altBarLen = altBarLenNow(s);
            long nAltBar = altBar_ + ((altPos_ + 1 >= altBarLen) ? 1 : 0);
            altOn = nAltBar % ab == ab - 1;
        }
        BarTable t;
        // For cross-bar lookahead with pending morphs we approximate genExtra=1 on
        // morphed steps by temporarily bumping; cheaper: compute without bumps —
        // SPEC-GAP: pending morphs are not visible to roll lookahead.
        computeTable(s, nbar, altOn, chao, genExtra, t);
        if (!(t.beat[nj] && t.roll[nj])) return;

        int n = rc * rs;
        float rsv = std::min(std::max(in("rollstartvelo").value(s), 0.0f), 1.0f);
        double span = (double)rs * period_;
        uint64_t beatTick = s.tick + (uint64_t)lround(period_);
        long gateLen = std::max(1L, lround(frac(s, "gatelength") * period_ / n));
        for (int i = 0; i < n; i++) {
            double tOff = -span + (span * i) / n;   // relative to the beat
            long long at = (long long)beatTick + (long long)llround(tOff);
            if (at < (long long)s.tick) continue;   // earlier than our lookahead
            float v = rsv + (1.0f - rsv) * float(i) / float(n);
            rolls_.push_back({(uint64_t)at, v, gateLen});
        }
    }

    void fireDueRolls(EngineState& s, long trigTicks) {
        for (auto it = rolls_.begin(); it != rolls_.end();) {
            if (it->at == s.tick) {
                trigUntil_ = s.tick + trigTicks;
                rollVelo_ = it->velo;
                gateWin_.push_back({s.tick, s.tick + (uint64_t)it->gateLen});
                it = rolls_.erase(it);
            } else if (it->at < s.tick) {
                it = rolls_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // ---- outputs ---------------------------------------------------------------
    void writeOutputs(EngineState& s, bool selected, long trigTicks) {
        (void)trigTicks;
        out("trigger").set(s, s.tick < trigUntil_ ? 1.0f : 0.0f);

        bool gateOn = false;
        for (auto it = gateWin_.begin(); it != gateWin_.end();) {
            if (s.tick >= it->end) { it = gateWin_.erase(it); continue; }
            if (s.tick >= it->start) gateOn = true;
            ++it;
        }
        out("gate").set(s, gateOn && !cur_.muted ? 1.0f : 0.0f);

        out("pitch").set(s, lastPitch_);
        float al = in("accentlow").value(s), ah = in("accenthigh").value(s);
        out("accent").set(s, (accentHigh_ && !cur_.muted) ? ah : al);
        out("rollvelocity").set(s, rollVelo_);
        out("startofbar").set(s, s.tick < sobUntil_ ? 1.0f : 0.0f);
        out("morphled").set(s, s.tick < morphledUntil_ ? 1.0f : 0.0f);
        out("fillsled").set(s, s.tick < fillledUntil_ ? 1.0f : 0.0f);
        out("branch").set(s, (float)branchFor(bar_, branchesNow(s)));
        out("lengthoutput").set(s, (float)effLen(s));
        out("muteled").set(s, cur_.muted ? 1.0f : 0.0f);

        long blink = std::max(1L, lround(0.25 * s.tickRateHz));
        bool blinkOn = ((long)(s.tick / (uint64_t)blink) % 2) == 0;
        out("unmuteled").set(s, unmuteArmed_ && blinkOn ? 1.0f : 0.0f);

        for (int i = 0; i < 4; i++)
            out("barled", i + 1).set(s, (bar_ % 4) == i ? 1.0f : 0.0f);

        if (selected) {
            int L = effLen(s);
            for (int i = 0; i < kButtons; i++) {
                float v;
                if (lenMode_) v = i < std::min(L, kButtons) ? 1.0f : 0.0f;
                else if (accMode_) v = cur_.accents[i] ? 1.0f : 0.0f;
                else if (altMode_) v = cur_.alt[i] ? 1.0f : 0.0f;
                else {
                    v = cur_.steps[i] ? 1.0f : 0.0f;
                    if (!cursorDark_ && pos_ >= 0 && i == cursorStep_)
                        v = std::max(v, 0.5f);
                }
                out("led", i + 1).set(s, v);
            }
        }
    }

    // ---- state -----------------------------------------------------------------
    bool inited_ = false;
    int nButtons_ = 0;

    Snapshot cur_;
    Snapshot presets_[kPresets];
    int prevPreset_ = 0;

    GateReader clockG_, resetG_;
    bool haveClock_ = false;
    uint64_t lastClock_ = 0;
    double period_ = 0.0;
    int pos_ = -1;
    long bar_ = 0;
    int altPos_ = -1;
    long altBar_ = 0;
    bool armed_ = true;
    bool cursorDark_ = false;
    int cursorStep_ = 0;

    uint32_t gen_[kMaxSteps] = {};
    uint32_t prevSeed_ = 0;
    float chaoCur_[kMaxSteps][10] = {};
    float chaoNext_[kMaxSteps][10] = {};
    bool nextPrepared_ = false;
    bool barPrepared_ = false;
    std::vector<int> nextMorphs_;
    int nextMorphCount_ = 0;
    int morphsThisBar_ = 0;
    double morphAcc_ = 0.0;

    bool npPrev_ = false, ppPrev_ = false, rrPrev_ = false;
    bool caPrev_ = false, clPrev_ = false, spPrev_ = false, lpPrev_ = false;
    bool mbPrev_ = false, ubPrev_ = false, cpPrev_ = false;
    bool btnPrev_[kButtons] = {};
    bool lenMode_ = false, accMode_ = false, altMode_ = false;
    bool unmuteArmed_ = false;

    float lastPitch_ = 0.0f;
    bool accentHigh_ = false;
    float rollVelo_ = 1.0f;
    uint64_t trigUntil_ = 0, sobUntil_ = 0, morphledUntil_ = 0, fillledUntil_ = 0;
    struct GateWin { uint64_t start, end; };
    std::vector<GateWin> gateWin_;
    struct RollBeat { uint64_t at; float velo; long gateLen; };
    std::vector<RollBeat> rolls_;
};

DROID_REGISTER_CIRCUIT(algoquencer, Algoquencer)

} // namespace droid
