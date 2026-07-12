#pragma once
#include "plugin.hpp"
#include "droidcolor.hpp"
#include <cmath>
#include <algorithm>

// Shared endless-encoder + value-ring widgets for the DROID controllers that
// carry rotary encoders (E4, DB8E). SDK-verified against Rack 2 headers:
//   - Rack 2 event types are DragStartEvent / DragMoveEvent / ButtonEvent
//     (widget/Widget.hpp), NOT the old event::DragStart spelling.
//   - The drag delta is e.mouseDelta (a math::Vec); vertical = rotation.
//   - A stock Knob is a ParamWidget and needs a bound ParamQuantity, which an
//     endless encoder does not have (no bounded value). So we base the encoder
//     on OpaqueWidget: it consumes the left ButtonEvent on press (the required
//     precondition for DragStart/DragMove to fire) and lets us capture raw drag
//     without a param. We accumulate drag into a monotonic detent counter the
//     module publishes upstream; the master diffs it into turn events.

// Endless (infinite-rotation) encoder. The module owns a monotonic uint32_t
// detent counter (wraps) and a bool push level; the widget mutates them via
// these pointers.
//
// Turn vs push (issue 1 fix): a turn is a left-press + vertical drag, so the
// press alone cannot mean "push". We treat a press as an OPTIMISTIC push, then
// reclassify it as a turn — clearing `pushed` — the instant the knob is actually
// dragged. A press/release with no drag therefore stays a momentary push, while
// turning no longer registers as a push (which previously lit the whole ring
// white for the drag's duration via the default-LED-from-button rule).
struct DroidEndlessEncoder : OpaqueWidget {
    uint32_t* detentCount = nullptr;   // module-owned monotonic counter (wraps)
    bool* pushed = nullptr;            // module-owned push level
    float accum = 0.f;                 // sub-detent drag accumulator (px)
    static constexpr float kPxPerDetent = 4.f;   // sensitivity (feel; tune in Rack)
    // Visual detents per full revolution for the rotation indicator (feel only;
    // has no bearing on the logical detent count the master consumes).
    static constexpr int   kVisualDetentsPerRev = 24;

    void onDragMove(const DragMoveEvent& e) override {
        // Vertical drag = rotation (Rack convention): mouse up (dy < 0) = increase.
        float dy = -e.mouseDelta.y;
        if (dy != 0.f && pushed) *pushed = false;   // movement means turn, not push
        accum += dy;
        while (accum >= kPxPerDetent)  { if (detentCount) (*detentCount)++; accum -= kPxPerDetent; }
        while (accum <= -kPxPerDetent) { if (detentCount) (*detentCount)--; accum += kPxPerDetent; }
        OpaqueWidget::onDragMove(e);
    }
    void onButton(const ButtonEvent& e) override {
        // OpaqueWidget::onButton consumes the left press, enabling DragStart.
        if (e.button == GLFW_MOUSE_BUTTON_LEFT && pushed) {
            if (e.action == GLFW_PRESS)   *pushed = true;    // optimistic; onDragMove clears if turned
            if (e.action == GLFW_RELEASE) *pushed = false;
        }
        OpaqueWidget::onButton(e);
    }
    void onDragEnd(const DragEndEvent& e) override {
        // Belt-and-suspenders: a mouse RELEASE outside the widget box never
        // reaches onButton, which would leave *pushed stuck true. onDragEnd is
        // always delivered to the widget that started the drag, so clear here
        // too. Backward-compatible with E4 (points the same pushed pointer).
        if (pushed) *pushed = false;
        OpaqueWidget::onDragEnd(e);
    }

    // Draw the dark-cap/white-pointer style shared with dw::DroidKnob (panel
    // overhaul: faithful faceplates use one consistent knob look), but the
    // pointer angle comes from the wrapping detent count rather than a bound
    // ParamQuantity value — an endless encoder has no absolute position, so
    // this is turn feedback (a tick that spins with each detent), not a value
    // indicator. Benefits E4 and DB8E (issue 3b).
    void draw(const DrawArgs& args) override {
        Vec c = box.size.div(2);
        float r = std::min(box.size.x, box.size.y) / 2.f;
        // cap (matches dw::DroidKnob::draw)
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, c.x, c.y, r);
        nvgFillColor(args.vg, nvgRGB(0x14, 0x14, 0x14));
        nvgFill(args.vg);
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStrokeColor(args.vg, nvgRGB(0x30, 0x30, 0x30));
        nvgStroke(args.vg);

        // Pointer: angle from the (wrapping) detent count, 12 o'clock = 0.
        // Drawn in the faceplate art's style — a white centre disc with a
        // tapered wedge to the cap edge (the E4/DB8E baked knobs both carry
        // this print) — so the overlay matches what it covers. Since the
        // encoder is endless this is turn feedback, not a value indicator.
        uint32_t d = detentCount ? *detentCount : 0u;
        float frac = (float)(d % kVisualDetentsPerRev) / (float)kVisualDetentsPerRev;
        float angle = 2.f * (float)M_PI * frac;
        nvgSave(args.vg);
        nvgTranslate(args.vg, c.x, c.y);
        nvgRotate(args.vg, angle);
        nvgFillColor(args.vg, nvgRGB(0xf0, 0xf0, 0xf0));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, 0.f, 0.f, r * 0.51f);   // centre disc (art ~0.51 r)
        nvgFill(args.vg);
        nvgBeginPath(args.vg);                     // wedge, seamless with disc
        nvgMoveTo(args.vg, -r * 0.20f, -r * 0.30f);
        nvgLineTo(args.vg,  r * 0.20f, -r * 0.30f);
        nvgLineTo(args.vg,  r * 0.09f, -r * 0.93f);
        nvgLineTo(args.vg, -r * 0.09f, -r * 0.93f);
        nvgClosePath(args.vg);
        nvgFill(args.vg);
        nvgRestore(args.vg);

        OpaqueWidget::draw(args);
    }
};

