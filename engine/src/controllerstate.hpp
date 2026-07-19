#pragma once
#include "registers.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace droid {

// Per-encoder runtime state (E4 / DB8E). Encoders are NOT value-bearing
// registers (manual encoder.md: "you cannot directly use the registers of the
// encoders"), so their live state lives here rather than in the RegisterFile.
//   * pendingDetents: signed detent movement accumulated since the last drain.
//     turnEncoder() adds to it; the owning encoder circuit READS it each tick;
//     the Engine clears it at end of tick (so several `encoder` circuits sharing
//     one physical knob all see the same movement before it drains).
//   * pushed: the built-in push button level (E4/DB8E), fed by pushEncoder().
//   * ringDisplay: last displayed normalized position (0..1); a panel-only
//     readback for the Rack LED-ring adapter (not observable in goldens).
//   * stepLed / stepLedColor: the encoquencer step LED — the middle-three cells
//     of the ring's bottom row, which act as one light with "the same function
//     as the touch button's LED in the M4" (encoquencer.md). Brightness 0..1 +
//     DROID colour value; a negative colour is the white played-step sentinel
//     (see SeqCore::kLedWhite). Panel-only, like ringDisplay.
// What the currently SELECTED encoder/encoderbank/encoquencer circuit shows on
// the 32-LED ring (panel-only; issue #15, verified against hardware photos of
// the four-menu fixture). Written each tick by the selected circuit only;
// beginTick() clears it, so with nothing selected the ring goes dark (the
// L-register white overlay still rides the generic leds[] path).
//   style 0 (encoder/encoderbank): the full 32-cell value ring. Unipolar zero
//     at BOTTOM-center sweeping clockwise (left side up, top, right side down);
//     bipolar zero at TOP-center — positive clockwise down the right side in
//     `color`, negative counterclockwise down the left in `negColor`, the zero
//     dot bright white. `fill` = ledfill (LEDs zero..value at half brightness);
//     unlit cells glow the colour dimly (real hardware background glow).
//   style 1 (encoquencer): the legacy 25-cell gauge from the bottom-left corner
//     (encoquencer.md "LED visualization"; the bottom-middle cells stay the
//     step LED). value is the gauge position, colour fields unused.
//   overlay: white whole-ring overlay from the circuit's `led` param (0..1),
//     select-gated — distinct from the always-on L-register overlay.
struct RingDisplay {
    bool    active = false;
    uint8_t style = 0;
    bool    bipolar = false;
    bool    fill = true;
    float   value = 0.0f;      // unipolar 0..1, bipolar -1..1
    float   color = 0.0f;      // DROID colour value (droidcolor.hpp)
    float   negColor = 0.0f;
    float   overlay = 0.0f;
};

struct EncoderState {
    long  pendingDetents = 0;
    bool  pushed = false;
    float ringDisplay = 0.0f;
    RingDisplay ring;          // select-gated display (issue #15); see above
    float stepLed = 0.0f;
    float stepLedColor = 0.0f;
};

// Per-motor-fader runtime state (M4). Motor faders are NOT value-bearing
// registers either — they are addressed by GLOBAL fader number (`fader = N`,
// motorfader.md: "All faders are simply enumerated"), 4 per M4 in chain order.
// The state is bidirectional:
//   * position: the fader's current physical position (0..1). moveFader() (user
//     movement) writes it; a circuit READS it to track user takeover; a circuit
//     also WRITES it via commandFader() to move the motor (preset recall, notch
//     snap, motorized takeover). `expect F<n>` reads this back.
//   * motorTarget: the position the circuit last commanded. In the headless
//     model the motor is instantaneous (SPEC-GAP: no speed spec in the manual,
//     so we snap — position == motorTarget the moment a circuit commands). It is
//     kept distinct so a future Rack adapter can animate position -> motorTarget.
//   * touched: the user is holding the fader itself (touchFader()); a level.
//     Gates edit acceptance and the pitch-bend hold (fadercore auto-return).
//   * plate: the touch plate BELOW the fader (pressFaderPlate()); a level, read
//     by the circuit's `button` output and motoquencer's step buttons, and
//     mirrored to the M4's B register. A separate sensor from the fader hold —
//     grabbing/moving a fader must never register as a plate press.
//   * notches: the haptic config the owning circuit last applied (0 none,
//     1 pitch-bend, 2 binary, N dents). Panel-only feel; recorded for the Rack
//     adapter and to document the observable output quantization.
//   * led / ledColor: LED brightness / colour below the fader (panel-only, like
//     pot's LED gauge and the encoder ring — not golden-observable directly,
//     but parked/darkened state is inferred from position for unusedfaders).
struct FaderState {
    float position = 0.0f;
    float motorTarget = 0.0f;
    bool  touched = false;
    bool  plate = false;
    int   notches = 0;
    float led = 0.0f;
    float ledColor = 0.0f;
};

