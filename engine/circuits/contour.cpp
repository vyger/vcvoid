// contour — Contour (enhanced ADSR) generator with six phases: predelay,
// attack, hold, decay, sustain, release. Spec: manual/circuits/contour.md.
// Standard A*B+C input math applies. Timings: attack = value*0.1 s (value 1 =>
// 0.1 s), decay/release = value*2.0 s, predelay/hold = value seconds; pitch
// (1V/oct) scales everything except attack (0.1 => 2x speed). `trigger` mode
// skips decay/sustain (attack+hold -> release). level scales the output;
// velocity sets the attack peak (captured at attack start).
//
// Gate semantics (manual 141/145): predelay and attack always run to completion
// once started (a gate ending during predelay cancels the envelope; abortattack
// cuts attack short and moves on to hold AT THE CURRENT LEVEL). After hold, a
// low gate routes straight to release (decay/sustain skipped -- so short gates
// and gateless loop mode never park at sustain); during decay/sustain the gate
// LEVEL (not just its falling edge) is what keeps the envelope from releasing.
//
// SPEC-GAP (spec_gap: true): the bent shape curves (shape/attackshape/
// decayshape/swellshape/releaseshape away from 0.5) and the swell curve are only
// described qualitatively -- implemented here as a power curve / linear swell,
// unverified vs hardware. zerocrossing and taptempo are not modelled (noted).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>

