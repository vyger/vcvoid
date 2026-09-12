#include "harness.hpp"
#include "MasterDiagnostics.hpp"
#include "src/loader.hpp"

#include <string>

using namespace vcvoid::diag;

// The structured master-condition record behind GET /master/{id}/diagnostics
// and the panel error display (#46/#49). The load results below are produced by
// the REAL loader wherever a classification is asserted, so a reworded engine
// error message shows up here as a failing test rather than as a silently
// unclassified code in the bridge and on the panel.

static droid::LoadResult compile(const std::string& text) {
    droid::CompiledPatch cp;
    return droid::compilePatch(text, droid::MasterType::Master16, cp);
}

static Input runningPatch(const std::string& text, const char* path = "/tmp/p.ini") {
    Input in;
    in.patchPath = path;
    in.load = compile(text);
    in.engineRunning = in.load.ok;
    in.statusLine = "p.ini — ok, 100 bytes RAM";
    in.stateLine = "state: fresh";
    return in;
}

TEST(diag_no_patch) {
    Diagnostics d = diagnose(Input{});
    CHECK(d.state == State::NoPatch);
    CHECK(std::string(stateName(d.state)) == "no_patch");
    CHECK(d.severity == Severity::Info);
    CHECK(d.code.empty());
    CHECK(d.line == 0);
    CHECK(d.warnings.empty());
}

TEST(diag_running_clean) {
    Input in = runningPatch("[copy]\n input = I1\n output = O1\n");
    Diagnostics d = diagnose(in);
    CHECK(d.state == State::Running);
    CHECK(std::string(stateName(d.state)) == "running");
    CHECK(d.severity == Severity::Ok);
    CHECK(d.line == 0);
    CHECK(d.code.empty());
    CHECK(d.warnings.empty());
    CHECK(d.stateLine == "state: fresh");
    CHECK(d.patchPath == "/tmp/p.ini");
}

// patches/uat-err-register.ini: the square= line names O9, which no master has.
TEST(diag_load_failed_unknown_register_carries_the_line) {
    Input in;
    in.patchPath = "/tmp/uat-err-register.ini";
    in.load = compile("[lfo]\n    hz = 2\n    square = O9\n");
    CHECK(!in.load.ok);
    in.statusLine = "LOAD ERROR line 3: ...";
    Diagnostics d = diagnose(in);
    CHECK(d.state == State::LoadFailed);
    CHECK(d.severity == Severity::Error);
    CHECK(d.line == 3);                       // the offending line, not 0
    CHECK(d.code == "unknown_register");
    CHECK(d.codeColor == "yellow");
    CHECK(!d.message.empty());
}

TEST(diag_load_failed_unknown_circuit) {
    Input in;
    in.patchPath = "/tmp/p.ini";
    in.load = compile("[nosuchcircuit]\n    input = I1\n");
    CHECK(!in.load.ok);
    Diagnostics d = diagnose(in);
    CHECK(d.state == State::LoadFailed);
    CHECK(d.code == "unknown_circuit");
    CHECK(d.codeColor == "red");
    CHECK(d.line == 1);
}

TEST(diag_load_failed_cable_misuse_is_green) {
    Input in;
    in.patchPath = "/tmp/p.ini";
    in.load = compile("[lfo]\n hz = 1\n square = _X\n"
                      "[lfo]\n hz = 2\n square = _X\n"
                      "[copy]\n input = _X\n output = O1\n");
    CHECK(!in.load.ok);
    Diagnostics d = diagnose(in);
    CHECK(d.state == State::LoadFailed);
    CHECK(d.code == "cable_misuse");
    CHECK(d.codeColor == "green");
}

TEST(diag_load_failed_unknown_parameter_is_orange) {
    Input in;
    in.patchPath = "/tmp/p.ini";
    in.load = compile("[copy]\n    nosuchparam = 1\n    output = O1\n");
    CHECK(!in.load.ok);
    Diagnostics d = diagnose(in);
    CHECK(d.code == "unknown_parameter");
    CHECK(d.codeColor == "orange");
}

