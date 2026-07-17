#include "harness.hpp"
#include "src/loader.hpp"
using namespace droid;

// RAM-accounting expectations are cross-checked against the Droid Forge's
// Patch::updateMemoryProblems + Patch::countUniqueConstants (patch.cpp) and
// JackDeduplicator::processJackAssignment (jackdeduplicator.cpp).
//
// The single most important correction over the naive hand-computed numbers:
// countUniqueConstants ALWAYS seeds {0.0, 1.0} and, for every non-zero source
// number n, inserts BOTH +n and -n (and 1/n, -1/n for fractions). So the
// "stuff" block is never zero (minimum numConstants == 2 -> 16 bytes after
// alignment), and several expectations differ from a +1-per-value model.

static unsigned ramOf(const std::string& text, bool expectOk = true) {
    CompiledPatch cp;
    auto r = compilePatch(text, MasterType::Master16, cp);
    CHECK(r.ok == expectOk);
    return cp.ramUsed;
}

TEST(ram_single_copy) {
    // x7 864 + copy base 24 + input "I1 * 0.5" (not simple: 16) + output (4)
    // constants: seed{0,1} + {0.5,-0.5} = 4 -> ALIGN_UP(16,16) = 16   => 924
    CHECK(ramOf("[copy]\n input = I1 * 0.5\n output = O1\n") == 924);
}

TEST(ram_simple_input_optimizations) {
    // simple register input -> 8.  Constants seed{0,1}=2 -> stuff ALIGN_UP(8,16)=16.
    // 864 + 24 + 8 + 4 + 16 = 916   (hand-computed 900 was WRONG: it forgot the
    // always-seeded 0/1 constants that make stuff 16, not 0.)
    CHECK(ramOf("[copy]\n input = I1\n output = O1\n") == 916);
    // simple constant 1.0 -> 4.  Constants {0,1,-1}=3 -> ALIGN_UP(12,16)=16.
    // 864 + 24 + 4 + 4 + 16 = 912
    CHECK(ramOf("[copy]\n input = 1\n output = O1\n") == 912);
    // simple constant 0.3 -> 8.  Constants {0,1,0.3,-0.3}=4 -> ALIGN_UP(16,16)=16.
    // 864 + 24 + 8 + 4 + 16 = 916
    CHECK(ramOf("[copy]\n input = 0.3\n output = O1\n") == 916);
}

TEST(ram_cables_and_shared_constants) {
    // two copies, one cable, constant 0.5.
    // 864 + copy1(24+16+4=44) + copy2(24 + _X simple cable 8 + 4 = 36)
    // constants {0,1,0.5,-0.5}=4, cables=1 -> ALIGN_UP(4*4 + 1*8, 16)=ALIGN_UP(24,16)=32
    // 864 + 44 + 36 + 32 = 976   (hand-computed 964 undercounted constants.)
    unsigned r = ramOf("[copy]\n input = I1 * 0.5\n output = _X\n"
                       "[copy]\n input = _X\n output = O1\n");
    CHECK(r == 976);
}

TEST(ram_controllers_counted) {
    // + p2b8 144.  copy: 24 + P1.1 simple reg 8 + output 4 = 36.
    // constants seed{0,1}=2 -> stuff 16.
    // 864 + 144 + 36 + 16 = 1060   (hand-computed 1044 forgot the stuff block.)
    CHECK(ramOf("[p2b8]\n[copy]\n input = P1.1\n output = O1\n") == 1060);
}

// Regression: the synthesized -1 factor of the `X - REG` shorthand IS a real
// AtomNumber(-1) in the Forge (jackassignmentinput.cpp form6 sets a = "-1"),
// so it must be counted as a constant. Constructed so that -1 is the 9th unique
// constant, crossing a 16-byte alignment boundary (8 -> 9 constants: 32 -> 48).
TEST(ram_register_subtraction_counts_minus_one) {
    // copy1 input "I1 * 0.3 + 0.7" not simple 16, output 4, base 24 = 44
    // copy2 input "5 - I2" (A=I2,B=-1,C=5) not simple 16, output 4, base 24 = 44
    // constants: {0,1, 0.3,-0.3, 0.7,-0.7, 5,-5, -1} = 9 -> ALIGN_UP(36,16)=48
    // 864 + 44 + 44 + 48 = 1000
    unsigned r = ramOf("[copy]\n input = I1 * 0.3 + 0.7\n output = O1\n"
                       "[copy]\n input = 5 - I2\n output = O2\n");
    CHECK(r == 1000);
}

