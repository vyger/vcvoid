// superjust — Dynamic just intonation for up to eight voices. Spec:
// manual/circuits/superjust.md. Property-faithful invented core where the
// manual gives no formula (user-approved SPEC-GAP policy, algoquencer
// precedent).
//
// The real algorithm is patent-pending and undocumented; this is a generic
// (prior-art) small-limit just-intonation retuner satisfying every
// manual-pinned invariant: each pair of output frequencies is in a rational
// ratio with small numerator/denominator, the average pitch is exactly
// unchanged, and the retune is dynamic ("listens" to the current notes).
//
// SPEC-GAP pins (property-faithful, unverified vs hardware):
//   * anchor = lowest connected voice; each voice's interval vs the anchor,
//     octave-reduced, snaps to the nearest of 1/1 16/15 9/8 6/5 5/4 4/3 7/5
//     3/2 8/5 5/3 9/5 15/8 (5-limit + 7/5 tritone) or the octave wrap.
//   * mean preservation: all candidates shift by mean(inputs)-mean(candidates)
//     (a single voice therefore passes through unchanged).
//   * hysteresis: retune recomputes when any input moves >= 5 cents from its
//     value at the last retune (or the connected set changes); else hold.
//   * tuningmode takes precedence over bypass; transpose applies in normal
//     mode only (both per the Inputs table, precedence pinned here).
//   * voices with an unconnected input output 0 V (tuningmode still applies).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>

namespace droid {

class Superjust : public Circuit {
    // 1200*log2(n/d) for 1/1 16/15 9/8 6/5 5/4 4/3 7/5 3/2 8/5 5/3 9/5 15/8,
    // in semitones (cents/100); same constants as the sjref.py replica.
    static constexpr double kJust[12] = {
        0.0, 1.117312852698, 2.039100017308, 3.156412870006,
        3.863137138648, 4.980449991346, 5.825121926043, 7.019550008654,
        8.136862861352, 8.843587129994, 10.175962878659, 10.882687147302,
    };
    static constexpr double kHysteresis = 0.05;   // 5 cents, in semitones

public:
    void tick(EngineState& s) override {
        bool tuning = in("tuningmode").value(s) >= kGateHighThreshold;
        bool bypass = in("bypass").value(s) >= kGateHighThreshold;
        float tp = in("tuningpitch").value(s);
        float tr = in("transpose").value(s);

        if (tuning) {
            for (int k = 0; k < 8; k++)
                if (out("output", k + 1).connected()) out("output", k + 1).set(s, tp);
            return;
        }
        if (bypass) {
            for (int k = 0; k < 8; k++)
                if (out("output", k + 1).connected())
                    out("output", k + 1).set(s, in("input", k + 1).value(s));
            return;
        }

        // gather connected voices in semitones
        int idx[8], n = 0;
        double st[8];
        for (int k = 0; k < 8; k++) {
            if (!in("input", k + 1).connected()) continue;
            idx[n] = k;
            st[n] = double(in("input", k + 1).value(s)) * 120.0;
            n++;
        }

        bool recompute = !haveTune_ || n != lastN_;
        if (!recompute)
            for (int i = 0; i < n; i++)
                if (idx[i] != lastIdx_[i] || std::fabs(st[i] - lastSt_[i]) >= kHysteresis) {
                    recompute = true;
                    break;
                }

        if (recompute && n > 0) {
            double anchor = st[0];
            for (int i = 1; i < n; i++) anchor = std::min(anchor, st[i]);
            double cand[8], sumIn = 0, sumCand = 0;
            for (int i = 0; i < n; i++) {
                double iv = st[i] - anchor;
                double oct = std::floor(iv / 12.0);
                double frac = iv - 12.0 * oct;
                double best = 0.0, bd = std::fabs(frac);
                for (double c : kJust)
                    if (std::fabs(frac - c) < bd) { best = c; bd = std::fabs(frac - c); }
                if (std::fabs(frac - 12.0) < bd) best = 12.0;   // octave wrap
                cand[i] = anchor + 12.0 * oct + best;
                sumIn += st[i];
                sumCand += cand[i];
            }
            double shift = (sumIn - sumCand) / n;
            for (int i = 0; i < n; i++) {
                tuned_[idx[i]] = (cand[i] + shift) / 120.0;
                lastSt_[i] = st[i];
                lastIdx_[i] = idx[i];
            }
            lastN_ = n;
            haveTune_ = true;
        }

        for (int k = 0; k < 8; k++) {
            Output& o = out("output", k + 1);
            if (!o.connected()) continue;
            float v = in("input", k + 1).connected() ? float(tuned_[k]) : 0.0f;
            o.set(s, v + tr);
        }
    }

private:
    bool haveTune_ = false;
    int lastN_ = 0;
    int lastIdx_[8] = {};
    double lastSt_[8] = {};
    double tuned_[8] = {};
};

DROID_REGISTER_CIRCUIT(superjust, Superjust)

} // namespace droid
