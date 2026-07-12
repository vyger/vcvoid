#include "harness.hpp"
#include "src/loader.hpp"
#include "src/controllers.hpp"
using namespace droid;

static LoadResult compile(const std::string& text, CompiledPatch& cp) {
    return compilePatch(text, MasterType::Master16, cp);
}
static bool hasError(const LoadResult& r, const std::string& sub) {
    for (auto& e : r.errors) if (e.message.find(sub) != std::string::npos) return true;
    return false;
}

TEST(loader_happy_path) {
    CompiledPatch cp;
    auto r = compile("[copy]\n input = I1 * 0.5\n output = _X\n"
                     "[copy]\n input = _X\n output = O1\n", cp);
    CHECK(r.ok);
    CHECK(cp.circuits.size() == 2);
    CHECK(cp.cableNames.size() == 1 && cp.cableNames[0] == "_X");
    CHECK(std::string(cp.circuits[0].def->name) == "copy");
    CHECK(cp.circuits[0].params.size() == 2);
}

TEST(loader_controllers) {
    CompiledPatch cp;
    auto r = compile("[p2b8]\n[p2b8]\n[copy]\n input = P2.1\n output = O1\n", cp);
    CHECK(r.ok);
    CHECK(cp.controllers.size() == 2);
    CompiledPatch cp2;   // P3.1 but only 2 controllers declared
    auto r2 = compile("[p2b8]\n[p2b8]\n[copy]\n input = P3.1\n output = O1\n", cp2);
    CHECK(!r2.ok && hasError(r2, "controller"));
}

// Descriptor spot checks against manual/hardware.md §6.4–6.12 (blue-7).
TEST(controller_descriptors) {
    auto has = [](const char* name, char t, unsigned n) {
        const ControllerModel* m = findControllerModel(name);
        return m && controllerHasElement(*m, t, n);
    };
    // p2b8 §6.4: P1.1–P1.2, B1.1–B1.8, L1.1–L1.8, no switches/encoders.
    CHECK(has("p2b8", 'P', 2) && !has("p2b8", 'P', 3));
    CHECK(has("p2b8", 'B', 8) && !has("p2b8", 'B', 9));
    CHECK(has("p2b8", 'L', 8) && !has("p2b8", 'S', 1) && !has("p2b8", 'E', 1));
    // p4b2 §6.5: 4 pots, 2 buttons/LEDs.
    CHECK(has("p4b2", 'P', 4) && !has("p4b2", 'P', 5));
    CHECK(has("p4b2", 'B', 2) && !has("p4b2", 'B', 3));
    // p10 §6.6: 10 pots (incl. the 2 big + 8 small), nothing else.
    CHECK(has("p10", 'P', 10) && !has("p10", 'P', 11) && !has("p10", 'B', 1));
    // s10 §6.7: 10 switches, no pots.
    CHECK(has("s10", 'S', 10) && !has("s10", 'S', 11) && !has("s10", 'P', 1));
    // p8s8 §6.8: 8 pots (sliders), 8 LEDs, 8 switches, no B.
    CHECK(has("p8s8", 'P', 8) && has("p8s8", 'L', 8) && has("p8s8", 'S', 8));
    CHECK(!has("p8s8", 'P', 9) && !has("p8s8", 'B', 1));
    // b32 §6.9: 32 buttons + 32 LEDs, no pots.
    CHECK(has("b32", 'B', 32) && has("b32", 'L', 32) && !has("b32", 'B', 33));
    CHECK(!has("b32", 'P', 1));
    // e4 §6.10: 4 encoders, 4 buttons, 4 ring LEDs.
    CHECK(has("e4", 'E', 4) && has("e4", 'B', 4) && has("e4", 'L', 4));
    CHECK(!has("e4", 'E', 5) && !has("e4", 'P', 1));
    // m4 §6.11: 4 touch-plate buttons + 4 LEDs; no pots/switches/encoders.
    CHECK(has("m4", 'B', 4) && has("m4", 'L', 4) && !has("m4", 'B', 5));
    CHECK(!has("m4", 'P', 1) && !has("m4", 'E', 1));
    // db8e §6.12: 9 buttons (B1.1–8 face + B1.9 encoder push), 1 encoder,
    // 9 LEDs (L1.1–8 face-button LEDs + L1.9 encoder ring). Forge parity:
    // moduledb8e.cpp numRegisters(BUTTON)=9, (LED)=9, (ENCODER)=1.
    CHECK(has("db8e", 'B', 9) && has("db8e", 'E', 1) && has("db8e", 'L', 9));
    CHECK(has("db8e", 'B', 8) && has("db8e", 'L', 8));
    CHECK(!has("db8e", 'B', 10) && !has("db8e", 'E', 2) && !has("db8e", 'P', 1) && !has("db8e", 'L', 10));
    // x7 carries no controls and is not a positioned controller model.
    CHECK(findControllerModel("x7") == nullptr);
}

