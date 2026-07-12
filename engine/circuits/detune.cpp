// detune — Detune up to eight voices "in a most disharmonic way". Spec:
// manual/circuits/detune.md. Property-faithful invented core where the manual
// gives no formula (user-approved SPEC-GAP policy, algoquencer precedent).
//
// Specified: tuningmode high -> every output emits tuningpitch; detune = 0 ->
// passthrough; 8 independent voices (unpatched inputs default to 0 V).
//
// SPEC-GAP: the manual states the algorithm is "identical with that of the
// Sinfonion" with no formula anywhere. Invented core (property-faithful,
// user-approved policy; unverified vs hardware):
//     output_k = input_k + detune * c_k / 120
// with per-voice coefficients in semitones at detune = 1.0:
//     c = [+1.00, -0.90, +0.78, -0.64, +0.52, -0.38, +0.26, -0.14]
// All magnitudes distinct (pairwise intervals distort rather than transpose),
// alternating signs spread the chord, +-1 semitone maximum at full detune,
// linear in the detune CV (negative detune mirrors the spread).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"

namespace droid {

class Detune : public Circuit {
    static constexpr float kCoef[8] = { +1.00f, -0.90f, +0.78f, -0.64f,
                                        +0.52f, -0.38f, +0.26f, -0.14f };

public:
    void tick(EngineState& s) override {
        bool tuning = in("tuningmode").value(s) >= kGateHighThreshold;
        float tp = in("tuningpitch").value(s);
        float d = in("detune").value(s);
        for (int k = 0; k < 8; k++) {
            Output& o = out("output", k + 1);
            if (!o.connected()) continue;
            if (tuning) { o.set(s, tp); continue; }
            o.set(s, in("input", k + 1).value(s) + d * kCoef[k] / 120.0f);
        }
    }
};

DROID_REGISTER_CIRCUIT(detune, Detune)

} // namespace droid
