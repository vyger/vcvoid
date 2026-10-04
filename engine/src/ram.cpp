#include "ram.hpp"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

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
// Stuff block (patch.cpp:795-801):
//   ALIGN_UP(numConstants*4 + numCables*8 + numTexts*4 + ALIGN_UP(numTexts*2,4), 16)
// i.e. 6 bytes per TEXT ATOM OCCURRENCE (a 4-byte pointer + a 2-byte length);
// the characters themselves live in the patch buffer and are never charged, so
// a 4-character and a 40-character pattern cost the same. countTexts()
// (patch.cpp:1143) walks atoms, so a repeated string is charged per use.
//
// Budget walk (patch.cpp:787-813): iterate circuits in patch order; if
// used_so_far + thisCircuitMem + stuff > availableMemory, record a problem on
// that circuit and keep accumulating (later circuits may also overflow).
//
// Controllers (patch.cpp:768-771): start from sum of controller ramSizes; on a
// MASTER (typeOfMaster()==16) always add the X7's 864 bytes, attached or not.
// (Patch::usedRAM counts the X7 only when needsX7(); the BUDGET rule — the one
// that decides whether a patch loads — always reserves it. We follow the budget
// rule, and tools/ramcheck.sh corrects for the difference on the Forge side.)
//
// Shared input values (issue #88; jackdeduplicator.cpp:81-99). The Forge's
// deploy preference "Detect and share duplicate values for inputs"
// (compression/deduplicate_jacks) makes the deployed patch reuse one jack-table
// entry for every input written with the same value, so each repeat costs 0:
//   1. ONLY jacks whose ramhint is `input` are sharable — not trigger_input,
//      not taptempo_input, and no output kind.
//   2. The key is JackAssignmentInput::valueToCanonicalString(): the atoms that
//      are actually present, joined by " * " and " + " in SLOT order (see
//      canonicalInputValue below). It is purely syntactic after parsing, so
//      `I2 - I1` and `-1 * I1 + I2` share, while `I1 * -1 + I2` does not.
//   3. ONE deduplicator runs over the WHOLE patch in circuit order — sharing is
//      not per circuit, and the first occurrence is the one that pays.
//   4. A shared repeat costs no texts either: each text atom in it increments
//      savedTexts, and the stuff block is sized on countTexts() - savedTexts.
//      Note the Forge recomputes that block INSIDE the per-circuit budget walk
//      from the savedTexts known so far (patch.cpp:779-801), so the overhead
//      shrinks as the walk proceeds; the reported total uses the final count.
// Constants and cables are NOT affected — they are already counted uniquely.
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