// Per-model element validation in the loader.
TEST(loader_controller_element_ranges) {
    CompiledPatch cp;
    // Valid element on the right model.
    CHECK(compile("[p2b8]\n[copy]\n input = P1.1\n output = O1\n", cp).ok);
    CHECK(compile("[b32]\n[copy]\n input = B1.32\n output = O1\n", cp).ok);
    CHECK(compile("[s10]\n[copy]\n input = S1.10\n output = O1\n", cp).ok);
    CHECK(compile("[p10]\n[copy]\n input = P1.10\n output = O1\n", cp).ok);
    // Element number out of range for the model.
    CHECK(hasError(compile("[p2b8]\n[copy]\n input = P1.3\n output = O1\n", cp),
                   "no register P1.3 on the p2b8"));
    CHECK(hasError(compile("[p2b8]\n[copy]\n input = B1.9\n output = O1\n", cp),
                   "no register B1.9 on the p2b8"));
    // Register kind that does not exist on the model.
    CHECK(hasError(compile("[b32]\n[copy]\n input = P1.1\n output = O1\n", cp),
                   "no register P1.1 on the b32"));
    CHECK(hasError(compile("[s10]\n[copy]\n input = P1.1\n output = O1\n", cp),
                   "no register P1.1 on the s10"));
    // Chain-position arithmetic across mixed models: P10 at position 2 has
    // P2.10, but the p2b8 at position 1 does not have P1.10.
    CHECK(compile("[p2b8]\n[p10]\n[copy]\n input = P2.10\n output = O1\n", cp).ok);
    CHECK(hasError(compile("[p2b8]\n[p10]\n[copy]\n input = P1.10\n output = O1\n", cp),
                   "no register P1.10 on the p2b8"));
    // x7 is excluded from numbering: it does not shift controller positions.
    CHECK(compile("[x7]\n[p2b8]\n[copy]\n input = P1.1\n output = O1\n", cp).ok);
    CHECK(hasError(compile("[x7]\n[p2b8]\n[copy]\n input = P2.1\n output = O1\n", cp),
                   "only 1 controllers are declared"));
}

TEST(loader_errors) {
    CompiledPatch cp;
    CHECK(hasError(compile("[flobbulator]\n", cp), "Unknown circuit"));
    CHECK(hasError(compile("[copy]\n inputt = I1\n output = O1\n", cp), "no parameter"));
    CHECK(hasError(compile("[copy]\n input = I1\n output = 0.5\n", cp), "single output register"));
    CHECK(hasError(compile("[copy]\n input = I1\n output = B1.1\n", cp), "cannot be used as an output"));
    CHECK(hasError(compile("[copy]\n input = I1\n output = O3\n"
                           "[copy]\n input = I2\n output = O3\n", cp), "Duplicate usage of O3"));
    CHECK(hasError(compile("[copy]\n input = O5\n output = O1\n", cp), "just used as an input"));
    CHECK(hasError(compile("[copy]\n input = _X\n output = O1\n", cp), "never used as an output"));
    CHECK(hasError(compile("[copy]\n input = I1\n output = _X\n", cp), "never used as an input"));
    CHECK(hasError(compile("[copy]\n input = I1\n output = O9\n", cp), "no register"));
    CHECK(hasError(compile("[copy]\n input = G9\n output = O1\n", cp) , "") == false); // G9 = X7 gate, valid
}

