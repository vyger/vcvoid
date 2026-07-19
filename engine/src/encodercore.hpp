#pragma once
// Shared virtual-value engine for the encoder-family circuits (encoder,
// encoderbank). encoderbank.md: "shares almost all features with the single
// encoder circuit" — only movement triggers, override and sharewithnext are
// encoder-only, so everything below (mode/range mapping, sensivity, autozoom,
// notch, snapto, smoothing, discrete, start/clear seeding) is genuinely common
// and factored here. Per-circuit orchestration (select gating, presets,
// movement triggers) stays in each circuit.
//
// SPEC-GAP notes (manual qualitative; chosen mappings documented, like slew):
//   * sensivity: manual pins the linear cases exactly — one 360 turn = 96
//     detents = a full 0..1 sweep (normal/bipolar/circular) or +sensivity of
//     value (infinity), and "one turn changes the virtual switch by eight
//     positions" in discrete. So base step per detent is sensivity/96 (finite &
//     infinite) or sensivity*8/96 = sensivity/12 (discrete). Exact.
//   * autozoom: purely qualitative ("fine when slow, coarse when fast"). We
//     scale a tick's movement by pow(|detents_this_tick|, autozoom*kZoomExp),
//     so a burst of N detents in one tick moves superlinearly more than N
//     single-detent ticks. Monotonic and off at autozoom=0; the exact curve is
//     unverified vs hardware.
//   * smooth: "essentially the same as slew" but no time constant given. We use
//     a one-pole low-pass on the logical value with tau = smooth*kSmoothMax.
//     smooth=0 disables it (exact steps); not applied in discrete mode.
//   * snapto/snapforce: pitch-bend return; no curve given. One-pole pull of the
//     position toward the snap target with rate snapforce*kSnapRate per second.
//     snapforce=0 or snapto unconnected disables it.
#include <cmath>

namespace droid::encodercore {

constexpr float kZoomExp    = 1.5f;   // SPEC-GAP autozoom exponent
constexpr float kSmoothMax  = 0.05f;  // SPEC-GAP max smoothing tau (seconds)
constexpr float kSnapRate   = 8.0f;   // SPEC-GAP snapto pull rate (1/seconds)

struct Params {
    int   mode = 1;          // 0..6 (0 = off, ignored when discrete>=2)
    int   discrete = 0;      // 0 = continuous, else >=2 positions
    float sensivity = 1.0f;
    float autozoom = 0.0f;
    float notch = 0.0f;
    float outputscale = 1.0f;
    float outputoffset = 0.0f;
    float startvalue = 0.0f;
    float snapto = 0.0f;
    bool  snapConnected = false;
    float snapforce = 0.5f;
    float smooth = 0.5f;
};

inline float clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}
inline float wrap01(float x) {
    x -= std::floor(x);                // into [0,1)
    if (x >= 1.0f) x -= 1.0f;          // guard fp edge
    return x;
}

// Center notch (modes 1/2): a flat dead-zone of width `notch` (fraction of the
// turn) around the center, endpoints preserved — identical to the pot notch.
inline float applyNotch(float p, float notch) {
    if (notch <= 0.0f) return p;
    if (notch > 0.5f) notch = 0.5f;
    float half = notch * 0.5f;
    float lo = 0.5f - half, hi = 0.5f + half;
    if (p < lo) return p / lo * 0.5f;
    if (p > hi) return 0.5f + (p - hi) / (1.0f - hi) * 0.5f;
    return 0.5f;
}

// One encoder's mutable virtual state.
struct State {
    float pos = 0.0f;       // raw internal: p in [0,1] for finite/circular modes,
                            // the value itself for infinity, a float position
                            // accumulator for discrete.
    float smoothed = 0.0f;  // low-pass output of the logical value (pre-scale)
    bool  seeded = false;

    // Seed pos from startvalue (also used by clear). startvalue is in the
    // "logical position" space where 0.5 == center (manual).
    void seed(const Params& p) {
        if (p.discrete >= 2) {
            long b = std::lround(p.startvalue);
            pos = float(clampf(float(b), 0.0f, float(p.discrete - 1)));
        } else switch (p.mode) {
            case 3:  pos = p.startvalue < 0.0f ? 0.0f : p.startvalue; break;
            case 4:  pos = p.startvalue > 0.0f ? 0.0f : p.startvalue; break;
            case 5:  pos = p.startvalue; break;
            case 6:  pos = wrap01(clampf(p.startvalue, 0.0f, 1.0f)); break;
            default: pos = clampf(p.startvalue, 0.0f, 1.0f); break;  // 0,1,2
        }
        smoothed = logical(p);
        seeded = true;
    }

