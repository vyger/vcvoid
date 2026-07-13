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
    // thread in profiles.
    //
    // Round 2 established (and non-NDEBUG builds dynamically prove, via the
    // 566 golden files) the repo-wide invariant: every call site in the
    // engine passes a name whose POINTER is stable across ticks (a string
    // literal, or an entry from a static/per-instance table built once — see
    // circuit.cpp for the audit; fadermatrix/matrixmixer were converted to
    // stable per-index name tables to satisfy it). Round 3 exploits that
    // invariant directly with a pointer-keyed open-addressed hash table
    // instead of a cursor-scanned vector: circuits with conditional jack
    // access (button's `if (selected)`, motorfader/seqcore preset paths)
    // desync a rotating scan cursor every tick, degrading it to a linear
    // scan over ~40-byte string-bearing entries — the table removes that
    // scan entirely (hash -> probe -> 3-scalar compare, no string touch on
    // the hit path).
    //
    // Table entries are pointer-keyed: (ptr, index, isInput) -> slot. `name`
    // (the CONTENT, as a std::string) is retained only to back the miss path
    // (a new pointer for an already-seen name must resolve to the SAME slot)
    // and, in non-NDEBUG builds, to dynamically re-verify content equality on
    // every hit — so `make test`'s golden suite continues to prove the
    // invariant. NDEBUG (the bench target) skips that verification.
    struct JackMemo { const char* ptr = nullptr; int index = 0; int slot = 0; bool isInput = false; std::string name; };
    std::vector<JackMemo> jackTable_;   // power-of-two capacity; empty slots have ptr == nullptr
    unsigned jackCount_ = 0;            // live entries, for grow-on-load
    static uint32_t memoHash(const char* ptr, int index, bool isInput) {
        uint64_t h = (uintptr_t(ptr) >> 3) * 0x9E3779B97F4A7C15ull;
        h ^= uint64_t(unsigned(index) << 1 | (isInput ? 1u : 0u));
        h ^= h >> 33;
        return uint32_t(h);
    }
    void memoGrow();
    void memoInsert(const char* name, int index, bool isInput, int slot);
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
