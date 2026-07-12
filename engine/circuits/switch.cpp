// switch — Addressable/clockable switch. Spec: manual/circuits/switch.md.
// Routes n inputs to m outputs as an N = max(n, m) rotation:
//   output[j] connects to input[(perm[j] + offset) mod N]
// where perm is the identity until `shuffle` randomly permutes it (`sort` and
// `reset` restore identity). offset = an internal counter (`forward` +1,
// `backward` -1) plus the rounded `offset` CV. Unpatched inputs read 0 V;
// unpatched outputs are discarded. `input`==`input1`, `output`==`output1`.
//
// `reset` zeroes the internal offset and forgets any shuffle, and WINS over a
// near-simultaneous forward/backward: any forward/backward within 5 ms after a
// reset (a round(0.005*tickRate)-tick window, always including the same tick) is
// ignored, so an external sequencer's imprecise reset+clock pair still lands on
// offset 0 (manual switch.md:133). A forward BEFORE a reset needs no special
// handling -- reset zeroes the offset regardless of what preceded it.
//
// SPEC-GAPs (literal readings; manual silent on each): (1) N is the highest
// patched index on each side (contiguous patching assumed per the manual's "use
// these in order, don't leave gaps"); (2) shuffle composes with offset as
// input[(perm[j]+offset) mod N] -- a permutation plus a constant shift stays a
// bijection, so no input is reused after shuffling and the offset still moves
// the shuffled connections around; (3) within one tick reset suppresses every
// other trigger; if shuffle and sort fire together, sort is applied first so
// shuffle wins.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include "../src/rng.hpp"
#include <cmath>
#include <cstdint>
#include <vector>

namespace droid {

class Switch : public Circuit {
public:
    void tick(EngineState& s) override {
        // Dimensions: highest patched index on each side (contiguous assumed).
        int n = 0, m = 0;
        for (int i = 1; i <= 16; i++) if (in("input", i).connected())  n = i;
        for (int j = 1; j <= 16; j++) if (out("output", j).connected()) m = j;
        int N = n > m ? n : m;

        // (Re)initialise perm to identity once the switch size is known.
        if ((int)perm_.size() != N) {
            perm_.resize(N);
            for (int j = 0; j < N; j++) perm_[j] = j;
        }

        // --- triggers (all tracked every tick so edges stay in sync) ---
        bool resetFired    = resetGate_.risingEdge(in("reset").value(s));
        bool shuffleFired  = shuffleGate_.risingEdge(in("shuffle").value(s));
        bool sortFired     = sortGate_.risingEdge(in("sort").value(s));
        bool forwardFired  = forwardGate_.risingEdge(in("forward").value(s));
        bool backwardFired = backwardGate_.risingEdge(in("backward").value(s));

        long window = std::lround(0.005 * (double)s.tickRateHz);  // 5 ms in ticks

        if (resetFired) {
            internalOffset_ = 0;
            for (int j = 0; j < N; j++) perm_[j] = j;   // forget shuffle
            everReset_ = true;
            lastResetTick_ = s.tick;
        } else {
            // sort undoes a shuffle (offset preserved); shuffle then re-permutes.
            if (sortFired)
                for (int j = 0; j < N; j++) perm_[j] = j;
            if (shuffleFired) {
                for (int j = 0; j < N; j++) perm_[j] = j;
                for (int i = N - 1; i > 0; i--) {        // Fisher-Yates via engine RNG
                    int k = (int)(randUniform(s.rngState) * (float)(i + 1));
                    if (k > i) k = i;
                    std::swap(perm_[i], perm_[k]);
                }
            }
            // forward/backward, suppressed within 5 ms of a reset (reset wins).
            bool nearReset = everReset_ && (s.tick - lastResetTick_) <= (uint64_t)window;
            if (!nearReset) {
                if (forwardFired)  internalOffset_ += 1;
                if (backwardFired) internalOffset_ -= 1;
            }
        }

        // --- routing ---
        if (N > 0) {
            long total = internalOffset_ + std::lround(in("offset").value(s));
            for (int j = 0; j < m; j++) {
                Output& o = out("output", j + 1);
                if (!o.connected()) continue;            // discarded output
                long src = ((perm_[j] + total) % N + N) % N;  // proper modulo
                o.set(s, in("input", src + 1).value(s));       // unpatched -> 0 V
            }
        }
    }

private:
    GateReader resetGate_, shuffleGate_, sortGate_, forwardGate_, backwardGate_;
    std::vector<int> perm_;
    long internalOffset_ = 0;
    bool everReset_ = false;
    uint64_t lastResetTick_ = 0;
};

DROID_REGISTER_CIRCUIT(switch, Switch)

} // namespace droid
