// spring — Physical spring simulation. Spec: manual/circuits/spring.md. A mass
// on an ideal spring: position p (0 = top), velocity v, integrated from the
// normalized ODE (every parameter defaults to 1):
//   a = (gravity - springforce*p - flowresistance*v - friction*sign(v)
//        + (shove ? shoveforce : 0)) / mass
// gravity pulls the mass down (increasing p); the spring pulls it back up; the
// two frictions dampen; shove adds a downward force while its gate is high.
// `speed` scales perceived time on a 1 V/oct base (+0.1 doubles it). reset (and
// the initial load) sets p/v to startposition/startvelocity.
//
// SPEC-GAP (spec_gap: true — the manual gives the ODE but NOT the numerics):
//   * Integration scheme/step size is unspecified. We use semi-implicit
//     (symplectic) Euler with dt = secondsPerTick * 2^(10*speed). Exact traces
//     are integrator-specific and unverified vs hardware.
//   * Output scaling: 1 sim unit -> 1 V (output = sim * 0.1 engine units). This
//     makes the documented ±10 (m, m/s) sim clamp coincide with the ±10 V CV
//     clamp — the self-consistent reading, but the manual never states the
//     volt<->unit mapping explicitly. The startposition/startvelocity inputs
//     live in the SAME CV domain as the outputs (1 V = 1 sim unit), so they are
//     divided by kScale on reset/init; ±1.0 engine (±10 V) spans the full ±10
//     sim clamp, and the position/velocity outputs read back the start CV.
//   * reset/init tick reports the fresh state without integrating (so the exact
//     start is observable); constant `friction` is 0 at v=0 (no static term).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>

namespace droid {

class Spring : public Circuit {
    static constexpr float kLimit = 10.0f;   // ±10 m / ±10 m/s sim clamp
    static constexpr float kScale = 0.1f;    // 1 sim unit -> 1 V (0.1 engine)

public:
    void tick(EngineState& s) override {
        bool resetEdge = resetGate_.risingEdge(in("reset").value(s));

        if (!inited_ || resetEdge) {
            // startposition/startvelocity are in the same CV domain as the
            // outputs (1 V = 1 sim unit), so convert engine units -> sim units
            // (/kScale) before clamping. This makes the position/velocity
            // outputs read back the same CV after a reset/init.
            p_ = clampL(in("startposition").value(s) / kScale);
            v_ = clampL(in("startvelocity").value(s) / kScale);
            inited_ = true;
            emit(s);                          // report the fresh state, no step
            return;
        }

        float mass = in("mass").value(s);
        if (mass == 0.0f) mass = 1e-6f;       // avoid divide-by-zero
        float gravity = in("gravity").value(s);
        float springforce = in("springforce").value(s);
        float flow = in("flowresistance").value(s);
        float friction = in("friction").value(s);
        float shoveForce = (in("shove").value(s) >= kGateHighThreshold)
                           ? in("shoveforce").value(s) : 0.0f;
        float sgn = v_ > 0.0f ? 1.0f : (v_ < 0.0f ? -1.0f : 0.0f);

        float a = (gravity - springforce * p_ - flow * v_ - friction * sgn + shoveForce) / mass;

        double dt = (double)s.secondsPerTick() * std::exp2(10.0 * (double)in("speed").value(s));
        v_ = clampL((float)(v_ + a * dt));    // semi-implicit Euler
        p_ = clampL((float)(p_ + v_ * dt));
        emit(s);
    }

private:
    static float clampL(float x) { return x > kLimit ? kLimit : (x < -kLimit ? -kLimit : x); }
    void emit(EngineState& s) {
        out("position").set(s, p_ * kScale);
        out("velocity").set(s, v_ * kScale);
    }

    GateReader resetGate_;
    float p_ = 0.0f, v_ = 0.0f;
    bool inited_ = false;
};

DROID_REGISTER_CIRCUIT(spring, Spring)

} // namespace droid