TEST(diag_load_failed_syntax_is_magenta) {
    Input in;
    in.patchPath = "/tmp/p.ini";
    in.load = compile("this is not a patch\n");
    CHECK(!in.load.ok);
    Diagnostics d = diagnose(in);
    CHECK(d.code == "invalid_syntax");
    CHECK(d.codeColor == "magenta");
}

// Issue #41: the size gate is a GLOBAL error (all LEDs, hardware blue) — it has
// no line to encode, and the message names the measured deployed size.
TEST(diag_oversize_patch_is_a_global_blue_code_with_no_line) {
    // Grow until the ABBREVIATED (deployed) size passes the gate — the size the
    // master actually measures (#41), not the verbose text length.
    std::string text = "[copy]\n    input = I1\n    output = O1\n";
    while (droid::deployedPatchSize(text) <= 64000)
        text += "[copy]\n    input = I2\n    output = _C" +
                std::to_string(text.size()) + "\n";
    Input in;
    in.patchPath = "/tmp/big.ini";
    in.load = compile(text);
    CHECK(!in.load.ok);
    Diagnostics d = diagnose(in);
    CHECK(d.state == State::LoadFailed);
    CHECK(d.code == "patch_too_big");
    CHECK(d.codeColor == "blue");
    CHECK(d.line == 0);
    CHECK(d.message.find("64000") != std::string::npos);
}

TEST(diag_patch_not_found) {
    Input in;
    in.patchPath = "/nope/droid.ini";
    in.statusLine = "cannot open /nope/droid.ini";
    Diagnostics d = diagnose(in);
    CHECK(d.state == State::LoadFailed);
    CHECK(d.code == "patch_not_found");
    CHECK(d.codeColor == "yellow");
    CHECK(d.line == 0);
}

// A deprecated circuit loads and warns — the one warning class a default master
// can reach (the memory-limit downgrades need the experimental opt-in).
TEST(diag_warnings_state_from_a_deprecated_circuit) {
    Input in = runningPatch("[p2b8]\n[togglebutton]\n    button = B1.1\n    led = L1.1\n");
    CHECK(in.load.ok);
    CHECK(!in.load.warnings.empty());
    Diagnostics d = diagnose(in);
    CHECK(d.state == State::Warnings);
    CHECK(d.severity == Severity::Warning);
    CHECK(d.warnings.size() == 1);
    CHECK(d.message.find("deprecated") != std::string::npos);
    CHECK(d.title == "Patch loaded with 1 warning");
}

TEST(diag_midi_warning_counts_as_a_warning) {
    Input in = runningPatch("[copy]\n input = I1\n output = O1\n");
    in.midiWarning = true;
    Diagnostics d = diagnose(in);
    CHECK(d.state == State::Warnings);
    CHECK(d.warnings.size() == 1);
    CHECK(d.warnings[0].find("MIDI") != std::string::npos);
}

TEST(diag_chain_error_beats_warnings) {
    Input in = runningPatch("[p2b8]\n[togglebutton]\n    button = B1.1\n    led = L1.1\n");
    in.chainError = "expected p2b8 at position 1, found m4";
    Diagnostics d = diagnose(in);
    CHECK(d.state == State::ChainError);
    CHECK(d.severity == Severity::Error);
    CHECK(d.message == in.chainError);
    // the warnings are still reported, they are just not what the state is about
    CHECK(d.warnings.size() == 1);
}

TEST(diag_load_failure_beats_chain_error) {
    Input in;
    in.patchPath = "/tmp/p.ini";
    in.load = compile("[lfo]\n    hz = 2\n    square = O9\n");
    in.chainError = "expected p2b8 at position 1, found m4";
    Diagnostics d = diagnose(in);
    CHECK(d.state == State::LoadFailed);
}

TEST(diag_severity_names) {
    CHECK(std::string(severityName(Severity::Ok)) == "ok");
    CHECK(std::string(severityName(Severity::Info)) == "info");
    CHECK(std::string(severityName(Severity::Warning)) == "warning");
    CHECK(std::string(severityName(Severity::Error)) == "error");
}