// Mirrors AtomNumber::niceNumber (patch/atomnumber.cpp): fixed-point with
// NUMBER_DIGITS (14, main/tuning.h) minus the value's decimal exponent, then
// trailing zeros and a trailing '.' chopped. Only the equivalence classes this
// induces matter here — two inputs share iff their canonical strings are equal.
static std::string niceNumber(double num) {
    int l = num == 0.0 ? 0 : (int)std::log10(std::fabs(num));
    int precision = 14 - l;
    if (precision < 0) precision = 0;
    if (precision > 60) precision = 60;    // snprintf sanity; never hit in practice
    char buf[128];
    std::snprintf(buf, sizeof buf, "%.*f", precision, num);
    std::string s(buf);
    while (s.find('.') != std::string::npos && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

// Atom::toCanonicalString(): niceNumber for an AtomNumber, the register
// spelling for an AtomRegister, "_name" for an AtomCable, the quoted content
// for an AtomText. The Forge lowercases register and cable tokens while
// parsing, so we lowercase the cable here (our RegId is already normalized).
static std::string atomCanon(const Atom& a, const std::vector<std::string>& texts) {
    switch (a.kind) {
        case Atom::Kind::Cable: {
            std::string s = a.cable;
            for (auto& ch : s) ch = char(std::tolower((unsigned char)ch));
            return s;
        }
        case Atom::Kind::Register:
            return toString(a.reg);
        case Atom::Kind::Number: {
            if (a.isText) {
                size_t idx = a.number < 0 ? 0 : (size_t)a.number;
                return "\"" + (idx < texts.size() ? texts[idx] : std::string()) + "\"";
            }
            // A fraction's stored value is the folded DOUBLE 1.0/divisor
            // (jackassignmentinput.cpp:249), not our folded float — recompute
            // it from the exact divisor, exactly as the constant count does.
            return niceNumber(a.isFraction ? 1.0 / double(a.fractionDenom)
                                           : double(a.number));
        }
        default:
            return std::string();
    }
}

std::string canonicalInputValue(const CompiledParam& p,
                                const std::vector<std::string>& texts) {
    // A is always present for a jack that parsed (the Forge's atoms[0] is null
    // only for an undefined jack, which never reaches the jack table). B and C
    // exist only when they came from the source: the implicit B=1 / C=0 we
    // materialize are the Forge's null atoms and must not appear in the key.
    const Atom* a = &p.a;
    const Atom* b = p.b.fromSource ? &p.b : nullptr;
    // `X - REG` lands in our slots as A=REG, B=-1 but in the Forge's as
    // A=-1, B=REG. Swap back, so the two spellings of it key alike.
    if (p.subtractForm && b) std::swap(a, b);
    const Atom* c = p.c.fromSource ? &p.c : nullptr;

    std::string s = atomCanon(*a, texts);
    if (b) s += " * " + atomCanon(*b, texts);
    if (c) s += " + " + atomCanon(*c, texts);
    return s;
}

static unsigned jackCost(const CompiledParam& p) {
    switch (p.def->ramHint) {
        case gen::RamHint::TaptempoInput: return 40;
        case gen::RamHint::TriggerInput:  return 20;
        case gen::RamHint::TriggerOutput: return 8;
        case gen::RamHint::Output:        return 4;
        case gen::RamHint::Input:
            if (p.simple) {
                // The 0/1 optimization tests isNumber() in the Forge
                // (jackdeduplicator.cpp:71-79); a text is an AtomText, so it
                // never qualifies — even though our parser gives text number 1
                // the numeric value 1.0.
                if (!p.a.isText && p.a.kind == Atom::Kind::Number &&
                    (p.a.number == 0.0f || p.a.number == 1.0f))
                    return 4;   // 0/1 special optimization
                return 8;       // simple input
            }
            return 16;          // A*B+C input
    }
    return 16;                  // unreachable; matches Forge's defensive default
}

unsigned computeRam(const CompiledPatch& p, MasterType master, bool shareInputValues,
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
                if (a->isText) continue;   // AtomText is not an AtomNumber (patch.cpp:1107)
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

    // Text atoms: countTexts() (patch.cpp:1143) counts OCCURRENCES, not unique
    // texts — the same string written twice is two atoms and costs twice, even
    // though both intern to one text number.
    unsigned numTexts = 0;
    for (auto& cc : p.circuits)
        for (auto& pp : cc.params)
            for (const Atom* a : {&pp.a, &pp.b, &pp.c})
                if (a->isText) numTexts++;

    // Per-circuit footprint, plus the running saved-text count as of the end of
    // that circuit. The one deduplicator spans the whole patch: the Forge builds
    // a single JackDeduplicator and hands it to every Circuit::RAMUsage in turn.
    std::set<std::string> shared;
    std::vector<unsigned> circuitMem(p.circuits.size(), 0);
    std::vector<unsigned> savedTextsAfter(p.circuits.size(), 0);
    unsigned savedTexts = 0;
    for (size_t i = 0; i < p.circuits.size(); i++) {
        const CompiledCircuit& cc = p.circuits[i];
        unsigned mem = cc.def->ramSize;
        for (auto& pp : cc.params) {
            unsigned cost = jackCost(pp);
            if (shareInputValues && pp.def->ramHint == gen::RamHint::Input) {
                if (!shared.insert(canonicalInputValue(pp, p.texts)).second) {
                    cost = 0;        // deployed as a reference "@<offset>"
                    for (const Atom* a : {&pp.a, &pp.b, &pp.c})
                        if (a->isText) savedTexts++;
                }
            }
            mem += cost;
        }
        circuitMem[i] = mem;
        savedTextsAfter[i] = savedTexts;
    }

    auto stuffFor = [&](unsigned saved) {
        unsigned t = numTexts - saved;
        return alignUp((unsigned)constants.size() * 4 +
                       (unsigned)p.cableNames.size() * 8 +
                       t * 4 + alignUp(t * 2, 4), 16);
    };

    unsigned budget = gen::kAvailableMemory[master == MasterType::Master16 ? 0 : 1];
    for (size_t i = 0; i < p.circuits.size(); i++) {
        // The Forge re-sizes the stuff block on every circuit from the texts
        // saved so far, so the overhead a later circuit is measured against can
        // be smaller than the one an earlier circuit saw.
        if (used + circuitMem[i] + stuffFor(savedTextsAfter[i]) > budget) {
            // The Forge appends its own hint here when sharing is off
            // (patch.cpp:805-807); ours names the menu item that turns it on.
            std::string msg = "This circuit exceeds the available memory";
            if (!shareInputValues) msg += " (try \"Share duplicate input values\")";
            errorsOut.push_back({p.circuits[i].line, msg, ErrorCode::OutOfMemory});
        }
        used += circuitMem[i];
    }
    return used + stuffFor(savedTexts);
}

} // namespace droid
