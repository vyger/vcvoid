#pragma once
#include "loader.hpp"
#include "registry.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace droid {

// --- persistent-state snapshot (DROIDSTA.BIN contract, hardware.md §11.1) ---
// One stateful circuit's persisted state. Circuits are matched across a patch
// reload by TYPE + per-type appearance ORDINAL (the Nth <type> saved state
// loads into the Nth <type> circuit of the new patch), exactly as the hardware
// numbers circuits. `values` is a flat list of doubles tagged with `version`
// (JSON-friendly / diffable), opaque to everything but the circuit itself.
struct CircuitState {
    std::string type;             // circuit def->name
    int ordinal = 0;              // 1-based, per-type patch-appearance order
    int version = 0;              // circuit stateVersion() of the blob
    std::vector<double> values;
};
struct StateSnapshot {
    std::vector<CircuitState> entries;   // patch-appearance order
    bool empty() const { return entries.empty(); }
};

class Engine {
public:
    explicit Engine(MasterType master = MasterType::Master16,
                    float tickRateHz = 6000.0f, uint32_t seed = 1);
    LoadResult load(const std::string& patchText);
    bool loaded() const { return loaded_; }
    void tick();
    uint64_t tickCount() const { return state_.tick; }
    float tickRateHz() const { return tickRateHz_; }
    unsigned ramUsed() const { return ramUsed_; }
    uint32_t rngState() const { return state_.rngState; }   // for determinism tests
    // [droid] ledbrightness: render hint for the Rack adapter (L registers are
    // NOT scaled — patches read LED registers back as logic values).
    float ledBrightness() const { return state_.master.ledBrightness; }

    // --- persistent state (DROIDSTA.BIN contract, hardware.md §11.1) ---------
    // saveState snapshots the manual-interaction state of every stateful circuit
    // (skipping those whose `dontsave` is high), numbering them per type in
    // patch-appearance order. restoreState walks the loaded patch's circuits and
    // hands each the matching (type, ordinal) entry if present and dontsave is
    // low; extra circuits keep their defaults, surplus saved entries are ignored,
    // and reordering same-type circuits mis-assigns state — all deliberately
    // faithful to the hardware.
    StateSnapshot saveState() const;
    void restoreState(const StateSnapshot& snap);

    // name: register ("I1", "P1.2", ...) or cable ("_X")
    bool setValue(const std::string& name, float v);   // marks I<n> patched
    float getValue(const std::string& name) const;
    void setInputPatched(int n, bool p) { state_.regs.setInputPatched(uint8_t(n), p); }

    // Controller mutation seam (golden harness + future Rack adapter). `name`
    // is an encoder handle "E<ctrl>.<num>" (chain lookup) or "E<n>" (direct
    // global index). Returns false if the name is not a resolvable encoder.
    bool turnEncoder(const std::string& name, long detents);
    bool pushEncoder(const std::string& name, bool down);
    // Motor-fader handles are "F<n>" (1-based global fader number). User
    // movement/touch; getValue("F<n>") reads the fader position back.
    bool moveFader(const std::string& name, float pos);
    bool touchFader(const std::string& name, bool down);
    bool pressFaderPlate(const std::string& name, bool down);
    // Parse-free overloads for the per-tick Rack adapter feed: an already-built
    // E-register id / 1-based global fader number instead of a name string.
    bool turnEncoder(const RegId& r, long detents) { return state_.controllers.turnEncoder(r, detents); }
    bool pushEncoder(const RegId& r, bool down) { return state_.controllers.pushEncoder(r, down); }
    bool moveFader(int fader1, float pos) {
        if (!state_.controllers.validateFader(fader1)) return false;
        state_.controllers.moveFader(fader1, pos);
        return true;
    }
    bool touchFader(int fader1, bool down) {
        if (!state_.controllers.validateFader(fader1)) return false;
        state_.controllers.touchFader(fader1, down);
        return true;
    }
    bool pressFaderPlate(int fader1, bool down) {
        if (!state_.controllers.validateFader(fader1)) return false;
        state_.controllers.pressFaderPlate(fader1, down);
        return true;
    }

    // --- controller-chain seams (Rack adapter + headless tests) -------------
    // Patch controller declarations in order (x7 never appears; the loader
    // excludes it to match hardware numbering).
    const std::vector<std::string>& declaredControllers() const { return declaredControllers_; }
    // Interned text table of the loaded patch (slot 0 == ""; manual §5.8).
    const std::vector<std::string>& texts() const { return texts_; }
    // Look up the text for a (possibly attenuated) number: floors v; returns ""
    // for v < 1, v >= texts().size(), or non-finite. 0 == the empty text.
    const std::string& textForNumber(float v) const;
    // Fast register access for the chain adapter: r must already be canonical
    // (the string setValue/getValue path canonicalizes; this one does not).
    void setRegister(const RegId& r, float v) { state_.regs.set(r, v); }
    float getRegister(const RegId& r) const { return state_.regs.get(r); }
    // True iff a circuit output of the loaded patch is bound to r (used by the
    // adapter for the hardware default-LED rule; L registers only in practice).
    bool registerDriven(const RegId& r) const { return drivenRegs_.count(pack(r)) != 0; }

