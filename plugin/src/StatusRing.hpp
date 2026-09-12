#pragma once
// The module halo (issue #46) — the PAINT half of the master's visible error
// state. What colour, and whether there is a ring at all, is decided by the
// Rack-free model in MasterStatus.hpp; this file only knows how to put it on
// the screen.
//
// Why a ring around the whole module rather than a widget on the faceplate: the
// faceplates are photoreal derivatives of the hardware, and the hardware has no
// such indicator — so anything painted ON the panel is a lie about the machine
// being emulated. A ring around the module is unmistakably Rack's own
// vocabulary (it is how Rack marks a selected module), it needs no panel real
// estate, it reads from across a large patch at any zoom, and it works
// identically on the MASTER18, which has no LED matrix at all to blink a code
// on. Design decision: issue #46, option C.
//
// It is drawn in drawLayer(args, 1), the pass Rack reserves for LED light and
// halo, for three reasons: that pass runs after every module's panel so a
// neighbour cannot paint over the ring; it is not clipped to the module box, so
// the glow bleeds over the rails exactly like a bright edge LED does; and it is
// the pass users already expect the "Halo brightness" preference to govern.
#include "plugin.hpp"
#include "MasterStatus.hpp"

namespace dw {

// Inset or bleeding?
//
// Bleeding (false) puts the ring ON the module's edge and lets the glow spill
// over the rails and the neighbouring modules — the LED-halo behaviour the
// light layer exists for, and what the design mocked up. Inset (true) keeps the
// whole ring inside the module box, which never touches a neighbour but eats a
// couple of px of faceplate.
//
// The design deliberately deferred this call until the ring can be judged in a
// real rack, so it is one constant and nothing else: flipping it is the whole
// change.
inline constexpr bool kStatusRingInset = false;

// Ring geometry, in module-local px (Rack's own units: RACK_GRID_WIDTH = 15 px
// per HP at 100 % zoom, scaled by the rack's zoom like everything else).
inline constexpr float kStatusRingWidth  = 2.f;    // the solid core ring
inline constexpr float kStatusRingGlow   = 14.f;   // how far the glow reaches
inline constexpr float kStatusRingRadius = 3.f;    // corner rounding
inline constexpr float kStatusRingGlowAlpha = 0.55f;

// Paint the ring. `vg` must already be in the module widget's local
// coordinates, i.e. this is called straight from ModuleWidget::drawLayer.
//
// The glow is scaled by settings::haloBrightness like every other halo in Rack
// (a user who has turned halos off gets no bleed). The solid core ring is NOT:
// at haloBrightness 0 the halo setting would otherwise hide the only sign that
// a master is refusing to run, which is the one thing this indicator exists to
// prevent. It stays a thin, hard 2 px line either way.
inline void drawStatusRing(NVGcontext* vg, rack::math::Vec boxSize, NVGcolor c) {
    float halo = rack::math::clamp(rack::settings::haloBrightness, 0.f, 1.f);
    if (halo > 0.f) {
        NVGcolor inner = c, outer = c;
        inner.a = kStatusRingGlowAlpha * halo;
        outer.a = 0.f;
        NVGpaint paint = nvgBoxGradient(vg, 0.f, 0.f, boxSize.x, boxSize.y,
                                        kStatusRingRadius, kStatusRingGlow,
                                        inner, outer);
        // Fill the ring of space AROUND the module only: the outer rect with
        // the module's own box punched out as a hole, so the glow never washes
        // over the faceplate art it is supposed to frame.
        nvgBeginPath(vg);
        nvgRect(vg, -kStatusRingGlow, -kStatusRingGlow,
                boxSize.x + 2 * kStatusRingGlow, boxSize.y + 2 * kStatusRingGlow);
        nvgRoundedRect(vg, 0.f, 0.f, boxSize.x, boxSize.y, kStatusRingRadius);
        nvgPathWinding(vg, NVG_HOLE);
        nvgFillPaint(vg, paint);
        nvgFill(vg);
    }
    // The core ring, centred either just inside or just outside the box edge.
    float d = kStatusRingInset ? kStatusRingWidth * 0.5f : -kStatusRingWidth * 0.5f;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, d, d, boxSize.x - 2 * d, boxSize.y - 2 * d, kStatusRingRadius);
    nvgStrokeWidth(vg, kStatusRingWidth);
    nvgStrokeColor(vg, c);
    nvgStroke(vg);
}

inline NVGcolor toNVG(const vcvoid::status::RGB& c) {
    return nvgRGBf(c.r, c.g, c.b);
}

}  // namespace dw