// Draw a 32-LED value ring. `value` (0..1) lights one position on the circle
// (the DROID encoder value dot); `overlay` (0..1) is the white L-register
// brightness added to every LED. Panel-only helper, called from a widget's
// draw(). Reused by E4 and DB8E. Simple geometry: 32 dots on a circle, 12
// o'clock = position 0, clockwise.
inline void drawValueRing(NVGcontext* vg, Vec center, float radius, float value, float overlay) {
    const int N = 32;
    int lit = (int)std::round(std::max(0.f, std::min(1.f, value)) * (N - 1));
    for (int k = 0; k < N; k++) {
        float a = 2.f * (float)M_PI * k / N - (float)M_PI / 2.f;
        Vec p = center.plus(Vec(std::cos(a), std::sin(a)).mult(radius));
        float on = (k == lit) ? 1.f : 0.f;
        float b = std::max(on, std::max(0.f, std::min(1.f, overlay)));
        nvgBeginPath(vg);
        nvgCircle(vg, p.x, p.y, 1.2f);
        nvgFillColor(vg, nvgRGBAf(b, b, b, 1.f));
        nvgFill(vg);
    }
}

// Draw a 32-LED value ring arranged on a SQUARE outline of small square LEDs —
// the layout baked into the E4 faceplate art (9 LED cells per side, corners
// shared = 32 unique cells). Cell roles follow encoquencer.md "LED
// visualization":
//   - the middle three cells of the BOTTOM row are the step/gate LED — they act
//     as one light with "the same function as the touch button's LED in the M4".
//     `stepLed` (0..1) is its brightness, `stepLedColor` the DROID colour value
//     (negative = white, the played-step marker; droidcolor.hpp).
//   - the two cells either side of those are unused (dark).
//   - the remaining 25 cells are the VALUE gauge: `value` (0..1) lights exactly
//     one of them (the DROID value dot). The gauge starts at the BOTTOM-LEFT
//     corner ("a blue LED gauge starting from the bottom left") and runs
//     clockwise — up the left side, across the top, down the right side — to the
//     bottom-right corner.
// `overlay` (0..1) is the white L-register brightness added to every cell.
//   center — widget-local centre of the ring
//   half   — distance from centre to a side's row of LED CENTRES (= 4 * pitch)
//   cell   — side length of each square LED, in px
// Panel-only helper; used by E4 and DB8E (both faceplates bake the same square
// ring: 9 cells per side, 62.6 px pitch, 44 px cells). The DB8E has no
// encoquencer step LED (its encoder cannot be used by it) — defaults keep those
// cells dark.
inline void drawValueRingSquare(NVGcontext* vg, Vec center, float half, float cell,
                                float value, float overlay,
                                float stepLed = 0.f, float stepLedColor = 0.f) {
    float pitch = half / 4.f;
    float ov = std::max(0.f, std::min(1.f, overlay));
    auto drawCell = [&](int c, int r, float cr, float cg, float cb) {
        Vec p = center.plus(Vec((c - 4) * pitch, (r - 4) * pitch));
        nvgBeginPath(vg);
        nvgRect(vg, p.x - cell * 0.5f, p.y - cell * 0.5f, cell, cell);
        nvgFillColor(vg, nvgRGBAf(std::max(cr, ov), std::max(cg, ov), std::max(cb, ov), 1.f));
        nvgFill(vg);
    };

    // Value gauge: 25 cells (col,row on the 9x9 perimeter grid), bottom-left
    // corner -> up the left column -> top row -> down the right column ->
    // bottom-right corner.
    const int NG = 25;
    int gcol[NG], grow[NG];
    int idx = 0;
    for (int r = 8; r >= 0; r--) { gcol[idx] = 0; grow[idx] = r; idx++; }  // left column, upward
    for (int c = 1; c <= 7; c++) { gcol[idx] = c; grow[idx] = 0; idx++; }  // top row, rightward
    for (int r = 0; r <= 8; r++) { gcol[idx] = 8; grow[idx] = r; idx++; }  // right column, downward
    // idx == 25 here; corners belong to the columns.
    int lit = (int)std::round(std::max(0.f, std::min(1.f, value)) * (NG - 1));
    for (int k = 0; k < NG; k++) {
        float b = (k == lit) ? 1.f : 0.f;
        drawCell(gcol[k], grow[k], b, b, b);
    }

    // Unused bottom cells (either side of the step LED): overlay only.
    for (int c : {1, 2, 6, 7}) drawCell(c, 8, 0.f, 0.f, 0.f);

    // Step/gate LED: the middle three bottom cells, one light.
    droid::color::RGB sc = droid::color::fromValue(stepLedColor);
    float sb = std::max(0.f, std::min(1.f, stepLed));
    for (int c : {3, 4, 5}) drawCell(c, 8, sc.r * sb, sc.g * sb, sc.b * sb);
}
