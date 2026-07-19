#pragma once
// Rack-agnostic controller-chain protocol shared by the Rack expander modules
// and headless tests. Controls flow toward the master (upstream) by PREPEND:
// each module inserts its own block at [0] and relays, so block order == chain
// order with no position handshake. LED data flows away from the master
// (downstream) by SHIFT: each module consumes block[0] and relays the rest.
// Structs are POD and fixed-size: they are memcpy'd through Rack's void*
// expander buffers.
#include "controllers.hpp"
#include "midi.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace droid { namespace chain {

constexpr int kMaxChainModules = 21;   // 16 controllers + 4 G8 + 1 slack
constexpr int kMaxPots = 10, kMaxSwitches = 10, kMaxLeds = 32, kGates = 8;
constexpr int kMaxEncodersPerModule = 4;   // e4 = 4, db8e = 1 (controllers.cpp table)
constexpr int kMaxFadersPerModule = 4;     // m4 = 4

// Wire values — a stable protocol contract, not table indices.
enum ModelId : uint8_t { None = 0, MP2B8 = 1, MP4B2 = 2, MP10 = 3, MS10 = 4,
                         MP8S8 = 5, MB32 = 6, ME4 = 7, MM4 = 8, MDB8E = 9,
                         MG8 = 10, MX7 = 11 };

const char* modelName(ModelId id);        // "p2b8"…"x7"; "" for None
bool isControllerModel(ModelId id);       // consumes a controller number (not None/MG8/MX7)
bool defaultLedFromButton(ModelId id);    // unwritten L follows the button (p2b8/p4b2/b32/e4/m4/db8e)
bool defaultLedFromPot(ModelId id);       // unwritten L follows the slider (p8s8)

// M5: a frame of MIDI carried alongside controls (upstream: adapter -> master)
// and LED state (downstream: master -> adapter). Rides EVERY block but is only
// meaningful on the X7 block (block[0] when an X7 is present). Fixed-size POD so
// the whole message still memcpy's through Rack's void* buffers.
//
// The two directions use DIFFERENT contracts because their pacing differs:
//
//   DOWNSTREAM (master -> X7, master-paced): the master writes a fresh
//   snapshot on its tick frames only and bumps `seq` when it writes events;
//   the X7 reads every frame and processes a frame ONCE per new seq. `total`
//   is unused (0).
//
//   UPSTREAM (X7 -> master, X7-paced but master-sampled): a per-frame
//   snapshot cannot work — events written on a non-sampled frame are
//   overwritten by later empty flips before the master's next sample. So the
//   X7 publishes a PERSISTENT sliding window of the most recent
//   kMidiEventsPerFrame events per port (ev[port][0..count-1], oldest first)
//   plus `total[port]`, a monotonic wrapping count of events ever captured —
//   the detentCount pattern. Each tick frame the master diffs `total` against
//   its last-seen value (wrap-safe, detentDelta) and consumes exactly the new
//   tail of the window, so no event is lost to the tick divider or relay
//   latency and no ack channel is needed. An event survives until displaced
//   by kMidiEventsPerFrame NEWER events; the reader detects displacement
//   (diff > count) as overflow. `seq` and `dropped` are unused upstream (0).
constexpr int kMidiEventsPerFrame = 16;   // per port per master sampling gap; far beyond real MIDI density

// The X7 carries exactly two MIDI ports over the chain. Frame port 0 = the
// X7's TRS jack, 1 = its USB jack; the master maps these onto the physical
// engine ports.
constexpr int kChainMidiPorts = 2;
inline MidiPort x7PhysicalPort(int framePort) {
    return framePort == 0 ? MidiPort::X7Trs : MidiPort::X7Usb;
}

struct MidiFrame {
    uint32_t seq = 0;                     // downstream dedupe (see contract above)
    uint32_t total[kChainMidiPorts] = {};   // upstream monotonic event count (see contract above)
    uint8_t count[kChainMidiPorts] = {};
    MidiEvent ev[kChainMidiPorts][kMidiEventsPerFrame] = {};
    uint16_t dropped[kChainMidiPorts] = {}; // downstream writer-side overflow diagnostic:
                                          // the block is re-zeroed every frame, so this
                                          // is a per-frame 0/1 "frame was full, remainder
                                          // still queued" flag — NOT cumulative.
};

// Upstream writer-side state backing the MidiFrame sliding-window contract:
// persists across audio frames in the adapter (X7), published into the wire
// block every frame. POD so it is headless-testable.
struct MidiUpstreamWindow {
    MidiEvent ev[kChainMidiPorts][kMidiEventsPerFrame] = {};
    uint8_t count[kChainMidiPorts] = {};
    uint32_t total[kChainMidiPorts] = {};