// Fraction divisor: `I1 / 12` folds to A=I1, B=1/12 (fromSource=true,
// isFraction=true; see parser.cpp canonicalize()). countUniqueConstants for a
// fraction atom inserts n, -n, 1/n AND -1/n (ram.cpp), on top of the
// always-seeded {0,1}. Not "simple" (3 tokens: I1, /, 12) so the input jack
// costs the full A*B+C 16, not the simple-input 8.
TEST(ram_fraction_divisor) {
    // constants: seed{0,1} plus for n=1/12: {-1/12, 1/12, 1/n=12, -1/n=-12}
    //   = {0,1,-1/12,1/12,12,-12} = 6 unique -> stuff = ALIGN_UP(6*4,16) = ALIGN_UP(24,16) = 32
    // circuit mem: copy base 24 + input (not simple) 16 + output 4 = 44
    // total: x7 864 + 44 + 32 = 940
    CHECK(ramOf("[copy]\n input = I1 / 12\n output = O1\n") == 940);
}

// Regression (M4 parity): the folded fraction value and its inverse must be
// canonized at DOUBLE precision, exactly as the Forge stores them
// (AtomNumber(1.0 / (double)bf), jackassignmentinput.cpp:249). Folding in float
// makes 1/12's inverse land at 11.9999994 rather than 12.0, so a literal `* 12`
// elsewhere would wrongly count as a NEW unique constant. With the divisor kept
// exactly, `/ 12`'s inverse is 12.0 and a literal 12 COLLAPSES onto it, whereas
// a literal 13 stays distinct. The `+ 3` pads the count so the ±2 difference
// crosses a 16-byte alignment boundary (before this fix both patches tied).
TEST(ram_fraction_inverse_double_precision) {
    unsigned collapses = ramOf("[copy]\n input = I1 / 12\n output = O1\n"
                               "[copy]\n input = I2 * 12 + 3\n output = O2\n");
    unsigned distinct  = ramOf("[copy]\n input = I1 / 12\n output = O1\n"
                               "[copy]\n input = I2 * 13 + 3\n output = O2\n");
    CHECK(collapses < distinct);
}

TEST(ram_over_budget) {
    // 7 bare cvloopers: 864 + 7*17336 = 122216 > 112867 -> error on a circuit line
    std::string patch;
    for (int i = 0; i < 7; i++) patch += "[cvlooper]\n";
    CompiledPatch cp;
    auto r = compilePatch(patch, MasterType::Master16, cp);
    CHECK(!r.ok);
    bool found = false;
    for (auto& e : r.errors)
        if (e.message.find("exceeds the available memory") != std::string::npos) found = true;
    CHECK(found);
}

// Experimental "ignore hardware memory limits" (#13): with the option set, the
// same over-budget patch compiles ok — overflows downgrade to warnings, and
// ramUsed still reports the true (over-budget) footprint.
TEST(ram_over_budget_ignored) {
    std::string patch;
    for (int i = 0; i < 7; i++) patch += "[cvlooper]\n";
    CompiledPatch cp;
    LoadOptions opts;
    opts.ignoreMemoryLimits = true;
    auto r = compilePatch(patch, MasterType::Master16, cp, opts);
    CHECK(r.ok);
    CHECK(r.errors.empty());
    bool warned = false;
    for (auto& w : r.warnings)
        if (w.find("exceeds the available memory") != std::string::npos) warned = true;
    CHECK(warned);
    CHECK(r.ramUsed > 112867);   // still the honest footprint
}

// The 64 000-byte patch-size cap is a hardware limit too: hard error normally,
// warning under ignoreMemoryLimits (the patch must still parse and run).
TEST(patch_size_cap_ignored) {
    // Pad past 64 000 bytes with a valid copy chain (stripPatch removes
    // comments/whitespace, so the padding must be real circuit text; each
    // cable is written once and read once so the patch stays well-formed).
    std::string patch = "[copy]\n input = I1\n output = _C0\n";
    int n = 0;
    while (stripPatch(patch).size() <= 64000) {   // the limit applies post-strip
        for (int i = 0; i < 100; i++) {
            patch += "[copy]\n input = _C" + std::to_string(n) +
                     "\n output = _C" + std::to_string(n + 1) + "\n";
            n++;
        }
    }
    patch += "[copy]\n input = _C" + std::to_string(n) + "\n output = O1\n";
    CompiledPatch cp;
    auto hard = compilePatch(patch, MasterType::Master16, cp);
    CHECK(!hard.ok);
    bool sizeErr = false;
    for (auto& e : hard.errors)
        if (e.message.find("maximum size") != std::string::npos) sizeErr = true;
    CHECK(sizeErr);

    LoadOptions opts;
    opts.ignoreMemoryLimits = true;
    auto soft = compilePatch(patch, MasterType::Master16, cp, opts);
    bool sizeWarn = false;
    for (auto& w : soft.warnings)
        if (w.find("maximum size") != std::string::npos) sizeWarn = true;
    CHECK(sizeWarn);
    for (auto& e : soft.errors)
        CHECK(e.message.find("maximum size") == std::string::npos);
}
