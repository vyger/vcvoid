#pragma once
#include "signal.hpp"
#include "../gen/jacktables.gen.hpp"
#include <cmath>
#include <vector>

namespace droid {

// --- persistent-state serialization (DROIDSTA.BIN contract) ---------------
// A stateful circuit serializes the RESULT OF MANUAL INTERACTION (button
// toggles, dialed values, entered patterns, presets, calibration) into a flat
// versioned list of doubles — JSON-friendly and diffable (hardware.md §11.1) —
// never the runtime dynamics (transport position, lfo phase, gate windows).
// These tiny cursors keep the per-circuit save/load code terse; loadState only
// runs after a matching size/version check, so the reader never underflows.
struct StateWriter {
    std::vector<double>& v;
    void f(double x) { v.push_back(x); }
    void n(long x)   { v.push_back((double)x); }
    void b(bool x)   { v.push_back(x ? 1.0 : 0.0); }
};
struct StateReader {
    const std::vector<double>& v;
    size_t i = 0;
    double f() { return i < v.size() ? v[i++] : 0.0; }
    long   n() { return std::lround(f()); }
    bool   b() { return f() != 0.0; }
};

class Circuit {
public:
    virtual ~Circuit() = default;
    virtual void tick(EngineState& s) = 0;

    // --- persistent state (DROIDSTA.BIN contract, hardware.md §11.1) --------
    // Stateful circuits (those with a `dontsave` jack) override these. The blob
    // is a flat list of doubles tagged with stateVersion(); loadState MUST leave
    // the circuit at its defaults when it does not recognize the version/length
    // (tolerate an unparseable blob). Default: no-op (non-stateful circuits).
    virtual int  stateVersion() const { return 0; }
    virtual void saveState(StateWriter&) const {}
    virtual void loadState(EngineState&, int /*version*/, const std::vector<double>& /*in*/) {}

    // True iff this circuit type persists state: it declares a `dontsave` jack
    // (the manual's exact rule for which circuits are saved). Const/memo-free so
    // Engine::saveState() can stay const.
    bool isStateful() const;
    // Current value of the `dontsave` input (>= gate threshold). A stateful
    // circuit with dontsave high is skipped for BOTH save and load (manual:
    // "prevent the state from being saved (and loaded)").
    bool dontsaveActive(const EngineState& s) const;

    // Jack access by base name + 1-based array index (index 1 for scalars).
    Input& in(const char* name, int index = 1);
    Output& out(const char* name, int index = 1);

    // loader-facing: flattened jack slots in jack-table order (arrays expanded)
    // aborts on author error; wantInput: -1 either, 1 inputs only, 0 outputs only
    const gen::JackDef* resolveJack(const char* name, int& index, int wantInput = -1) const;
    void allocateSlots(const gen::CircuitDef* def);
    int slotIndex(const gen::JackDef* jack, int arrayIndex) const;
    const gen::CircuitDef* def = nullptr;
    std::vector<Input> inputs;
    std::vector<Output> outputs;

private:
    // Memoized name->slot resolution. in()/out() are called with the same
    // (name, index) pairs every tick; resolving through gen::findJack each
    // time (linear prefix search + std::string temp) dominated the audio
    // thread in profiles. Keyed by name CONTENT, not pointer — a few circuits
    // (fadermatrix, matrixmixer) snprintf names into a reused stack buffer.
    struct JackMemo { std::string name; int index; int slot; bool isInput; };
    std::vector<JackMemo> jackMemo_;
    size_t memoCursor_ = 0;   // scan start: entry after the previous hit
    int memoSlot(const char* name, int index, bool wantInput);

public:

    // Neighbour access for chainable circuits (e.g. sequencer's chaintonext).
    // Set by the Engine after load: `peers_` lists every circuit in patch order
    // and `peerIndex_` is this circuit's slot in it. nextPeer() returns the
    // circuit immediately after this one, or nullptr if this is the last.
    const std::vector<Circuit*>* peers_ = nullptr;
    int peerIndex_ = -1;
    Circuit* nextPeer() const {
        if (!peers_ || peerIndex_ < 0) return nullptr;
        size_t n = size_t(peerIndex_) + 1;
        return n < peers_->size() ? (*peers_)[n] : nullptr;
    }
};

} // namespace droid
