// lfo — Low frequency oscillator with seven waveforms, morphing, phase
// modulation, sync, tap tempo and randomization.
// Spec: manual/circuits/lfo.md. Standard A*B+C input math applies to every
// input before we read it.
//
// Phase convention: the output at a given tick is sampled from the phase
// accumulated BEFORE this tick's increment, so tick 0 emits phase 0 and the
// increment (freq / tickRate) is applied at the end of the tick. This is the
// convention the goldens hand-compute against (see tests/golden/lfo/).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include "../src/taptempo.hpp"
#include "../src/rng.hpp"
#include <cmath>

namespace droid {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;

inline float wrap01(double x) { return float(x - std::floor(x)); }
inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

// Unipolar waveforms: each returns a value in [0,1], starting at phase 0.
inline float wSaw(float p)  { return p; }            // rising ramp
inline float wRamp(float p) { return 1.0f - p; }     // falling ramp
inline float wSine(float p) { return 0.5f * (1.0f - std::cos(kTwoPi * p)); }
inline float wCosine(float p) { return 0.5f * (1.0f + std::sin(kTwoPi * p)); } // == sine(p+1/4)
inline float wParabola(float p) { return 4.0f * p * (1.0f - p); }
inline float wSquare(float p, float pw) { return p < pw ? 1.0f : 0.0f; }

// Triangle with skew: peak sits at phase `skew`. skew=0 -> falling ramp,
// skew=0.5 -> symmetric triangle, skew=1 -> rising sawtooth.
inline float wTriangle(float p, float skew) {
    if (p < skew) return p / skew;            // p<skew implies skew>0
    return (1.0f - p) / (1.0f - skew);        // p>=skew implies skew<1
}

// Morph waveform selector (manual codes): 0 square, 1 sawtooth, 2 triangle,
// 3 ramp, 4 paraboloid, 5 sine, 6 cosine.
inline float waveByIndex(int i, float p, float skew, float pw) {
    switch (i) {
        case 0: return wSquare(p, pw);
        case 1: return wSaw(p);
        case 2: return wTriangle(p, skew);
        case 3: return wRamp(p);
        case 4: return wParabola(p);
        case 5: return wSine(p);
        default: return wCosine(p);           // 6
    }
}

} // namespace

class Lfo : public Circuit {
public:
    void tick(EngineState& s) override {
        // --- frequency (Hz) ---
        // rate is 1 V/oct: 1 V (0.1 engine units) doubles the frequency, so the
        // multiplier is 2^(rate*10). hz is a direct frequency (default 1.0);
        // with taptempo it acts as a multiplier of the tapped rate.
        float rate    = in("rate").value(s);
        float hzInput = in("hz").value(s);
        float octMult = std::pow(2.0f, rate * 10.0f);
        float freq;
        if (in("taptempo").connected()) {
            if (tapGate_.risingEdge(in("taptempo").value(s)))
                tap_.record(s.tick, s.tickRateHz);
            freq = (1.0f / tap_.interval) * hzInput * octMult;
        } else {
            freq = hzInput * octMult;
        }

        // --- sync: a rising edge restarts the waveform at `syncphase` ---
        if (in("sync").connected() && syncGate_.risingEdge(in("sync").value(s))) {
            phase_ = wrap01(in("syncphase").value(s));
            hillInit_ = false;   // a fresh hill starts at the restart point
        }

        // --- effective phase (with the phase-shift input) ---
        float eff = wrap01(double(phase_) + in("phase").value(s));

        // --- randomize: redraw one attenuation per hill (phase wrap) ---
        // SPEC-GAP: the manual specifies randomization only qualitatively. We
        // redraw once per full cycle and apply a single attenuation to every
        // output; bipolar half-hills, per-waveform-independent hills, and
        // skew/pulsewidth-shifted hill boundaries are not modelled. RNG only
        // drawn when randomize>0 so other circuits' determinism is unaffected.
        float randomize = in("randomize").value(s);
        bool newHill = !hillInit_ || (eff < prevEff_);
        if (newHill) {
            hillAtt_ = randomize > 0.0f
                     ? 1.0f - randomize * (1.0f - randUniform(s.rngState))
                     : 1.0f;
            hillInit_ = true;
        }
        prevEff_ = eff;

        // --- shaping parameters ---
        float level  = in("level").value(s);
        float offset = in("offset").value(s);
        bool  bipolar = in("bipolar").value(s) >= kGateHighThreshold;
        float pw   = clamp01(in("pulsewidth").value(s));
        float skew = clamp01(in("skew").value(s));

        auto emit = [&](const char* jack, float u) {
            float swing = bipolar ? (2.0f * u - 1.0f) : u;
            out(jack).set(s, swing * hillAtt_ * level + offset);
        };

        emit("triangle",   wTriangle(eff, skew));
        emit("sawtooth",   wSaw(eff));
        emit("ramp",       wRamp(eff));
        emit("square",     wSquare(eff, pw));
        emit("sine",       wSine(eff));
        emit("cosine",     wCosine(eff));
        emit("paraboloid", wParabola(eff));

        // --- morph output ---
        float wf = in("waveform").value(s);
        if (wf < 0.0f) wf = 0.0f; else if (wf > 6.0f) wf = 6.0f;
        int lo = int(wf);
        int hi = lo < 6 ? lo + 1 : 6;
        float frac = wf - float(lo);
        float morph = waveByIndex(lo, eff, skew, pw) * (1.0f - frac) +
                      waveByIndex(hi, eff, skew, pw) * frac;
        emit("output", morph);

        // --- advance phase for next tick ---
        phase_ = wrap01(double(phase_) + double(freq) * s.secondsPerTick());
    }

private:
    double phase_ = 0.0;
    float  prevEff_ = 0.0f;
    bool   hillInit_ = false;
    float  hillAtt_ = 1.0f;
    GateReader syncGate_;
    GateReader tapGate_;
    TapTempo tap_;   // basics.md 3.3 estimator (shared, see taptempo.hpp)
};

DROID_REGISTER_CIRCUIT(lfo, Lfo)

} // namespace droid