    void append(int port, const MidiEvent& e);      // shift-out-oldest when full
    void publish(MidiFrame& f) const;               // copy window + totals into the block
};

// Upstream reader-side consume: given the sampled frame and the reader's
// last-seen total for `port`, yields the index range of not-yet-seen events
// (f.ev[port][first..first+n-1], oldest first) and returns the number that
// overflowed the window between samples (0 when none). Caller updates its
// baseline to f.total[port] afterwards.
int32_t consumeUpstreamMidi(const MidiFrame& f, int port, uint32_t lastTotal,
                            int& first, int& n);

struct UpstreamBlock {                    // one module's controls, toward master
    uint8_t modelId = None;
    uint32_t buttons = 0;                 // bit e-1 = button e held
    float pots[kMaxPots] = {};            // 0..1
    float switches[kMaxSwitches] = {};    // S10 rotary 0..7, toggles 0/1/2
    float gates[kGates] = {};             // G8 jack input levels, 0/1
    // Wave 3: encoders + motor faders. detentCount is a MONOTONIC per-encoder
    // counter (wraps): the module increments it as the knob turns; the master
    // diffs against its last-seen value on tick frames (detentDelta), so no
    // event is lost to the tick divider or relay latency and no ack channel
    // is needed. faderPos is the fader's physical position 0..1; faderTouch
    // bit f-1 = the user is holding fader f (drag or plate — the "grabbed"
    // signal). The master calls moveFader only while touched — untouched
    // position echoes (the widget animating toward motorTarget) must never
    // register as user movement. plateTouch bit f-1 = the touch plate BELOW
    // fader f is pressed (TOUCH_PARAMS only, never a drag): it feeds the B
    // register and the engine's plate surface (motoquencer step buttons,
    // motorfader/faderbank/fadermatrix `button` outputs). Keeping the two
    // apart is what stops a fader drag from toggling sequencer gates.
    uint32_t detentCount[kMaxEncodersPerModule] = {};
    float faderPos[kMaxFadersPerModule] = {};
    uint8_t faderTouch = 0;
    uint8_t plateTouch = 0;
    MidiFrame midi;                       // M5: adapter -> master MIDI (X7 block only)
};
struct DownstreamBlock {                  // one module's LED/gate-out state, from master
    uint8_t modelId = None;
    float leds[kMaxLeds] = {};            // brightness 0..1
    float gates[kGates] = {};             // G8 output register values (module applies >= 0.1 -> 5 V)
    // R-register LED overrides (manual/basics.md §5.5, registers R17..R56): when
    // a patch drives the R register mapped to one of this module's matrix LEDs,
    // its value OVERRIDES the LED's normal function (the G8 gate mirror / the X7
    // indicator). rLedDriven bit j set -> rLeds[j] carries the DROID colour value
    // (0 = dark), rendered Rack-side via the droidcolor.hpp value->RGB table; the
    // bit clear -> the module keeps its default LED. The master fills these per
    // G8 block from R17.. (8 per G8) and per X7 block from R49...
    uint8_t rLedDriven = 0;
    float   rLeds[kGates] = {};
    // [droid] ledbrightness render hint (Engine::ledBrightness). The manual
    // scopes it to "the 24 LEDs of the master and the G8", so only the G8
    // scales its jack lights by it; controller modules ignore it.
    float ledBrightness = 1.f;
    // Wave 3: ring = the encoder's VALUE ring (EncoderState.ringDisplay, 0..1;
    // the L-register white OVERLAY rides leds[] as on every other model —
    // manual/circuits/encoder.md: L is additive white, not the value dot).
    // motorTarget/notches let the Rack fader animate circuit-commanded moves
    // and render dent positions. disp* is the DB8E's symbolic screen content
    // (header/text as NUL-terminated ASCII truncated to fit; value+numbermode
    // when dispIsText == 0). Pixels are rendered Rack-side.
    float ring[kMaxEncodersPerModule] = {};
    // Issue #15: the full select-gated ring image (EncoderState::RingDisplay),
    // mirroring hardware. ringFlags bits: 0 = active (a selected circuit drives
    // the ring; clear = ring dark), 1 = bipolar (zero at top-center instead of
    // bottom-center), 2 = ledfill (light zero..value), 3 = style 1 (legacy
    // encoquencer 25-cell gauge — render ringValue like the old ring[] dot).
    // ringValue is unipolar 0..1 / bipolar -1..1; the colours are DROID colour
    // values (droidcolor.hpp); ringOverlay is the select-gated white overlay
    // from the circuit's `led` param (the always-on L-register overlay rides
    // leds[] as before).
    uint8_t ringFlags[kMaxEncodersPerModule] = {};
    float ringValue[kMaxEncodersPerModule] = {};
    float ringColor[kMaxEncodersPerModule] = {};
    float ringNegColor[kMaxEncodersPerModule] = {};
    float ringOverlay[kMaxEncodersPerModule] = {};
    // encoquencer step LED (the middle-three ring cells below each encoder,
    // EncoderState.stepLed/-Color): brightness 0..1 + DROID colour value, where
    // a negative colour renders white (the played-step marker).
    float encStepLed[kMaxEncodersPerModule] = {};
    float encStepLedColor[kMaxEncodersPerModule] = {};
    float motorTarget[kMaxFadersPerModule] = {};
    uint8_t notches[kMaxFadersPerModule] = {};
    // Wave 3b: M4 fader LED below each fader. faderLed = brightness 0..1;
    // faderLedColor = DROID colour value 0..1 (0 = dark), rendered Rack-side via
    // the DROID value->RGB table. Distinct from the generic leds[] (which carry
    // L-register button/slider brightness on other models).
    float faderLed[kMaxFadersPerModule]      = {};
    float faderLedColor[kMaxFadersPerModule] = {};
    char dispHeader[24] = {};
    char dispText[24] = {};
    float dispValue = 0.f;
    uint8_t dispNumbermode = 0;
    uint8_t dispFontsize = 0;
    uint8_t dispIsText = 0;
    MidiFrame midi;                       // M5: master -> adapter MIDI (X7 block only)
};
struct UpstreamMessage  { uint8_t count = 0; UpstreamBlock  block[kMaxChainModules]; };
struct DownstreamMessage{ uint8_t count = 0; DownstreamBlock block[kMaxChainModules]; };

