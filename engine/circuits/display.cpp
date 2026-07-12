// display — show texts or values on a DB8E's OLED, or fix its number mode / font.
// Spec: manual/circuits/display.md. This is the ENGINE-SIDE symbolic screen model
// only: it writes DisplayState (header + text|value + numbermode/fontsize) for one
// DB8E via ControllerState. What those fields actually RENDER (font sizing, gauges,
// sparklines, note names, screensaver animation) is the Wave-3b Rack half and is
// verified-by-human; goldens here assert WHAT should be on screen, never HOW it
// looks. `expectdisplay` (Task 5) observes the DisplayState headlessly.
//
// Content model (display.md "Showing texts" / "Showing values", Inputs table):
//   * `text` patched            -> text mode: body = floored text NUMBER; a text
//                                  number of 0 is the empty text (no content).
//   * `value` patched, no text  -> value mode: body = the value as-is (numbermode
//                                  is stored, not applied — formatting is render).
//   * `text` AND `value`        -> text wins ("value does not work if you used text").
//   * `header`                  -> floored text number (0 = none). A header alone
//                                  never activates the display (manual: "not possible
//                                  to just display a header"); it only rides along
//                                  with a body. Header changes do NOT by themselves
//                                  drive an update — the manual's own BPM example
//                                  keeps the screen OFF while a constant header sits
//                                  over an unchanged value, so the activation driver
//                                  is the body (value/text), matching display.md's
//                                  "become active whenever the input value changes".
//
// Update / arbitration (display.md linger/threshold/trigger + Task-6 resolutions):
//   * A write ATTEMPT happens when the body changes (value beyond `threshold`, or a
//     different text number), OR on a `trigger` rising edge (popup: while `trigger`
//     is patched, change detection is OFF and only edges push content — even an
//     unchanged one), OR a `useasdefault` re-assert, OR the catch-up on becoming
//     selected. `threshold` compares against the last DISPLAYED value (a noise floor
//     around what is on screen), not the previous tick, so sub-threshold drift never
//     accumulates into an update. Default threshold 0.0005 (the jack has no codegen
//     default), default linger 0.01 s.
//   * An attempt is ACCEPTED iff it comes from the current owner, OR the linger
//     window has expired (tick >= lingerUntilTick), OR it is a same-tick overwrite
//     (another circuit already wrote THIS tick: patch order wins, later lands last —
//     SPEC-GAP, see DisplayState.lastWriteTick). On accept the content updates,
//     owner := this, lingerUntilTick := tick + linger*tickRate.
//   * `useasdefault = 1`: while selected, once tick >= lingerUntilTick and someone
//     else owns, re-assert this circuit's current content each tick (idempotent).
//     Two defaults -> last in patch order ends up owning (same SPEC-GAP tie-break).
//
// SPEC-GAPs (engine model, no headless observable — accept the params for RAM
// accounting but implement no behavior; Wave-3b Rack surface):
//   * `screensaver` — OLED burn-in animation timing.
//   * `takeovercontrols` — hands the 8 buttons + encoder (+ ring LEDs) to the patch.
// Both are documented in display.md but are panel/visual only.
//
// Targeting SPEC-GAP: `display = N` counts DB8Es only; 0 => suppressed (never
// writes); an N with no such DB8E => inert (not a load error, mirroring G8's
// absent-hardware tolerance). Default 1.
#include "../src/registry.hpp"
#include "../src/uihelpers.hpp"
#include <cmath>

