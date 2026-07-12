#pragma once
// Shared value engine for the motor-fader circuits (motorfader, faderbank,
// fadermatrix). Like encodercore.hpp, this factors the genuinely-common logic —
// notch quantization, pitch-bend auto-return, the resting position the motor
// holds, and the pre-scale output value — so each circuit only orchestrates its
// own select/preset/LED/sharewithnext behaviour around it.
//
// A motor fader's virtual value is a raw position in [0,1]. What a circuit does
// with it depends on the `notches` config:
//   * notches <= 0 : continuous. output == position (0..1).
//   * notches == 1 : pitch-bend. output == position; when the plate is NOT
//     touched the fader auto-returns to the centre 0.5 (manual: "as soon as you
//     release it, it snaps back to the middle").
//   * notches >= 2 : discrete dents. The position snaps to the nearest of
//     `notches` evenly-spaced rest points i/(notches-1), i in 0..notches-1, and
//     the output is the integer dent index i (manual: notches=2 -> 0/1 binary
//     switch; notches=8 -> 0..7). The rest points put dent 0 at the bottom
//     (0.0) and the top dent at 1.0, matching the binary-switch description.
//
// SPEC-GAPs (manual silent / hardware-only, documented like slew/encodercore):
//   * motor speed: instant (see controllerstate.hpp commandFader) — no spec.
//   * force-feedback FEEL (dent strength, the >25-notch "force feedback off"
//     threshold, pitch-bend centring force) is hardware-only; we model only the
//     value/position it produces, not the haptics. The value quantization still
//     uses the raw notch count above 25.
//   * startvalue is the fader's initial OUTPUT value, not a raw 0..1 position:
//     for a notched fader it names the dent index (matching `output`, which
//     emits 0..notches-1), and for a continuous fader it is the raw 0..1
//     position (output == position there). This is what real patches rely on —
//     e.g. the MFPS clock BPM digits declare `n = 10, sv = 2` to boot showing
//     "2", and `n = 25, sv = 12` to boot at the centre dent. Treating sv as a
//     raw position instead clamped every sv>=1 to the top dent (see seed()).
#include <cmath>

namespace droid::fadercore {

inline float clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

// The dent index (0..notches-1) a raw position rests in (notches >= 2).
inline int notchIndex(float pos, int notches) {
    int i = (int)std::lround(pos * float(notches - 1));
    return i < 0 ? 0 : (i > notches - 1 ? notches - 1 : i);
}
// The rest position (0..1) of a dent index (notches >= 2).
inline float notchRest(int index, int notches) {
    return notches <= 1 ? 0.0f : float(index) / float(notches - 1);
}

// The output value (pre outputscale/offset) for a settled position + notches.
inline float outputOf(float pos, int notches) {
    pos = clampf(pos, 0.0f, 1.0f);
    if (notches < 2) return pos;                 // continuous / pitch-bend: raw
    return float(notchIndex(pos, notches));      // discrete: dent index
}

// Evaluate a raw source position under the notch config + touch state:
//   .position = where the motor should hold the fader (written back, read by
//               `expect F<n>`); .output = the pre-scale output value.
struct Result { float position; float output; };
inline Result evaluate(float pos, int notches, bool touched) {
    pos = clampf(pos, 0.0f, 1.0f);
    if (notches <= 0) return {pos, pos};             // continuous 0..1
    if (notches == 1) {                              // pitch-bend
        if (!touched) pos = 0.5f;                    // auto-return to centre
        return {pos, pos};
    }
    int idx = notchIndex(pos, notches);              // discrete dents
    return {notchRest(idx, notches), float(idx)};
}

// Seed a rest position from startvalue, where startvalue is the desired initial
// OUTPUT value (see the SPEC-GAP note above), inverting outputOf():
//   * continuous (notches <= 0): startvalue is the raw 0..1 position.
//   * pitch-bend (notches == 1): always rests at the centre 0.5 (the output at
//     rest); startvalue does not name a stable position here.
//   * discrete (notches >= 2): startvalue is the dent INDEX (0..notches-1); the
//     rest position is that dent's grid point i/(notches-1).
inline float seed(float startvalue, int notches) {
    if (notches <= 0) return clampf(startvalue, 0.0f, 1.0f);   // continuous
    if (notches == 1) return 0.5f;                             // pitch-bend centre
    int idx = (int)std::lround(startvalue);                    // discrete dent index
    if (idx < 0) idx = 0;
    if (idx > notches - 1) idx = notches - 1;
    return notchRest(idx, notches);
}

} // namespace droid::fadercore
