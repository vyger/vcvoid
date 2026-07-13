#include "circuit.hpp"
#include "gatereader.hpp"   // kGateHighThreshold
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

// Double jackTable_'s capacity (or seed it at 16) and rehash all live
// entries. Called only from memoInsert when the table is >= 3/4 full, i.e.
// during the first tick's resolutions — steady state is lookup-only.
void Circuit::memoGrow() {
    std::vector<JackMemo> old = std::move(jackTable_);
    size_t newCap = old.empty() ? 16 : old.size() * 2;
    jackTable_.clear();
    jackTable_.resize(newCap);
    for (auto& e : old) {
        if (!e.ptr) continue;
        uint32_t mask = uint32_t(jackTable_.size() - 1);
        uint32_t pos = memoHash(e.ptr, e.index, e.isInput) & mask;
        while (jackTable_[pos].ptr) pos = (pos + 1) & mask;
        jackTable_[pos] = std::move(e);
    }
}

// Insert-only (tombstone-free): grow first if load factor would exceed 3/4,
// then linear-probe to the first empty slot.
void Circuit::memoInsert(const char* name, int index, bool isInput, int slot) {
    if (jackTable_.empty() || (jackCount_ + 1) * 4 > jackTable_.size() * 3)
        memoGrow();
    uint32_t mask = uint32_t(jackTable_.size() - 1);
    uint32_t pos = memoHash(name, index, isInput) & mask;
    while (jackTable_[pos].ptr) pos = (pos + 1) & mask;
    jackTable_[pos] = JackMemo{name, index, slot, isInput, std::string(name)};
    jackCount_++;
}

// Memoized front end for in()/out(): return the cached slot for this
// (name, index) pair, or resolve once through resolveJack()/slotIndex() and
// cache. in()/out() run with the same names every tick, and (per the
// repo-wide audit in circuit.hpp) every call site passes a name whose
// POINTER is stable tick to tick, so this is a pointer-keyed open-addressed
// hash lookup: hash -> linear probe -> 3-scalar compare, no string touch on
// the hit path. The key uses the CALLER's index (before any name-embedded
// index like "input3" rewrites it), so each call site keys consistently.
int Circuit::memoSlot(const char* name, int index, bool wantInput) {
    if (!jackTable_.empty()) {
        uint32_t mask = uint32_t(jackTable_.size() - 1);
        uint32_t pos = memoHash(name, index, wantInput) & mask;
        for (;;) {
            JackMemo& e = jackTable_[pos];
            if (!e.ptr) break;   // insert-only table: empty slot ends the probe
            if (e.ptr == name && e.index == index && e.isInput == wantInput) {
#if VCVOID_VERIFY_MEMO
                // Pointer-identity hit. Dynamically verify the repo-wide
                // invariant (same pointer => same content). Compiled only
                // when VCVOID_VERIFY_MEMO is defined (unittests/droidtest —
                // see root Makefile): the Rack SDK builds this plugin with
                // -O3 and no -DNDEBUG, so a plain assert() here would still
                // run (and could abort the audio thread) in the shipped
                // plugin. Gating on an explicit macro instead of NDEBUG
                // keeps the check out of every build except the ones that
                // exist to prove the invariant.
                if (std::strcmp(e.name.c_str(), name) != 0) {
                    std::fprintf(stderr,
                        "VCVOID_VERIFY_MEMO: memo pointer/content mismatch in circuit '%s': "
                        "pointer matched but name changed from '%s' to '%s'\n",
                        def ? def->name : "?", e.name.c_str(), name);
                    std::abort();
                }
#endif
                return e.slot;
            }
            if (e.index == index && e.isInput == wantInput &&
                e.name[0] == name[0] && std::strcmp(e.name.c_str(), name) == 0) {
                // Pointer mismatch but content match: a distinct-pointer
                // literal/table entry with equal content landed on this
                // key. Resolve via the content path (already done — we just
                // compared it) and adopt the new pointer so subsequent ticks
                // from this call site take the fast path. This strcmp is
                // functional (probe resolution), not a debug-only check, so
                // it stays compiled in all builds. If this branch is never
                // taken, the loop below simply keeps probing and — per the
                // repo-wide stable-pointer invariant — may insert a second,
                // functionally-equivalent entry for the same logical jack at
                // a later slot; that's benign (a few wasted table slots), not
                // a correctness bug, so this adopt path is an optimization,
                // not a requirement.
                e.ptr = name;
                return e.slot;
            }
            pos = (pos + 1) & mask;
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
    memoInsert(name, index, wantInput, slot);
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