namespace droid {

class Contour : public Circuit {
    enum Phase { IDLE, PREDELAY, ATTACK, HOLD, DECAY, SUSTAIN, RELEASE };

public:
    void tick(EngineState& s) override {
        float dt = s.secondsPerTick();
        float levelP    = in("level").value(s);
        float sustain   = in("sustain").value(s);
        bool  useTrigger = in("trigger").connected();
        bool  loop      = in("loop").value(s) >= kGateHighThreshold;
        bool  startFromZero = in("startfromzero").value(s) >= kGateHighThreshold;
        bool  abortAttack   = in("abortattack").value(s) >= kGateHighThreshold;
        bool  retrigger = in("retrigger").value(s) >= kGateHighThreshold;

        // pitch scales all timings except attack (1 V => 2x speed).
        float speed = std::pow(2.0f, in("pitch").value(s) * 10.0f);
        float preT  = in("predelay").value(s) / speed;
        float atkT  = in("attack").value(s) * 0.1f;             // NOT pitch-scaled
        float holdT = in("hold").value(s) / speed;
        float decT  = in("decay").value(s) * 2.0f / speed;
        float relT  = in("release").value(s) * 2.0f / speed;

        // --- trigger detection ---
        bool gateNow = in("gate").value(s) >= kGateHighThreshold;
        bool startEvt = useTrigger ? trigEdge_.risingEdge(in("trigger").value(s))
                                    : gateEdge_.risingEdge(in("gate").value(s));
        bool gateFell = !useTrigger && !gateNow && gateWasHigh_;
        gateWasHigh_ = gateNow;

        // A start restarts unless retrigger is off and we're not idle/releasing.
        if (startEvt && (retrigger || phase_ == IDLE || phase_ == RELEASE)) {
            velocity_ = in("velocity").value(s);          // capture attack peak
            if (startFromZero) level_ = 0.0f;
            enter(preT > 0.0f ? PREDELAY : ATTACK, s);
        }
        // Loop restarts on its own once idle.
        if (loop && phase_ == IDLE) {
            velocity_ = in("velocity").value(s);
            if (startFromZero) level_ = 0.0f;
            enter(preT > 0.0f ? PREDELAY : ATTACK, s);
        }

        // gate release semantics. Predelay/attack finalize regardless of the
        // gate unless abortattack cuts attack short into hold (manual 145);
        // decay/sustain release on the gate LEVEL, not the falling edge, so a
        // gate that ended back during attack still releases after hold.
        if (!useTrigger && gateFell) {
            if (phase_ == PREDELAY) { enter(IDLE, s); level_ = 0.0f; }
            else if (phase_ == ATTACK && abortAttack) enter(HOLD, s);
        }
        if (!useTrigger && !gateNow && (phase_ == DECAY || phase_ == SUSTAIN))
            enter(RELEASE, s);

        // Advance the current phase. Timed phases with a non-positive duration
        // are skipped instantly.
        int guard = 0;   // avoid infinite skips through zero-length phases
        while (guard++ < 8) {
            bool advanced = false;
            switch (phase_) {
                case IDLE:    break;
                case PREDELAY:
                    level_ = startBase_;
                    if (stepDone(preT, dt)) { emit(epUntil_, s); phase_ = ATTACK; enterReset(s); advanced = true; }
                    break;
                case ATTACK: {
                    float p = progress(atkT);
                    level_ = lerp(startBase_, velocity_, curve(p, shapeOf(s, "attackshape")));
                    if (stepDone(atkT, dt)) { level_ = velocity_; emit(eaUntil_, s); phase_ = HOLD; enterReset(s); advanced = true; }
                    break; }
                case HOLD:
                    level_ = startBase_;   // completed attack's peak, or the
                                           // aborted attack's current level
                    if (stepDone(holdT, dt)) { emit(ehUntil_, s);
                        // Trigger mode and a low gate skip decay/sustain.
                        phase_ = (useTrigger || !gateNow) ? RELEASE : DECAY;
                        enterReset(s); advanced = true; }
                    break;
                case DECAY: {
                    float p = progress(decT);
                    level_ = lerp(velocity_, sustain, curve(p, shapeOf(s, "decayshape")));
                    if (stepDone(decT, dt)) { level_ = sustain; emit(edUntil_, s); phase_ = SUSTAIN; enterReset(s); advanced = true; }
                    break; }
                case SUSTAIN:
                    level_ = sustain;             // (swell not modelled; SPEC-GAP)
                    break;                        // stays until gate release
                case RELEASE: {
                    float p = progress(relT);
                    level_ = lerp(startBase_, 0.0f, curve(p, shapeOf(s, "releaseshape")));
                    if (stepDone(relT, dt)) { level_ = 0.0f; emit(erUntil_, s); phase_ = IDLE; advanced = true; }
                    break; }
            }
            if (!advanced) break;
        }

        float outv = level_ * levelP;
        out("output").set(s, outv);
        out("negated").set(s, -outv);
        out("inverted").set(s, levelP - outv);   // level when output 0, 0 when output=level
        out("endofpredelay").set(s, s.tick < epUntil_ ? 1.0f : 0.0f);
        out("endofattack").set(s,   s.tick < eaUntil_ ? 1.0f : 0.0f);
        out("endofhold").set(s,     s.tick < ehUntil_ ? 1.0f : 0.0f);
        out("endofdecay").set(s,    s.tick < edUntil_ ? 1.0f : 0.0f);
        out("endofrelease").set(s,  s.tick < erUntil_ ? 1.0f : 0.0f);
    }

private:
    void enter(Phase p, EngineState& s) { phase_ = p; phaseElapsed_ = 0.0; startBase_ = level_; (void)s; }
    void enterReset(EngineState&) { phaseElapsed_ = 0.0; startBase_ = level_; }
    // progress within the current phase using elapsed-before-advance.
    double progress(float T) { return T > 0.0f ? phaseElapsed_ / T : 1.0; }
    bool stepDone(float T, float dt) { phaseElapsed_ += dt; return T <= 0.0f || phaseElapsed_ >= T; }
    static float lerp(float a, float b, float t) { return a + (b - a) * t; }
    static void emit(uint64_t& until, EngineState& s) { long d = std::lround(0.01 * s.tickRateHz); if (d < 1) d = 1; until = s.tick + (uint64_t)d; }

    // shape: dedicated <phase>shape if patched, else `shape`. 0.5 linear,
    // 1 exponential (fast start), 0 logarithmic (slow start).
    float shapeOf(EngineState& s, const char* jack) {
        return in(jack).connected() ? in(jack).value(s) : in("shape").value(s);
    }
    static float curve(float p, float shape) {
        if (p <= 0.0f) return 0.0f; if (p >= 1.0f) return 1.0f;
        if (shape == 0.5f) return p;               // linear (exact)
        float e = std::pow(4.0f, 0.5f - shape);    // SPEC-GAP bend
        return std::pow(p, e);
    }

    Phase phase_ = IDLE;
    double phaseElapsed_ = 0.0;
    float level_ = 0.0f, startBase_ = 0.0f, velocity_ = 1.0f;
    bool  gateWasHigh_ = false;
    GateReader gateEdge_, trigEdge_;
    uint64_t epUntil_ = 0, eaUntil_ = 0, ehUntil_ = 0, edUntil_ = 0, erUntil_ = 0;
};

DROID_REGISTER_CIRCUIT(contour, Contour)

} // namespace droid
