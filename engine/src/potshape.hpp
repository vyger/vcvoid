#pragma once
// Shared pot-shaping math for the pot-family circuits (pot, notchedpot,
// switchedpot). Only the behaviors the manual describes *identically* across
// those docs live here; per-circuit logic (presets, discrete, select, ...)
// stays in each circuit.
#include <cmath>

namespace droid::potshape {

// Center notch: a "dead zone" of total width `notch` (fraction of the 0..1
// travel) around 0.5 that maps to exactly 0.5. The two outer regions are
// linearly rescaled so the endpoints still reach 0.0 and 1.0. `x`, return are
// in [0,1]; `notch` is clamped to [0, 0.5] (0 disables it). Matches
// notchedpot.md / pot.md ("range of tolerance considered to be the center";
// endpoints preserved so e.g. bipolar keeps its full range).
inline float applyNotch(float x, float notch) {
    if (x < 0.0f) x = 0.0f; else if (x > 1.0f) x = 1.0f;
    if (notch <= 0.0f) return x;
    if (notch > 0.5f) notch = 0.5f;
    float half = notch * 0.5f;      // <= 0.25, so lo > 0 and (1-hi) > 0
    float lo = 0.5f - half;
    float hi = 0.5f + half;
    if (x < lo) return x / lo * 0.5f;
    if (x > hi) return 0.5f + (x - hi) / (1.0f - hi) * 0.5f;
    return 0.5f;
}

// The six "hemisphere" outputs shared by pot and notchedpot, derived from a
// value `v` in [0,1] and scaled by `scale` (notchedpot passes 1.0; pot passes
// `outputscale`). See the Outputs tables of both docs — the formulas are
// identical, notchedpot being the scale==1 case.
struct Hemispheres {
    float bipolar, absbipolar;
    float lefthalf, righthalf;
    float lefthalfinv, righthalfinv;
};
inline Hemispheres hemispheres(float v, float scale) {
    float leftU  = v < 0.5f ? (1.0f - 2.0f * v) : 0.0f;   // 1 at left end, 0 at/after mid
    float rightU = v > 0.5f ? (2.0f * v - 1.0f) : 0.0f;   // 0 up to mid, 1 at right end
    Hemispheres h;
    h.bipolar      = (2.0f * v - 1.0f) * scale;
    h.absbipolar   = std::fabs(2.0f * v - 1.0f) * scale;
    h.lefthalf     = leftU * scale;
    h.righthalf    = rightU * scale;
    h.lefthalfinv  = (1.0f - leftU) * scale;
    h.righthalfinv = (1.0f - rightU) * scale;
    return h;
}

// Virtual-pot "pickup": advance a virtual value `virt` toward the physical pot
// so that, turning in one direction, both reach the same endpoint (0.0 or 1.0)
// together. `prevPhys`/`phys` are the physical pot position (0..1) before and
// after this tick. When turning right the virtual value gains distance
// proportional to (1-virt)/(1-prevPhys); when turning left it loses
// virt/prevPhys. At the boundary (prevPhys 1.0 going up, or 0.0 going down)
// the virtual value cannot move further. Word-for-word identical algorithm in
// pot.md ("Picking up the pots") and switchedpot.md.
inline float pickup(float virt, float prevPhys, float phys) {
    float d = phys - prevPhys;
    if (d > 0.0f) {
        float denom = 1.0f - prevPhys;
        if (denom > 1e-6f) virt += d * (1.0f - virt) / denom;
    } else if (d < 0.0f) {
        float denom = prevPhys;
        if (denom > 1e-6f) virt += d * virt / denom;   // d < 0 -> virt decreases
    }
    if (virt < 0.0f) virt = 0.0f; else if (virt > 1.0f) virt = 1.0f;
    return virt;
}

} // namespace droid::potshape