// Encoders (E) are input-only: writing to one is rejected, matching the Forge
// ("You cannot use an encoder as output"). Loader rule 8 previously omitted E.
TEST(loader_encoder_as_output_rejected) {
    CompiledPatch cp;
    CHECK(hasError(compile("[e4]\n[copy]\n input = I1\n output = E1.1\n", cp),
                   "E1.1 cannot be used as an output"));
    // I/P/B/S remain rejected as before.
    CHECK(hasError(compile("[p2b8]\n[copy]\n input = I1\n output = B1.1\n", cp),
                   "cannot be used as an output"));
}

// Dotted num==0 (`P1.0`) is rejected (no register is 0-indexed); the Forge flags
// "The number of the register may not be less than 1". Previously a dotted
// zero slipped past parseRegisterName's plain-form-only guard.
TEST(loader_dotted_zero_element_rejected) {
    CompiledPatch cp;
    CHECK(hasError(compile("[p2b8]\n[copy]\n input = P1.0\n output = O1\n", cp),
                   "invalid value 'P1.0'"));
    CHECK(!parseRegisterName("P1.0").has_value());
    CHECK(!parseRegisterName("G1.0").has_value());
    CHECK(!parseRegisterName("P0").has_value());
}

// Section names are not trimmed inside the brackets. The Forge's parser regex is
// [a-zA-Z0-9]+ and rejects `[ lfo ]` outright; we keep the untrimmed name so it
// fails circuit resolution. Both REJECT — that is the parity that matters (we do
// NOT silently accept a trimmed name).
TEST(loader_section_name_with_spaces_rejected) {
    CompiledPatch cp;
    CHECK(!compile("[ lfo ]\n hz = 5\n", cp).ok);
    CHECK(!compile("[lfo ]\n hz = 5\n", cp).ok);
}

// Rule 2: x7 never occupies a controller-numbering position (matches hardware).
TEST(loader_rule2_x7_not_a_controller) {
    CompiledPatch cp;
    CHECK(compile("[x7]\n[p2b8]\n[copy]\n input = P1.1\n output = O1\n", cp).ok);
    CHECK(hasError(compile("[x7]\n[copy]\n input = P1.1\n output = O1\n", cp),
                   "only 0 controllers are declared"));
}

// Rule 11: an internal cable written by two outputs is a duplicate-output error.
TEST(loader_rule11_cable_written_twice) {
    CompiledPatch cp;
    CHECK(hasError(compile("[copy]\n input = I1\n output = _X\n"
                           "[copy]\n input = I2\n output = _X\n"
                           "[copy]\n input = _X\n output = O1\n", cp),
                   "Duplicate usage of patch cable _X as output"));
}

// F0b: the duplicate-cable error must report the line of the second (offending)
// write, not line 0.
TEST(loader_rule11_duplicate_cable_reports_second_write_line) {
    CompiledPatch cp;
    auto r = compile("[copy]\n input = I1\n output = _X\n"      // line 1-3
                     "[copy]\n input = I2\n output = _X\n"      // line 4-6: second write at line 6
                     "[copy]\n input = _X\n output = O1\n", cp);
    CHECK(!r.ok);
    bool found = false;
    for (auto& e : r.errors) {
        if (e.message.find("Duplicate usage of patch cable _X as output") != std::string::npos) {
            found = true;
            CHECK(e.line == 6);
        }
    }
    CHECK(found);
}