    // Apply this tick's detents (caller has already gated on select/override).
    void applyMovement(const Params& p, long detents) {
        if (detents == 0) return;
        float zoom = 1.0f;
        if (p.autozoom > 0.0f && p.discrete < 2) {
            long a = detents < 0 ? -detents : detents;
            zoom = std::pow(float(a), p.autozoom * kZoomExp);
        }
        if (p.discrete >= 2) {
            pos += float(detents) * (p.sensivity / 12.0f);
            pos = clampf(pos, 0.0f, float(p.discrete - 1));
            return;
        }
        float step = float(detents) * (p.sensivity / 96.0f) * zoom;
        switch (p.mode) {
            case 3:  pos = std::max(0.0f, pos + step); break;
            case 4:  pos = std::min(0.0f, pos + step); break;
            case 5:  pos += step; break;
            case 6:  pos = wrap01(pos + step); break;
            default: pos = clampf(pos + step, 0.0f, 1.0f); break;  // 0,1,2
        }
    }

    // Pull the position toward snapto (pitch-bend return), if enabled.
    void applySnap(const Params& p, float dt) {
        if (!p.snapConnected || p.snapforce <= 0.0f || p.discrete >= 2) return;
        // snapto is a logical value; convert to pos-space for finite modes.
        float target;
        switch (p.mode) {
            case 2:  target = clampf((p.snapto + 1.0f) * 0.5f, 0.0f, 1.0f); break;
            case 5:  target = p.snapto; break;             // infinity: value-space
            case 3:  target = std::max(0.0f, p.snapto); break;
            case 4:  target = std::min(0.0f, p.snapto); break;
            case 6:  target = wrap01(p.snapto); break;
            default: target = clampf(p.snapto, 0.0f, 1.0f); break;  // 0,1
        }
        float a = 1.0f - std::exp(-dt * p.snapforce * kSnapRate);
        pos += (target - pos) * a;
    }

    // Discrete switch index (only meaningful when discrete>=2).
    int index(const Params& p) const {
        long i = std::lround(pos);
        return int(clampf(float(i), 0.0f, float(p.discrete - 1)));
    }

    // Logical value (pre outputscale/offset) from pos.
    float logical(const Params& p) const {
        if (p.discrete >= 2) return float(index(p));
        switch (p.mode) {
            case 2:  return 2.0f * applyNotch(pos, p.notch) - 1.0f;
            case 3: case 4: case 5: return pos;   // infinity: value-space
            case 6:  return pos;                  // circular 0..1
            default: return applyNotch(pos, p.notch);  // 0,1: normal 0..1
        }
    }

    // Advance smoothing toward the logical value and return the emitted value
    // (pre-scale). Discrete mode returns the exact index (no smoothing).
    float smoothStep(const Params& p, float dt) {
        float target = logical(p);
        if (p.discrete >= 2 || p.smooth <= 0.0f) { smoothed = target; return target; }
        float tau = p.smooth * kSmoothMax;
        float a = 1.0f - std::exp(-dt / tau);
        smoothed += (target - smoothed) * a;
        return smoothed;
    }

    // Final output = emitted value * outputscale + outputoffset.
    float output(const Params& p, float emitted) const {
        if (p.discrete >= 2) return float(index(p)) * p.outputscale + p.outputoffset;
        return emitted * p.outputscale + p.outputoffset;
    }
};

// Default ring colour when the `color` param is unconnected. SPEC-GAP: the
// manual marks `color` as a smart default without naming it; blue matches the
// documented "blue LED gauge" of the fader/encoder value displays.
constexpr float kDefaultRingColor = 1.2f;   // blue (droidcolor table)

// Ring-display value for the E4/DB8E 32-LED ring (panel-only; issue #15).
// Returns the dot value and whether the ring renders bipolar (zero at
// top-center) — see controllerstate.hpp RingDisplay for the geometry contract.
// SPEC-GAP: the manual doesn't describe the ring image of the infinity modes
// (3/4/5); we wrap the position so the dot keeps moving with the value.
inline void ringDisplayValue(const State& st, const Params& p,
                             bool& bipolar, float& v) {
    if (p.discrete >= 2) {
        bipolar = false;
        v = float(st.index(p)) / float(p.discrete - 1);
        return;
    }
    switch (p.mode) {
        case 2:  bipolar = true;  v = 2.0f * applyNotch(st.pos, p.notch) - 1.0f; break;
        case 5:  bipolar = true;  v = st.pos - 2.0f * std::floor((st.pos + 1.0f) * 0.5f); break;
        case 3: case 4:
                 bipolar = false; v = st.pos - std::floor(st.pos); break;
        case 6:  bipolar = false; v = st.pos; break;
        default: bipolar = false; v = applyNotch(st.pos, p.notch); break;   // 0,1
    }
}

// Clamp an override value (logical units) into the mode/discrete range.
inline float clampOverride(const Params& p, float v) {
    if (p.discrete >= 2) {
        long i = std::lround(v);
        return clampf(float(i), 0.0f, float(p.discrete - 1));
    }
    switch (p.mode) {
        case 2:  return clampf(v, -1.0f, 1.0f);
        case 3:  return std::max(0.0f, v);
        case 4:  return std::min(0.0f, v);
        case 5:  return v;
        case 6:  return wrap01(v);
        default: return clampf(v, 0.0f, 1.0f);
    }
}

} // namespace droid::encodercore
