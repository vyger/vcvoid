// octave — Multi-VCO octave animator. Spec: manual/circuits/octave.md.
// Drives three VCO pitches (output1..output3) from one `input` pitch:
//   output1 = input                                  (exact passthrough)
//   output2 = input + spreadOffset/2 + detuneOffset  (mid voice, detuned up)
//   output3 = input + spreadOffset   - detuneOffset  (top voice, detuned down)
// spreadOffset spans output1->output3; output2 sits at its midpoint.
//
// Spread (stepped): fully left = no change; at 1.0 output3 is four octaves up
// and output2 two octaves up (manual pg 355). `fifths=on` inserts a perfect
// fifth (7 semitones) between each octave step.
//
// Detune (linear / constant-Hz, manual pg 356): at detune=1.0 the detune is
// 4 semitones for input=0 V and HALVES per octave of input (2 st at 1 V, 1 st
// at 2 V, ...), so the beating frequency stays constant across pitch.
//
// Units: engine 1.0 == 10 V; 1 octave (1 V/oct) = 0.1; 1 semitone = 0.1/12.
//
// SPEC-GAP (manual gives endpoints/rule but not every middle step, literal
// readings taken): (1) spread is quantized to the nearest step,
// round(spread * stepsPerFullTravel) with stepsPerFullTravel = 4 (fifths off)
// or 8 (fifths on); (2) output2 is the exact geometric midpoint of the spread,
// not independently re-quantized; (3) the linear detune halves against `input`
// per the manual's wording, not against each VCO's spread-shifted pitch;
// (4) spread/detune outside the documented [0,1] extrapolate the same formulas
// (unclamped) since the manual is silent there; negative spread mirrors the
// octave/fifth ladder downward (odd-symmetric about 0).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>

namespace droid {

namespace {
constexpr float kOctaveUnit   = 0.1f;         // 1 octave in engine units (1 V)
constexpr float kSemitoneUnit = 0.1f / 12.0f; // 1 semitone in engine units
// output3 offset in semitones for spread step L, with a perfect fifth inserted
// between each octave when `fifths` is on: 0, (5th), 8ve, (8ve+5th), 2*8ve, ...
// The sequence is odd-symmetric about 0, so negative levels mirror downward
// (level -1 -> -7, -2 -> -12, -3 -> -19, ...) rather than following C++'s
// truncating division, which would fold negatives back upward.
float spreadSemitones(long level, bool fifths) {
    if (!fifths) return 12.0f * (float)level;   // whole octaves only
    long sign = level < 0 ? -1 : 1;
    long mag  = level < 0 ? -level : level;
    long octave = mag / 2, within = mag % 2;
    return (float)sign * (12.0f * (float)octave + (within ? 7.0f : 0.0f));
}
}

class Octave : public Circuit {
public:
    void tick(EngineState& s) override {
        float input  = in("input").value(s);
        float spread = in("spread").value(s);
        float detune = in("detune").value(s);
        bool  fifths = in("fifths").value(s) >= kGateHighThreshold;

        // Spread: quantize to the nearest step, then look up output3's offset.
        // Full travel (spread 1.0) is 4 octaves = 4 steps without fifths, or
        // 8 steps (fifth+octave alternating) with fifths.
        long steps = fifths ? 8 : 4;
        long level = std::lround((double)spread * (double)steps);
        float spreadOffset = spreadSemitones(level, fifths) * kSemitoneUnit;

        // Linear detune: 4 semitones at input 0 V, halving per octave of input.
        float inputOctaves = input / kOctaveUnit;
        float detuneSemis  = 4.0f * detune * std::pow(2.0f, -inputOctaves);
        float detuneOffset = detuneSemis * kSemitoneUnit;

        out("output", 1).set(s, input);
        out("output", 2).set(s, input + 0.5f * spreadOffset + detuneOffset);
        out("output", 3).set(s, input + spreadOffset - detuneOffset);
    }
};

DROID_REGISTER_CIRCUIT(octave, Octave)

} // namespace droid
