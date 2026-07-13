#pragma once
#include "registers.hpp"
#include "controllerstate.hpp"
#include "midi.hpp"
#include <cstdint>
#include <functional>
#include <vector>

namespace droid {

// MASTER18 frequency probe on I1 (manual hardware.md §9.7): the Rack adapter
// measures positive zero-crossings at full sample rate (the engine tick rate
// is far too low for audio) and publishes the result here; vcotuner reads it.
struct ProbeState {
    float hz = 0.0f;
    bool present = false;
};

// SD-card file source (midifileplayer): maps a file number to the bytes of
// midi<N>.mid. The adapter (Rack: the folder holding the loaded droid.ini;
// goldens: the `midifile` directive) installs it; null = no SD card present.
// Returns false when the file does not exist.
using FileProvider = std::function<bool(int fileNum, std::vector<uint8_t>& out)>;

// Global master settings written by the [droid] circuit each tick and consumed
// by the Engine (engine/circuits/droid.cpp). Output conditioning applies ONLY
// to outputs whose maxslope/lpfilter jack is patched — the hardware's
// always-on default smoothing (0.25/0.25) is deliberately NOT emulated: the
// engine's outputs are artifact-free floats and silently filtering every
// output would change semantics everywhere (SPEC-GAP deviation, ledger-noted).
struct MasterSettings {
    float lpf[8] = {};          // per-output LPF strength 0..1
    float maxslope[8] = {};     // jump threshold, engine units; 0 = no smoothing
    bool conditioned[8] = {};   // lpfilter_k or maxslope_k patched in a [droid]
    float ledBrightness = 1.0f; // render hint for the Rack adapter; L registers unscaled
    bool requestClear = false;    // broadcast clear to unpatched clear inputs
    bool requestClearAll = false; // ... likewise clearall
};

struct EngineState {
    RegisterFile regs;
    MasterSettings master;        // [droid] global settings seam
    ControllerState controllers;  // encoder (M4b) / fader (M4c) live state
    MidiState midi;              // M5 MIDI seam: queues + per-tick snapshot
    ProbeState probe;            // MASTER18 I1 frequency probe (adapter-fed)
    FileProvider fileProvider;   // SD-card file source (adapter-installed)
    std::vector<float> cables;   // indexed by cable id
    uint64_t tick = 0;
    uint32_t rngState = 1;       // xorshift32; circuits draw from this
    float tickRateHz = 6000.0f;  // engine tick rate; set by Engine on load.
                                 // Time-based circuits (lfo, contour, ...)
                                 // derive per-tick phase increments from this.
    float secondsPerTick() const { return 1.0f / tickRateHz; }
};

struct Operand {
    enum class Kind : uint8_t { Const, Register, Cable };
    Kind kind = Kind::Const;
    float constant = 0.0f;
    RegId reg;
    uint16_t cableIndex = 0;
    float value(const EngineState& s) const {
        switch (kind) {
            case Kind::Const:    return constant;
            case Kind::Register: return s.regs.get(reg);
            case Kind::Cable:    return s.cables[cableIndex];
        }
        return 0.0f;
    }
};

class Input {
public:
    bool connected() const { return present_; }
    float value(const EngineState& s) const {
        if (!present_) return defaultValue_;
        return a_.value(s) * b_.value(s) + c_.value(s);
    }
    // loader-facing:
    void bind(Operand a, Operand b, Operand c) { a_ = a; b_ = b; c_ = c; present_ = true; }
    void setDefault(float v) { defaultValue_ = v; }
    // Primary (value-A) operand. Circuits that need the literal binding rather
    // than a runtime value — e.g. `encoder` resolving an E<ctrl>.<num> handle
    // once at load — inspect this. Register handles carry no live value, so
    // value() would return 0 for them.
    const Operand& primary() const { return a_; }
private:
    Operand a_, b_, c_;
    bool present_ = false;
    float defaultValue_ = 0.0f;
};

class Output {
public:
    bool connected() const { return present_; }
    void set(EngineState& s, float v) const {
        if (!present_) return;
        if (target_.kind == Operand::Kind::Register) s.regs.set(target_.reg, v);
        else if (target_.kind == Operand::Kind::Cable) s.cables[target_.cableIndex] = v;
    }
    void bind(Operand target) { target_ = target; present_ = true; }
private:
    Operand target_;
    bool present_ = false;
};

} // namespace droid
