// vcotuner — measure frequency and tuning of a VCO. Spec: manual/circuits/vcotuner.md.
//
// Reads the MASTER18 frequency probe (s.probe — the Rack adapter measures
// positive zero-crossings on I1 at full sample rate, 1 Hz..20 kHz; the engine
// tick rate is far too low for audio, so this circuit is pure math on that
// published value). MASTER18-only: the loader rejects it on a MASTER
// (loader.cpp, Forge parity circuitNeedsMaster18).
//
// Conventions (manual + engine): DROID pitch is 1 V/oct with C0 = 0 V and
// A1 = concertpitch (440 Hz default), i.e. f_C0 = concertpitch * 2^(-21/12) —
// A1 sits 21 semitones above C0. note = 12*log2(hz/f_C0) semitones above C0;
// pitch out = basepitch + note * 0.1/12 engine units. The reference is the
// nearest semitone (tuningnote = -1) or the nearest instance of pitch class
// `tuningnote` (0 = C, 1 = C#, ...). cents = (note - ref) * 100 (raw number,
// e.g. the manual's `override = _CENTS / 50` example); tuning = the same
// deviation expressed in 1 V/oct engine units (cents/1200 V). intune iff
// |cents| <= precision. led* outputs only work while selected (ui::isSelected);
// `intune` reports regardless.
//
// CHOSEN READINGS where the manual is silent (also logged in circuits-status.yaml):
//   * smoothing: one-pole low-pass on hz, tau = smooth^2 * 2 s ("per default
//     smoothing is just subtle" — smooth 0.5 -> 0.5 s); the first valid
//     measurement initializes the filter directly (no ramp from 0 Hz).
//   * led magnitude: 0.2 + 0.8 * (|cents| - precision) / (50 - precision),
//     clamped to 1 (the manual pins only the 0.2..1.0 range and the 50-cent
//     max distance); flat/sharp are mutually exclusive with in-tune.
//   * on signal loss: hz -> 0 (manual), vcofound -> 0; pitch/referencepitch
//     HOLD their last value (pitch-follower continuity); cents/tuning/leds -> 0.
//   * the DB8E tuner screen (display/header inputs) is NOT implemented in this
//     pass — both inputs are accepted but inert.
#include "../src/registry.hpp"
#include "../src/uihelpers.hpp"
#include <cmath>

namespace droid {

class VcoTuner : public Circuit {
    float smHz_ = 0.0f;    // one-pole smoothed frequency
    bool  have_ = false;   // filter initialized (a valid signal was seen)

public:
    void tick(EngineState& s) override {
        bool found = s.probe.present && s.probe.hz >= 1.0f && s.probe.hz <= 20000.0f;
        bool selected = ui::isSelected(*this, s);

        out("vcofound").set(s, found ? 1.0f : 0.0f);
        if (!found) {
            have_ = false;
            out("hz").set(s, 0.0f);            // manual: "If no signal is found, the output is 0"
            out("cents").set(s, 0.0f);
            out("tuning").set(s, 0.0f);
            out("intune").set(s, 0.0f);
            if (selected) {
                out("ledflat").set(s, 0.0f);
                out("ledsharp").set(s, 0.0f);
                out("ledintune").set(s, 0.0f);
            }
            return;                            // pitch/referencepitch hold
        }

        float sm = in("smooth").value(s);
        if (sm < 0.0f) sm = 0.0f;
        if (sm > 1.0f) sm = 1.0f;
        float tau = sm * sm * 2.0f;
        float alpha = tau <= 0.0f ? 1.0f
                                  : 1.0f - std::exp(-1.0f / (tau * s.tickRateHz));
        smHz_ = have_ ? smHz_ + alpha * (s.probe.hz - smHz_) : s.probe.hz;
        have_ = true;

        float cp = in("concertpitch").value(s);
        if (cp <= 0.0f) cp = 440.0f;
        float fC0 = cp * std::exp2(-21.0f / 12.0f);           // A1 = cp
        float note = 12.0f * std::log2(smHz_ / fC0);          // semitones above C0
        int tn = (int)std::lround(in("tuningnote").value(s)); // -1 = nearest semitone
        float ref = tn < 0 ? std::round(note)
                           : tn + 12.0f * std::round((note - tn) / 12.0f);
        float cents = (note - ref) * 100.0f;
        float prec = in("precision").value(s);
        bool intune = std::fabs(cents) <= prec;

        float base = in("basepitch").value(s);
        out("hz").set(s, smHz_);
        out("pitch").set(s, base + note * (0.1f / 12.0f));
        out("referencepitch").set(s, base + ref * (0.1f / 12.0f));
        out("cents").set(s, cents);
        out("tuning").set(s, cents * (0.1f / 1200.0f));       // deviation, 1 V/oct units
        out("intune").set(s, intune ? 1.0f : 0.0f);
        if (selected) {
            float span = 50.0f - prec;
            float mag = intune ? 0.0f
                : 0.2f + 0.8f * std::fmin(1.0f, (std::fabs(cents) - prec) /
                                                std::fmax(1.0f, span));
            out("ledflat").set(s, (!intune && cents < 0.0f) ? mag : 0.0f);
            out("ledsharp").set(s, (!intune && cents > 0.0f) ? mag : 0.0f);
            out("ledintune").set(s, intune ? 1.0f : 0.0f);
        }
    }
};

DROID_REGISTER_CIRCUIT(vcotuner, VcoTuner)

} // namespace droid
