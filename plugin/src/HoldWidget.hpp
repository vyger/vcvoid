#pragma once
// Rack side of the button-hold gestures (issue #39): the hold modifier, the
// one arbiter, the once-per-frame modifier poll, the cross-cast interface a
// module menu uses to find held controls, and the ring they all draw.
//
// Split out of DroidWidgets.hpp because the gesture is not only for buttons:
// the E4/DB8E encoder PUSH is a B register too, and its widget lives in
// EncoderWidgets.hpp, which is included before DroidWidgets.hpp. The decision
// logic itself stays Rack-free in ButtonHold.hpp so it can be unit-tested
// headless.
#include "plugin.hpp"
#include "ButtonHold.hpp"

namespace dw {

// The modifier that arms the hold gesture.
//
// NOT Alt, however natural "Alt+click to hold" sounds — Rack owns Alt+click
// and takes it before any module ever sees it. From Rack v2
// src/ui/ScrollWidget.cpp, ScrollWidget::onButton:
//
//     // Handle Alt-click before children, since most widgets consume
//     // Alt-click without needing to.
//     if (e.button == GLFW_MOUSE_BUTTON_LEFT
//         && (e.mods & RACK_MOD_MASK) == GLFW_MOD_ALT) {
//         e.consume(this);
//         return;
//     }
//
// The rack lives inside a RackScrollWidget (: ui::ScrollWidget), which is an
// ancestor of every ModuleWidget, and that check runs BEFORE the event is
// dispatched to children. So an Alt+left-click anywhere in the rack is claimed
// for click-drag scrolling and never reaches a widget at all: the button did
// not even flash, because Switch::onDragStart was never called. Nothing a
// plugin does inside its own widget can undo an ancestor's consume.
//
// Shift is clear along the whole path, which was checked against the same
// sources rather than assumed:
//   - ScrollWidget::onButton     — grabs only exactly-Alt and middle-click.
//   - RackWidget::onButton       — children first; claims nothing on left.
//   - ModuleWidget::onButton     — Shift-click selects the module, but ONLY
//                                  `if (e.isConsumed()) return;` fails, and
//                                  OpaqueWidget::onButton consumes a left
//                                  press for us first.
//   - ParamWidget::onButton      — its touch-param and context-menu branches
//                                  both require `mods == 0`, so they simply
//                                  sit out a modified click.
//   - Switch::onDragStart        — reads mods only on the NON-momentary path;
//                                  our buttons are momentary.
// Shift also happens to be the word DROID patches already use for these
// gestures ("shift combos"), so the docs read the way the hardware does.
//
// One trade-off, inherited from any modifier: `ParamWidget::onButton` only
// registers a touched param when `mods == 0`, so a modified click cannot be
// the one that arms Rack's MIDI-Map. Map with a plain click first.
static constexpr int kHoldMod = GLFW_MOD_SHIFT;
static constexpr const char* kHoldModName = "Shift";

// Not a style preference — a hard constraint. Alt+left-click is consumed by
// Rack's own ScrollWidget before the event is dispatched to children, so a hold
// armed by Alt can never fire: the click vanishes with no press, no LED and no
// ring. Fail the build rather than ship that again.
static_assert(kHoldMod != GLFW_MOD_ALT,
              "Rack's ScrollWidget claims Alt+left-click before children see it");

// The one arbiter for the whole rack — the hold is a rack-wide gesture (you
// hold CTRL on the p2b8 and chord it with a button on the b32, or with an
// encoder push on the e4), so it cannot live per widget or per module.
inline vcvoid::HoldArbiter& holdArbiter() {
    static vcvoid::HoldArbiter a;
    return a;
}

// Poll the PHYSICAL modifier key once per UI frame. Window::getMods() is a raw
// glfwGetKey of the left+right modifier keys against the main window, so it
// keeps reporting while a context menu is up or focus has moved — unlike a
// key-up EVENT, which is delivered to whatever widget is focused and would
// never reach the control, leaving it stuck down. Every holdable control calls
// this; the frame stamp makes all but the first call in a frame a no-op.
// (getFrameTime() is NAN before the first frame, and NAN != NAN, so we simply
// poll — which is what we want anyway.)
inline void pollHoldModOnce() {
    static double lastFrame = -1.0;
    double t = APP->window->getFrameTime();
    if (t == lastFrame) return;
    lastFrame = t;
    // A bit test, not an equality: once the hold is out, the user may press
    // other keys without it dropping.
    holdArbiter().pollMod((APP->window->getMods() & kHoldMod) != 0);
}

// True when a left-button press carries the hold modifier and nothing else, so
// every other combination stays free for whatever else wants it.
inline bool isHoldModPress(const rack::widget::Widget::ButtonEvent& e) {
    return e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT
        && (e.mods & RACK_MOD_MASK) == kHoldMod;
}

// Cross-cast target so a module's context menu can find every holdable control
// on it without knowing the widget types — which differ in kind, not just in
// artwork: the buttons are ParamWidgets, the encoder pushes are not.
struct HoldableControl {
    virtual ~HoldableControl() = default;
    virtual void releaseHold() = 0;
};

// The hold indicator: a ring in the panel's orange accent (the P8S8 fader cap
// colour) — deliberately not the warm cream an LED glows, so "the patch lit
// this" and "you are holding this" never look alike. One opacity for both
// latched and mod-held: which of the two it is matters far less than THAT it is
// down, and two shades of orange read as a rendering glitch rather than a
// distinction.
inline void drawHoldRing(NVGcontext* vg, rack::math::Vec c, float r) {
    nvgBeginPath(vg);
    nvgCircle(vg, c.x, c.y, r);
    nvgStrokeWidth(vg, 2.0f);
    nvgStrokeColor(vg, nvgRGB(0xf7, 0x94, 0x1d));
    nvgStroke(vg);
}

} // namespace dw