// Per-DB8E symbolic screen content. NOT pixels: header + (text | value+format).
// Content arbitration implements the [display]-circuit tier only (precedence
// Library > [display] > circuit > control; the other tiers are Rack/M6 —
// see plan Global Constraints). Owner + linger give the manual's semantics:
// a write is accepted iff it comes from the current owner OR the linger
// window has expired. `useasdefault` content re-asserts after expiry.
struct DisplayState {
    bool active = false;          // false until first accepted write
    int headerText = 0;           // text number, 0 = none
    bool isText = false;
    int bodyText = 0;             // when isText
    float value = 0.0f;           // when !isText
    uint8_t numbermode = 0;
    uint8_t fontsize = 0;
    const void* owner = nullptr;  // opaque circuit identity for linger arbitration
    uint64_t lingerUntilTick = 0;
    // Tick of the last accepted write. Lets the [display] circuit implement the
    // documented same-tick tie-break (two circuits both changing on one tick:
    // patch order wins, later accepted write lands last) WITHOUT the linger a
    // fresh write just installed blocking a later same-tick writer. Cross-tick
    // arbitration still uses lingerUntilTick; this only unblocks writers on the
    // exact tick the current content was last written. 0 + active==false is the
    // pre-first-write sentinel (never a false same-tick match at tick 0).
    uint64_t lastWriteTick = 0;
};

// Engine-owned model of the controller chain's stateful (non-register) elements.
// Currently the encoder tier (M4b); motor faders join in M4c. Built at load from
// the ordered list of controller model names (x7 already excluded by the loader,
// matching hardware numbering). The mutation API (turn/push) is the ONE seam the
// golden harness and the future Rack adapter both drive — it is free of any
// harness types on purpose.
class ControllerState {
public:
    // (Re)build the encoder index map from the ordered controller model names,
    // e.g. {"p2b8","e4","db8e"}. Global encoder indices are assigned 1-based in
    // chain order: every E-capable model contributes its encoders in turn (e4 -> 4,
    // db8e -> 1). Resets all encoder state.
    void configure(const std::vector<std::string>& controllerModels);

    int encoderCount() const { return (int)encoders_.size(); }
    int faderCount() const { return (int)faders_.size(); }
    int displayCount() const { return (int)displays_.size(); }

    // E<ctrl>.<num> -> 1-based global encoder index, or 0 if no such encoder.
    int globalIndexForReg(const RegId& r) const;
    // Returns global1 if 1..encoderCount(), else 0.
    int validateGlobal(int global1) const;
    // 1-based global index -> its E<ctrl>.<num> register (type 0 RegId if bad).
    RegId regForGlobal(int global1) const;

    // --- mutation API (golden harness + future Rack adapter) -----------------
    // All index-form calls take a 1-based global encoder index; out-of-range is
    // ignored. The RegId forms accept E<ctrl>.<num> (chain lookup) or E<n> with
    // ctrl==0 (direct global index); they return false if unresolved.
    void turnEncoder(int global1, long detents);
    void pushEncoder(int global1, bool down);
    bool turnEncoder(const RegId& r, long detents);
    bool pushEncoder(const RegId& r, bool down);

    // Fader mutation API (user side). `fader1` is a 1-based GLOBAL fader number
    // (no register form — faders are addressed only by global number). Out of
    // range is ignored (returns false for the bool forms). moveFader clamps to
    // 0..1 and marks the fader as moved by the user this tick.
    void moveFader(int fader1, float pos);
    void touchFader(int fader1, bool down);
    void pressFaderPlate(int fader1, bool down);
    bool validateFader(int fader1) const;

    // --- circuit-facing reads / motor commands -------------------------------
    EncoderState* encoder(int global1);
    const EncoderState* encoder(int global1) const;
    FaderState* fader(int fader1);
    const FaderState* fader(int fader1) const;
    float faderPosition(int fader1) const;   // 0..1; 0 if out of range
    // Motor command: move the fader to `pos` (instant in the headless model).
    void commandFader(int fader1, float pos);

    // Per-DB8E display state. `db8e1` is 1-based, counting DB8Es only (chain
    // order; the manual's `display=N` addressing). Bounds-checked like fader().
    DisplayState* display(int db8e1);
    const DisplayState* display(int db8e1) const;

    // Called by the Engine before every tick: clears the select-gated ring
    // display so a deselected circuit's image can't linger (issue #15).
    void beginTick();
    // Called by the Engine after every tick: drains accumulated detents.
    void endTick();

private:
    // Resolve any E RegId (register or bare-global form) to a global index, or 0.
    int resolve(const RegId& r) const;
    struct Slot { uint8_t ctrl; uint8_t num; };
    std::vector<Slot> slots_;             // [global-1] -> (ctrl,num)
    std::vector<EncoderState> encoders_;  // parallel to slots_
    std::vector<FaderState> faders_;      // [global fader-1]
    std::vector<DisplayState> displays_;  // [global DB8E-1]
};

} // namespace droid