namespace droid {

class Display : public Circuit {
public:
    void tick(EngineState& s) override {
        // --- resolve the target DB8E screen -------------------------------------
        // floorClamp range-checks the float before casting (input math can produce a
        // finite-but-out-of-range target like `display = I1 * 1e20`, whose cast to int
        // would be UB). 0 => suppressed; an out-of-range/huge/non-finite value clamps
        // to kTargetMax, which display() rejects as "no such DB8E" => inert.
        int n = floorClamp(in("display").value(s), 0, kTargetMax);
        if (n == 0) return;                              // suppressed: never writes
        DisplayState* d = s.controllers.display(n);
        if (!d) return;                                  // no such DB8E: inert

        // --- select / selectat overlay gating -----------------------------------
        bool selectUsed = in("select").connected() || in("selectat").connected();
        bool selected = ui::isSelected(*this, s);
        // Catch-up only applies to a real select transition (deselected -> selected),
        // never the first-ever tick of a circuit that does not use select at all.
        bool justSelected = selectUsed && selected && !prevSelected_;
        prevSelected_ = selected;
        if (selectUsed && !selected) {
            // Inactive: no attempts, and DON'T advance baselines/trigger edge, so
            // that on re-selection the current value differs and catches up.
            trigPrev_ = in("trigger").value(s) >= kGateHighThreshold;
            return;
        }

        // --- candidate content ---------------------------------------------------
        bool textMode = in("text").connected();
        int header = floorText(in("header").value(s));
        bool haveContent;
        int body = 0;
        float val = 0.0f;
        if (textMode) {
            body = floorText(in("text").value(s));
            haveContent = (body != 0);                   // empty text => no content
        } else {
            haveContent = in("value").connected();
            val = in("value").value(s);
        }

        float threshold = in("threshold").connected() ? in("threshold").value(s)
                                                       : 0.0005f;
        threshold = std::fabs(threshold);
        float linger = in("linger").value(s);            // default 0.01 s
        bool trigPatched = in("trigger").connected();
        bool trigEdge = risingEdge(trigPrev_, in("trigger").value(s));
        bool useDefault = in("useasdefault").value(s) >= kGateHighThreshold;

        // --- decide whether to attempt a write -----------------------------------
        bool attempt = false;
        if (haveContent) {
            if (trigPatched) {
                attempt = trigEdge;                      // popup: edges only
            } else if (justSelected) {
                attempt = true;                          // catch-up on select
            } else if (textMode) {
                // Edge detection against the last DISPLAYED text number. prevText_
                // advances only on an accepted write (or resets to 0 when the text
                // goes empty), mirroring sentValue_ — so a text change blocked by
                // another circuit's linger stays pending and re-attempts until it
                // lands (symmetric with value). A text returning from empty to its
                // slot number is still seen as a change (Started/Stopped example).
                attempt = (body != prevText_);
            } else {
                attempt = (std::fabs(val - sentValue_) > threshold);
            }
            if (useDefault && s.tick >= d->lingerUntilTick && d->owner != this)
                attempt = true;                          // default re-assert
        }

        // --- arbitrate + write ---------------------------------------------------
        if (attempt) {
            bool accepted = (d->owner == this) ||
                            (s.tick >= d->lingerUntilTick) ||
                            (d->active && d->lastWriteTick == s.tick);  // same-tick
            if (accepted) {
                d->active = true;
                d->headerText = header;
                d->isText = textMode;
                if (textMode) d->bodyText = body;
                else          d->value = val;
                // floorClamp guards the float before the cast (finite-out-of-range
                // input math would make the raw (int)floor UB), then lands in range.
                d->numbermode = (uint8_t)floorClamp(in("numbermode").value(s), 0, 18);
                d->fontsize   = (uint8_t)floorClamp(in("fontsize").value(s), 0, 3);
                d->owner = this;
                long lt = std::lround((double)linger * s.tickRateHz);
                if (lt < 0) lt = 0;
                d->lingerUntilTick = s.tick + (uint64_t)lt;
                d->lastWriteTick = s.tick;
                sentValue_ = val;   // value baseline advances only on a DISPLAYED value
                prevText_  = body;  // text baseline likewise advances only on a DISPLAYED text
            }
            // Rejected writes leave BOTH baselines (sentValue_ / prevText_) untouched so
            // the circuit keeps re-attempting every tick until the owner's linger
            // expires — this is how it yields the screen. Text now retries symmetrically
            // with value (delay-not-discard) rather than dropping a blocked change.
        }
        // A text that multiplies out to empty (0) is not a pending write to retry — the
        // circuit is voluntarily showing nothing — so reset the baseline to 0. Without
        // this, a text returning from empty to its old slot number would not be seen as
        // a change (the Started/Stopped example depends on this; see text-toggle.gold).
        if (textMode && !haveContent) prevText_ = 0;
    }

private:
    // Largest DB8E target we bother resolving; anything beyond is inert. Well within
    // int and float-exact, so the cast in floorClamp never overflows.
    static constexpr int kTargetMax = 256;
    // Text numbers are 1-based; anything at/above this is "no such text" (textForNumber
    // returns ""). Float-exact and int-safe so a huge input math product cannot make
    // the cast UB.
    static constexpr int kTextMax = 1 << 20;

    // Range-check-before-cast: a finite-but-out-of-range float cast to a narrow int is
    // UB, and input math (`= I1 * 1e20`) can produce one. Compare AS FLOAT, clamp into
    // [lo, hi], and only ever cast an already-in-range value. Non-finite maps to lo.
    static int floorClamp(float v, int lo, int hi) {
        if (!std::isfinite(v) || v <= float(lo)) return lo;
        if (v >= float(hi)) return hi;
        return (int)std::floor(v);
    }
    // Text numbers are 1-based positive integers (0 = empty). <=0 (and non-finite) map
    // to 0 (empty); an out-of-range large number clamps to kTextMax which resolves to
    // "" (no such text), matching textForNumber's contract without a UB cast.
    static int floorText(float v) {
        return floorClamp(v, 0, kTextMax);
    }

    bool  prevSelected_ = false;
    bool  trigPrev_ = false;
    int   prevText_ = 0;
    float sentValue_ = 0.0f;
};

DROID_REGISTER_CIRCUIT(display, Display)

} // namespace droid
