#include "circuit.hpp"
#include "gatereader.hpp"   // kGateHighThreshold
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace droid {

// Bad jack access is a circuit-author (engine) bug, not patch input —
// fail loudly instead of UB-indexing.
[[noreturn]] static void jackFatal(const gen::CircuitDef* def, const char* name, const char* why) {
    std::fprintf(stderr, "vcvoid: fatal jack access in circuit '%s', jack '%s': %s\n",
                 def ? def->name : "?", name, why);
    std::abort();
}

void Circuit::allocateSlots(const gen::CircuitDef* d) {
    def = d;
    unsigned nIn = 0, nOut = 0;
    for (unsigned i = 0; i < d->numJacks; i++)
        (d->jacks[i].isInput ? nIn : nOut) += d->jacks[i].count;
    inputs.resize(nIn);
    outputs.resize(nOut);
    // seed input defaults
    unsigned slot = 0;
    for (unsigned i = 0; i < d->numJacks; i++) {
        const gen::JackDef& j = d->jacks[i];
        if (!j.isInput) continue;
        for (unsigned k = 0; k < j.count; k++)
            inputs[slot + k].setDefault(j.hasDefault ? j.defaultValue : 0.0f);
        slot += j.count;
    }
}

int Circuit::slotIndex(const gen::JackDef* jack, int arrayIndex) const {
    unsigned slot = 0;
    for (unsigned i = 0; i < def->numJacks; i++) {
        const gen::JackDef& j = def->jacks[i];
        if (j.isInput != jack->isInput) continue;
        if (&j == jack) return int(slot) + arrayIndex - 1;
        slot += j.count;
    }
    return -1;
}

// Resolve jack + effective array index. Honors a name-embedded index
// ("input3") when the caller leaves index = 1; conflicting nontrivial
// indices (name says 3, arg says 2) are a hard error.
const gen::JackDef* Circuit::resolveJack(const char* name, int& index, int wantInput) const {
    // def is set by allocateSlots(), which every code path runs before any
    // resolveJack()/in()/out() call. Guard the invariant cheaply: a null def
    // here would be a use-before-allocate engine bug, not patch input.
    if (!def) jackFatal(def, name, "resolveJack before allocateSlots");
    int nameIdx = 1;
    const gen::JackDef* j = gen::findJack(*def, name, nameIdx, wantInput);
    if (!j) jackFatal(def, name, "unknown jack name");
    if (nameIdx != 1) {
        if (index != 1 && index != nameIdx)
            jackFatal(def, name, "conflicting array indices (name vs. index argument)");
        index = nameIdx;
    }
    if (index < 1 || index > (int)j->count)
        jackFatal(def, name, "array index out of range");
    return j;
}

// Memoized front end for in()/out(): return the cached slot for this
// (name, index) pair, or resolve once through resolveJack()/slotIndex() and
// cache. in()/out() run with the same names every tick, and (per the
// repo-wide audit in circuit.hpp) every call site passes a name whose
// POINTER is stable tick to tick, so the hit path compares the stored
// pointer first — three scalar compares, no strcmp — and only falls back to
// a content compare (and then repairs the stored pointer) if the pointer
// ever changes. The key uses the CALLER's index (before any name-embedded
// index like "input3" rewrites it), so each call site keys consistently.
int Circuit::memoSlot(const char* name, int index, bool wantInput) {
    // A circuit's tick() replays the same in()/out() sequence every tick, so
    // the entry after the previous hit is almost always the next one asked
    // for: scan from a rotating cursor and the common case is one probe.
    size_t n = jackMemo_.size();
    for (size_t k = 0; k < n; k++) {
        size_t pos = memoCursor_ + k;
        if (pos >= n) pos -= n;
        JackMemo& e = jackMemo_[pos];
        // direction is part of the key: a circuit may use one name for both an
        // input and an output (algoquencer's `pitch`)
        if (e.index != index || e.isInput != wantInput) continue;
        if (e.ptr == name) {
            // Pointer-identity fast path. Dynamically verify the repo-wide
            // invariant (same pointer => same content) in non-NDEBUG builds;
            // unit tests + all 566 goldens run with asserts active, so this
            // proves the invariant across every circuit they exercise. The
            // bench target defines NDEBUG and skips the check.
            assert(std::strcmp(e.name.c_str(), name) == 0);
            memoCursor_ = pos + 1 < n ? pos + 1 : 0;
            return e.slot;
        }
        // Pointer mismatch: fall back to content compare (handles two
        // distinct-pointer literals/tables with equal content sharing a
        // key). On a content match, adopt the new pointer so subsequent
        // ticks from this call site take the fast path.
        if (e.name[0] == name[0] && std::strcmp(e.name.c_str(), name) == 0) {
            e.ptr = name;
            memoCursor_ = pos + 1 < n ? pos + 1 : 0;
            return e.slot;
        }
    }
    int effIndex = index;
    const gen::JackDef* j = resolveJack(name, effIndex, wantInput ? 1 : 0);
    if (j->isInput != wantInput)
        jackFatal(def, name, wantInput ? "in() called on an output jack"
                                       : "out() called on an input jack");
    int slot = slotIndex(j, effIndex);
    int limit = int(wantInput ? inputs.size() : outputs.size());
    if (slot < 0 || slot >= limit)
        jackFatal(def, name, wantInput ? "bad input slot" : "bad output slot");
    jackMemo_.push_back({name, name, index, slot, wantInput});
    return slot;
}

// Locate the `dontsave` jack of this circuit's def (const, memo-free). Returns
// {jack, slot} or {nullptr, -1} if the circuit type has no dontsave input.
static const gen::JackDef* findDontsave(const gen::CircuitDef* def, int& slot) {
    slot = -1;
    if (!def) return nullptr;
    unsigned s = 0;
    for (unsigned i = 0; i < def->numJacks; i++) {
        const gen::JackDef& j = def->jacks[i];
        if (!j.isInput) continue;
        if (std::strcmp(j.name, "dontsave") == 0) { slot = (int)s; return &j; }
        s += j.count;
    }
    return nullptr;
}

bool Circuit::isStateful() const {
    int slot;
    return findDontsave(def, slot) != nullptr;
}

bool Circuit::dontsaveActive(const EngineState& s) const {
    int slot;
    if (!findDontsave(def, slot) || slot < 0 || slot >= (int)inputs.size())
        return false;
    return inputs[slot].value(s) >= kGateHighThreshold;
}

Input& Circuit::in(const char* name, int index) {
    return inputs[memoSlot(name, index, /*wantInput=*/true)];
}

Output& Circuit::out(const char* name, int index) {
    return outputs[memoSlot(name, index, /*wantInput=*/false)];
}

} // namespace droid