void prependUpstream(const UpstreamBlock& mine, const UpstreamMessage& fromRight,
                     UpstreamMessage& out);
void shiftDownstream(const DownstreamMessage& fromLeft, DownstreamBlock& mine,
                     DownstreamMessage& out);

// Wrap-safe signed detent difference (two's-complement subtraction).
int32_t detentDelta(uint32_t now, uint32_t last);

// Master-side helpers.
std::vector<std::string> controllerModels(const UpstreamMessage& m);   // G8s skipped
// Prefix validation (approved semantics: declared must match chain position-
// wise; surplus physical modules are idle). "" when valid, else a Forge-style
// message: "controller N: patch declares X, chain has Y|nothing".
std::string validateChain(const std::vector<std::string>& declared,
                          const std::vector<std::string>& physical);

// X7 placement check: an X7 must be first in the chain (block[0], nearest the
// master) and unique. "" when valid (including no X7 at all), else a message:
// "x7 must be first in the chain" or "only one x7 can be attached".
std::string x7ChainError(const UpstreamMessage& m);

// Debounce for the master's chainOk gate (ISSUE-5). On a chain hot-plug Rack
// re-enumerates the expander chain over several UI frames; during that window
// the physical chain is transiently shorter than the patch declares, so
// validateChain briefly fails. Demoting chainOk to false on that blip pauses
// the engine (`if (!chainOk) return;`) for ~1 frame — an audible/CV hitch even
// though the chain immediately self-heals. This delays demotion from a
// PREVIOUSLY-VALID state until the chain has been continuously invalid for
// kMaxInvalidFrames revalidations, so a transient shrink never pauses the
// engine, while a genuinely wrong chain still errors within that bound.
//
// The caller keeps revalidating every UI frame while pending() is set (not just
// on chain-change events), so the counter advances in real time and a static
// wrong chain reaches the demotion bound promptly rather than stalling at 1.
struct ChainOkDebounce {
    // Frames of continuous invalidity tolerated before demoting a valid chainOk.
    // Constraint: must exceed Rack's expander re-enumeration transient on a
    // single hot-plug (a couple of UI frames) yet stay far under a second of UI
    // frames (~60/s) so a real mismatch errors promptly. 6 frames ≈ 100 ms.
    static constexpr int kMaxInvalidFrames = 6;
    int invalidFrames = 0;

    struct Result {
        bool ok;        // debounced chainOk
        bool pending;   // caller must revalidate again next frame to advance the debounce
    };

    // Call once per revalidation. prevOk = chainOk before this check; valid =
    // validateChain (+ x7 error) currently passes; force = a fresh patch
    // (re)load, which demotes at once (no debounce) so a wrong chain at load
    // errors immediately rather than after the transient window.
    Result update(bool prevOk, bool valid, bool force) {
        if (valid)            { invalidFrames = 0; return {true, false}; }
        if (force || !prevOk) { invalidFrames = 0; return {false, false}; }
        if (++invalidFrames >= kMaxInvalidFrames) { invalidFrames = 0; return {false, false}; }
        return {true, true};   // still within the tolerance window: hold Ok, keep polling
    }
};

}} // namespace droid::chain