    // Wave-3 readbacks for the Rack chain adapter (all bounds-safe: 0 on bad
    // index, never UB — the underlying encoder()/fader() return nullptr).
    float encoderRing(int global1) const {
        const EncoderState* es = state_.controllers.encoder(global1);
        return es ? es->ringDisplay : 0.0f;
    }
    float encoderStepLed(int global1) const {
        const EncoderState* es = state_.controllers.encoder(global1);
        return es ? es->stepLed : 0.0f;
    }
    float encoderStepLedColor(int global1) const {
        const EncoderState* es = state_.controllers.encoder(global1);
        return es ? es->stepLedColor : 0.0f;
    }
    float faderMotorTarget(int fader1) const {
        const FaderState* fs = state_.controllers.fader(fader1);
        return fs ? fs->motorTarget : 0.0f;
    }
    int faderNotches(int fader1) const {
        const FaderState* fs = state_.controllers.fader(fader1);
        return fs ? fs->notches : 0;
    }
    float faderLed(int fader1) const {
        const FaderState* fs = state_.controllers.fader(fader1);
        return fs ? fs->led : 0.0f;
    }
    float faderLedColor(int fader1) const {
        const FaderState* fs = state_.controllers.fader(fader1);
        return fs ? fs->ledColor : 0.0f;
    }

    // Per-DB8E display readback (Wave 3a Task 4): db8e1 is 1-based, counting
    // DB8Es only in chain order.
    int displayCount() const { return state_.controllers.displayCount(); }
    const DisplayState* displayState(int db8e1) const { return state_.controllers.display(db8e1); }

    // --- MIDI seams (M5) ----------------------------------------------------
    // Adapter -> engine: queue an inbound event, drained into the per-tick
    // snapshot at the start of the next tick().
    void sendMidiIn(MidiPort p, MidiEvent ev) { state_.midi.in[(int)p].push(ev); }
    // Engine -> adapter: pop one outbound event a circuit emitted this run.
    bool drainMidiOut(MidiPort p, MidiEvent& ev) { return state_.midi.out[(int)p].pop(ev); }
    // X7 expander presence (set by the controller chain, survives patch loads).
    bool x7Present() const { return state_.midi.x7; }
    void setX7Present(bool present) { state_.midi.x7 = present; }
    // True iff the loaded patch contains any MIDI circuit (midiin/midiout/
    // midithrough — the circuits gated on s.midi.available()). Lets the Rack
    // adapter warn when a MIDI patch has no reachable MIDI hardware (ISSUE-3).
    bool patchUsesMidi() const { return usesMidi_; }
    // True iff MIDI hardware is reachable (X7 attached or MASTER18 built-in) —
    // exactly what the MIDI circuits check before doing anything (midi.hpp
    // available()). A MIDI patch with this false runs silently.
    bool midiAvailable() const { return state_.midi.available(); }
    // Readback of the current tick's inbound snapshot depth (tests / adapter).
    int midiTickCount(MidiPort p) const { return state_.midi.tickCount[(int)p]; }

    // --- MASTER18 frequency-probe seam ---------------------------------------
    // Adapter -> engine: measured I1 frequency (positive zero-crossings/sec) and
    // signal presence. vcotuner reads s.probe each tick.
    void setProbe(float hz, bool present) { state_.probe.hz = hz; state_.probe.present = present; }

    // --- SD-card file seam (midifileplayer) -----------------------------------
    // Installs the file source; survives patch reloads (the card, not the patch,
    // provides the files).
    void setFileProvider(FileProvider fp) {
        fileProvider_ = std::move(fp);
        state_.fileProvider = fileProvider_;
    }

private:
    Operand makeOperand(const Atom& a) const;
    MasterType master_;
    float tickRateHz_;
    uint32_t seed_ = 1;
    EngineState state_;
    std::vector<std::unique_ptr<Circuit>> circuits_;   // patch order
    std::vector<Circuit*> circuitPtrs_;                // raw view for peer access
    std::unordered_map<std::string, uint16_t> cableIndex_;
    bool loaded_ = false;
    bool usesMidi_ = false;   // patch contains a midiin/midiout/midithrough circuit
    unsigned ramUsed_ = 0;
    std::vector<std::string> declaredControllers_;
    std::vector<std::string> texts_;
    std::unordered_set<uint32_t> drivenRegs_;
    FileProvider fileProvider_;   // survives load()'s state reset
    // [droid] output-conditioning filter state (per O1..O8) + the one-tick
    // clear/clearall broadcast pulses (see Engine::tick).
    float smooth_[8] = {};
    bool haveSmooth_[8] = {};
    std::vector<Input*> pokedClears_;
};

} // namespace droid
