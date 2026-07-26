#include "harness.hpp"
#include "src/engine.hpp"
#include "src/gatereader.hpp"
#include "src/loader.hpp"
using namespace droid;

// Experimental circuits (#12): circuits that exist only in vcvoid, declared in
// engine/experimental.json and refused at load unless the module's
// "Allow experimental circuits" option is on. See
// docs/adr/0001-experimental-circuits.md.

static const char* kTrigseqPatch =
    "[trigseq]\n"
    "    clock   = I1\n"
    "    pattern = \"x...x.x.\"\n"
    "    output  = O1\n";

TEST(experimental_circuit_refused_by_default) {
    CompiledPatch cp;
    auto r = compilePatch(kTrigseqPatch, MasterType::Master16, cp);
    CHECK(!r.ok);
    bool found = false;
    for (auto& e : r.errors)
        if (e.message.find("trigseq") != std::string::npos &&
            e.message.find("experimental") != std::string::npos) {
            found = true;
            CHECK(e.line == 1);                 // localized to the circuit header
            // The message must name the switch that enables it, so a user can
            // act on it without consulting the docs.
            CHECK(e.message.find("Allow experimental circuits") != std::string::npos);
        }
    CHECK(found);
}

TEST(experimental_circuit_allowed_with_option) {
    CompiledPatch cp;
    LoadOptions opts;
    opts.allowExperimental = true;
    auto r = compilePatch(kTrigseqPatch, MasterType::Master16, cp, opts);
    CHECK(r.ok);
    CHECK(r.errors.empty());
    CHECK(cp.circuits.size() == 1);
    CHECK(cp.circuits[0].def->experimental);
}

// The option is scoped to experimental circuits only: it must not soften any
// other load error (an unknown circuit stays unknown).
TEST(experimental_option_does_not_excuse_unknown_circuits) {
    CompiledPatch cp;
    LoadOptions opts;
    opts.allowExperimental = true;
    auto r = compilePatch("[nosuchcircuit]\n", MasterType::Master16, cp, opts);
    CHECK(!r.ok);
    bool unknown = false;
    for (auto& e : r.errors)
        if (e.message.find("Unknown circuit") != std::string::npos) unknown = true;
    CHECK(unknown);
}

// trigseq: the pattern's length IS the string's length, with no ceiling in the
// engine. This lives in a unit test rather than a golden on purpose: the patch
// FORMAT caps a text at 18 characters (DB8E_MAX_TEXT_LENGTH, from
// master:db8elink.h), which tools/crosscheck.sh enforces on every golden by
// running it through the Forge. The engine imposes no such limit, and this is
// the seam that can prove it. A 40-step pattern emitting only on steps 1 and 40
// pins both ends: a truncating implementation would fire early and never reach
// step 40.
TEST(trigseq_pattern_longer_than_the_text_format_allows) {
    std::string pattern(40, '.');
    pattern[0] = 'x';
    pattern[39] = 'x';
    droid::Engine e(MasterType::Master16, 6000.0f);
    LoadOptions opts;
    opts.allowExperimental = true;
    auto r = e.load("[trigseq]\n    clock = I1\n    pattern = \"" + pattern +
                    "\"\n    output = O1\n", opts);
    CHECK(r.ok);

    // Clock 40 steps: high tick then low tick per step, sampling O1 while high.
    int emitted = 0, firstEmit = 0, lastEmit = 0;
    for (int step = 1; step <= 40; step++) {
        e.setValue("I1", 1.0f); e.tick();
        if (e.getValue("O1") >= kGateHighThreshold) {
            emitted++;
            if (!firstEmit) firstEmit = step;
            lastEmit = step;
        }
        e.setValue("I1", 0.0f); e.tick();
    }
    CHECK(emitted == 2);
    CHECK(firstEmit == 1);
    CHECK(lastEmit == 40);
}

// Firmware circuits are never flagged experimental — the gate must be inert for
// every one of the 76 hardware circuits.
TEST(firmware_circuits_are_not_experimental) {
    unsigned experimental = 0;
    for (unsigned i = 0; i < gen::kNumCircuits; i++)
        if (gen::kCircuits[i].experimental) experimental++;
    CHECK(gen::kNumCircuits == 77);
    CHECK(experimental == 1);
    CHECK(gen::findCircuit("euklid") != nullptr);
    CHECK(!gen::findCircuit("euklid")->experimental);
    CHECK(gen::findCircuit("trigseq")->experimental);
}
