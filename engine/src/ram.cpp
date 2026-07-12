#include "ram.hpp"
#include <cstdio>
#include <set>
#include <string>

// ---------------------------------------------------------------------------
// Forge parity notes (read tools/droidcheck/vendor/droidforge/droidforge/...):
//
// Per-jack bytes (jackdeduplicator.cpp:33-45): taptempo_input 40, trigger_input
// 20, trigger_output 8, output 4, input 16. Inputs get optimized (lines 64-80):
// a "simple" input (single atom, no * or +) drops 16 -> 8, and if that single
// atom is the number 0.0 or 1.0 it drops to 4. Only inputs are optimized.
//
// Constants (patch.cpp:1097 countUniqueConstants). This is the subtle part and
// the reference implementation in the task brief got it wrong:
//   1. The set is PRE-SEEDED with canonized 0.0 and 1.0 -> numConstants >= 2
//      always, so the "stuff" block is never 0 (min ALIGN_UP(8,16) = 16).
//   2. For every NON-ZERO source number n it inserts BOTH +n and -n.
//   3. For a fraction atom (division divisor, ATOM_NUMBER_FRACTION) it ALSO
//      inserts 1/n and -1/n.
//   4. n == 0.0 is skipped (already seeded).
// The Forge has no "fromSource" concept: it counts every AtomNumber that is
// actually present. Our parser materializes implicit B=1 / C=0 as fromSource
// =false atoms the Forge never creates, so we skip those; every atom the Forge
// WOULD create is fromSource=true in ours (including the -1 of `X - REG`, whose
// Forge form6 emits a literal AtomNumber(-1)). See parser.cpp header note.
//
// Stuff block (patch.cpp:795-801, numTexts=0 in M1):
//   ALIGN_UP(numConstants*4 + numCables*8 + 0, 16)
//
// Budget walk (patch.cpp:787-813): iterate circuits in patch order; if
// used_so_far + thisCircuitMem + stuff > availableMemory, record a problem on
// that circuit and keep accumulating (later circuits may also overflow).
//
// Controllers (patch.cpp:768-771): start from sum of controller ramSizes; on a
// MASTER (typeOfMaster()==16) always add the X7's 864 bytes, attached or not.
// ---------------------------------------------------------------------------

namespace droid {

static unsigned alignUp(unsigned x, unsigned n) { return (x + n - 1) / n * n; }

// Mirrors Patch::canonizeNumber: fixed 10 decimals, so values that differ only
// by float noise collapse to one entry (matters only for the unique count).
static std::string canon(double n) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.10f", n);
    return std::string(buf);
}

static unsigned jackCost(const CompiledParam& p) {
    switch (p.def->ramHint) {
        case gen::RamHint::TaptempoInput: return 40;
        case gen::RamHint::TriggerInput:  return 20;
        case gen::RamHint::TriggerOutput: return 8;
        case gen::RamHint::Output:        return 4;
        case gen::RamHint::Input:
            if (p.simple) {
                if (p.a.kind == Atom::Kind::Number &&
                    (p.a.number == 0.0f || p.a.number == 1.0f))
                    return 4;   // 0/1 special optimization
                return 8;       // simple input
            }
            return 16;          // A*B+C input
    }
    return 16;                  // unreachable; matches Forge's defensive default
}

unsigned computeRam(const CompiledPatch& p, MasterType master,
                    std::vector<LoadError>& errorsOut) {
    unsigned used = 0;
    for (auto& name : p.controllers)
        if (auto* c = gen::findController(name)) used += c->ramSize;
    if (master == MasterType::Master16)
        if (auto* x7 = gen::findController("x7")) used += x7->ramSize;  // always reserved

    // Unique constants, mirroring Patch::countUniqueConstants.
    std::set<std::string> constants;
    constants.insert(canon(0.0));
    constants.insert(canon(1.0));
    for (auto& cc : p.circuits)
        for (auto& pp : cc.params)
            for (const Atom* a : {&pp.a, &pp.b, &pp.c}) {
                if (a->kind != Atom::Kind::Number || !a->fromSource) continue;
                // For a fraction (`X / d`) the Forge stores the folded value as a
                // DOUBLE `1.0 / (double)d` (jackassignmentinput.cpp:249) and counts
                // n, -n, 1/n, -1/n off that double. Our runtime `number` is the
                // folded FLOAT, whose double-canonization carries visible float
                // noise (1/12's inverse lands at 11.9999994, not 12.0), so recompute
                // from the exact divisor to match the Forge byte-for-byte.
                double n = a->isFraction ? 1.0 / double(a->fractionDenom) : double(a->number);
                if (n == 0.0) continue;
                constants.insert(canon(-n));
                constants.insert(canon(n));
                if (a->isFraction) {
                    constants.insert(canon(1.0 / -n));
                    constants.insert(canon(1.0 / n));
                }
            }

    unsigned numTexts = 0;   // M1: text parameters unsupported
    unsigned stuff = alignUp((unsigned)constants.size() * 4 +
                             (unsigned)p.cableNames.size() * 8 +
                             numTexts * 4 + alignUp(numTexts * 2, 4), 16);

    unsigned budget = gen::kAvailableMemory[master == MasterType::Master16 ? 0 : 1];
    for (auto& cc : p.circuits) {
        unsigned mem = cc.def->ramSize;
        for (auto& pp : cc.params) mem += jackCost(pp);
        if (used + mem + stuff > budget)
            errorsOut.push_back({cc.line, "This circuit exceeds the available memory"});
        used += mem;
    }
    return used + stuff;
}

} // namespace droid
