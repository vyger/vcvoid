#pragma once
// Rack side of the button-hold gestures (issue #39): the one arbiter, the
// once-per-frame Alt poll, the cross-cast interface a module menu uses to find
// held controls, and the ring they all draw.
//
// Split out of DroidWidgets.hpp because the gesture is not only for buttons:
// the E4/DB8E encoder PUSH is a B register too, and its widget lives in
// EncoderWidgets.hpp, which is included before DroidWidgets.hpp. The decision
// logic itself stays Rack-free in ButtonHold.hpp so it can be unit-tested
// headless.
#include "plugin.hpp"
#include "ButtonHold.hpp"

namespace dw {

// The one arbiter for the whole rack — the alt-hold is a rack-wide gesture
// (you alt-hold CTRL on the p2b8 and chord it with a button on the b32, or
// with an encoder push on the e4), so it cannot live per widget or per module.
inline vcvoid::AltHoldArbiter& altHoldArbiter() {
    static vcvoid::AltHoldArbiter a;
    return a;
}

// Poll the PHYSICAL Alt key once per UI frame. Window::getMods() is a raw
// glfwGetKey of left+right Alt against the main window, so it keeps reporting
// while a context menu is up or focus has moved — unlike a key-up EVENT, which
// is delivered to whatever widget is focused and would never reach the control,
// leaving it stuck down. Every holdable control calls this; the frame stamp
// makes all but the first call in a frame a no-op. (getFrameTime() is NAN
// before the first frame, and NAN != NAN, so we simply poll — which is what we
// want anyway.)
inline void pollAltOnce() {
    static double lastFrame = -1.0;
    double t = APP->window->getFrameTime();
    if (t == lastFrame) return;
    lastFrame = t;
    altHoldArbiter().pollAlt((APP->window->getMods() & GLFW_MOD_ALT) != 0);
}

// True when a left-button press carries Alt and nothing else, so Ctrl+Alt and
// friends stay free for whatever else wants them.
inline bool isAltPress(const rack::widget::Widget::ButtonEvent& e) {
    return e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT
        && (e.mods & RACK_MOD_MASK) == GLFW_MOD_ALT;
}

// Cross-cast target so a module's context menu can find every holdable control
// on it without knowing the widget types — which now differ in kind, not just
// in artwork: the buttons are ParamWidgets, the encoder pushes are not.
struct HoldableControl {
    virtual ~HoldableControl() = default;
    virtual void releaseHold() = 0;
};

// The hold indicator: a ring in the panel's orange accent (the P8S8 fader cap
// colour) — deliberately not the warm cream an LED glows, so "the patch lit
// this" and "you are holding this" never look alike. One opacity for both
// latched and alt-held: which of the two it is matters far less than THAT it is
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