TEST(loader_last_assignment_wins) {
    CompiledPatch cp;
    auto r = compile("[copy]\n input = I1\n input = I2\n output = O1\n", cp);
    CHECK(r.ok);
    CHECK(cp.circuits[0].params.size() == 2);            // one input, one output
    const CompiledParam* in = nullptr;
    for (auto& p : cp.circuits[0].params) if (p.def->isInput) in = &p;
    CHECK(in && in->a.reg.num == 2);                     // I2 won
}

TEST(loader_deprecated_warns_but_loads) {
    CompiledPatch cp;
    auto r = compile("[togglebutton]\n button = B1.1\n output = O1\n[p2b8]\n", cp);
    // note: [p2b8] must come first for B1.1 to be valid — fix ordering:
    CompiledPatch cp2;
    auto r2 = compile("[p2b8]\n[togglebutton]\n button = B1.1\n output = O1\n", cp2);
    CHECK(r2.ok);
    bool warned = false;
    for (auto& w : r2.warnings) if (w.find("deprecated") != std::string::npos) warned = true;
    CHECK(warned);
    (void)r; (void)cp;
}

static LoadResult compile18(const std::string& text, CompiledPatch& cp) {
    return compilePatch(text, MasterType::Master18, cp);
}

// Master18 register set (Forge parity: droidforge/patch/patch.cpp updateProblems
// + Patch::registerAvailable): I1-I2 only, no N, native G1-G4 + X7 G9-G12,
// R5-R16 and X1 absent.
TEST(loader_master18_registers) {
    CompiledPatch cp;
    // valid: gates in, CV outs, native gates, X7 gates, G8 expanders, R1-R4
    CHECK(compile18("[copy]\n input = I2\n output = O8\n", cp).ok);
    CHECK(compile18("[lfo]\n square = G4\n", cp).ok);
    CHECK(compile18("[lfo]\n square = G12\n", cp).ok);          // X7
    CHECK(compile18("[lfo]\n square = G4.8\n", cp).ok);         // 4th G8
    CHECK(compile18("[lfo]\n square = R4\n", cp).ok);           // diag LED
    CHECK(compile18("[lfo]\n square = R17\n", cp).ok);          // virtual R
    // invalid on Master18
    CHECK(hasError(compile18("[copy]\n input = I3\n output = O1\n", cp), "input"));
    CHECK(hasError(compile18("[copy]\n input = I1\n output = N1\n", cp), "normalization"));
    CHECK(hasError(compile18("[lfo]\n square = G5\n", cp), "gate"));
    CHECK(hasError(compile18("[lfo]\n square = R5\n", cp), "register"));
    CHECK(hasError(compile18("[lfo]\n square = R16\n", cp), "register"));
    CHECK(hasError(compile18("[copy]\n input = X1\n output = O1\n", cp), "register"));
    // still invalid on Master16 (regression: the check now runs for both masters)
    CompiledPatch cp16;
    CHECK(hasError(compile("[copy]\n input = I9\n output = O1\n", cp16), "register"));
}

// vcotuner/sinfonionlink are MASTER18-only (Forge: circuitNeedsMaster18).
TEST(loader_master18_only_circuits) {
    CompiledPatch cp;
    CHECK(hasError(compile("[vcotuner]\n hz = O1\n", cp), "MASTER18"));
    CHECK(hasError(compile("[sinfonionlink]\n", cp), "MASTER18"));
    CHECK(compile18("[vcotuner]\n hz = O1\n", cp).ok);
}

TEST(loader_size_limit) {
    std::string big = "[copy]\n input = I1\n output = O1\n";
    std::string comments(100000, '#');                    // comments don't count
    CompiledPatch cp;
    CHECK(compile(big + "#" + comments, cp).ok);
    std::string huge;                                     // real content does count
    for (int i = 0; i < 4000; i++) huge += "[copy]\ninput=I1\noutput=_X\n";
    CHECK(hasError(compile(huge, cp), "maximum size"));
}
