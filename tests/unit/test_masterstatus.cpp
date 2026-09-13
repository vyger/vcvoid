#include "harness.hpp"
#include "MasterStatus.hpp"
#include "src/loader.hpp"   // the wire-level tests at the bottom compile real patch text

// The master's visible error state (issue #46), model half. Three things are
// pinned here: the hardware LED blink codes, against the worked examples in
// manual/basics.md §5.3; the five-state verdict that drives the ring, the
// matrix, the tooltip and the context-menu card; and the words that same
// verdict is reported in over the UAT bridge (issue #49), end to end from the
// real loader's error tags.

using namespace vcvoid::status;
using droid::ErrorCode;

// Which LEDs are lit, as the manual counts them: 1..8 for the input row pair,
// 1..8 again for the output row pair.
static std::vector<int> litInputs(const BlinkCode& b) {
    std::vector<int> v;
    for (int i = 0; i < 8; i++)
        if (b.led[i].r != 0.f || b.led[i].g != 0.f || b.led[i].b != 0.f) v.push_back(i + 1);
    return v;
}
static std::vector<int> litOutputs(const BlinkCode& b) {
    std::vector<int> v;
    for (int i = 8; i < 16; i++)
        if (b.led[i].r != 0.f || b.led[i].g != 0.f || b.led[i].b != 0.f) v.push_back(i - 7);
    return v;
}
static bool sameColor(RGB a, RGB b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

// --- the digit encoding --------------------------------------------------
// "LED n indicates n; add them up. If a 9 is needed, then 8 + 1 will flash."

TEST(blink_digits_single_led_up_to_eight) {
    for (int d = 1; d <= 8; d++)
        CHECK(ledDigits(d) == (1u << (d - 1)));
    CHECK(ledDigits(0) == 0u);
}

TEST(blink_digits_nine_is_eight_plus_one) {
    CHECK(ledDigits(9) == ((1u << 7) | (1u << 0)));
}

TEST(blink_digits_hundreds_ten_is_two_plus_eight) {
    // manual §5.3: "if LED 2 and LED 8 flash white, ... just add them up",
    // i.e. 10 hundreds = line 1000.
    CHECK(ledDigits(10) == ((1u << 1) | (1u << 7)));
}

TEST(blink_digits_max_is_thirty_six) {
    CHECK(ledDigits(36) == 0xffu);   // 1+2+...+8, i.e. line 3600+
}

// --- the manual's worked examples ---------------------------------------

TEST(blink_line_90_lights_input_leds_one_and_eight) {
    // "If the error happens to be in line 90, then LED 1 + 8 will flash."
    // Line 90 is the manual's "Undefined parameter" example, hence orange.
    BlinkCode b = blinkCode(ErrorCode::UnknownParameter, 90);
    CHECK(b.active && !b.global);
    CHECK(litInputs(b) == std::vector<int>({1, 8}));
    CHECK(litOutputs(b).empty());            // ones digit 0: no output LED
    CHECK(sameColor(b.led[0], kOrange));
    CHECK(sameColor(b.led[7], kOrange));
}

TEST(blink_line_81_invalid_parameter_value_is_magenta) {
    BlinkCode b = blinkCode(ErrorCode::InvalidSyntax, 81);
    CHECK(litInputs(b) == std::vector<int>({8}));    // tens = 8
    CHECK(litOutputs(b) == std::vector<int>({1}));   // ones = 1
    CHECK(sameColor(b.led[7], kMagenta));
    CHECK(sameColor(b.led[8], kMagenta));
}

TEST(blink_line_99_invalid_register_is_yellow_one_and_eight_both_rows) {
    BlinkCode b = blinkCode(ErrorCode::UnknownRegister, 99);
    CHECK(litInputs(b) == std::vector<int>({1, 8}));
    CHECK(litOutputs(b) == std::vector<int>({1, 8}));
    CHECK(sameColor(b.led[0], kYellow));
    CHECK(sameColor(b.led[8], kYellow));
}

TEST(blink_line_144_line_too_long_uses_a_white_hundreds_led) {
    BlinkCode b = blinkCode(ErrorCode::LineTooLong, 144);
    CHECK(litInputs(b) == std::vector<int>({1, 4}));   // 1 white (hundreds) + 4 blue (tens)
    CHECK(litOutputs(b) == std::vector<int>({4}));     // ones = 4
    CHECK(sameColor(b.led[0], kWhite));
    CHECK(sameColor(b.led[3], kBlue));
    CHECK(sameColor(b.led[11], kBlue));
}

TEST(blink_lines_one_to_nine_light_no_input_led) {
    // "If it is in line 1 to 9, then no input LED flashes at all."
    BlinkCode b = blinkCode(ErrorCode::UnknownCircuit, 7);
    CHECK(litInputs(b).empty());
    CHECK(litOutputs(b) == std::vector<int>({7}));
    CHECK(sameColor(b.led[14], kRed));
}

TEST(blink_max_line_3699_lights_every_input_led_white) {
    BlinkCode b = blinkCode(ErrorCode::UnknownRegister, 3699);
    CHECK(b.active && !b.global);
    // hundreds = 36 -> all eight input LEDs white (they out-rank the tens)
    CHECK(litInputs(b) == std::vector<int>({1, 2, 3, 4, 5, 6, 7, 8}));
    for (int i = 0; i < 8; i++) CHECK(sameColor(b.led[i], kWhite));
    CHECK(litOutputs(b) == std::vector<int>({1, 8}));   // ones = 9
}

// --- global codes --------------------------------------------------------

TEST(blink_global_codes_flash_all_sixteen_leds) {
    struct { ErrorCode code; RGB color; } cases[] = {
        {ErrorCode::PatchNotFound,      kYellow},
        {ErrorCode::TooManyControllers, kRed},
        {ErrorCode::PatchTooBig,        kBlue},
        {ErrorCode::OutOfMemory,        kCyan},
        {ErrorCode::InvalidFirmware,    kMagenta},
        {ErrorCode::NoSdCard,           kWhite},
    };
    for (auto& c : cases) {
        BlinkCode b = blinkCode(c.code, 0);
        CHECK(b.active && b.global);
        for (int i = 0; i < 16; i++) CHECK(sameColor(b.led[i], c.color));
    }
}

TEST(blink_global_code_ignores_the_line_number) {
    // Out of memory is reported per circuit here (so it has a line), but on the
    // hardware it is a whole-patch error: all 16 LEDs cyan either way.
    BlinkCode b = blinkCode(ErrorCode::OutOfMemory, 42);
    CHECK(b.global);
    for (int i = 0; i < 16; i++) CHECK(sameColor(b.led[i], kCyan));
}

TEST(blink_unmapped_code_shows_nothing) {
    BlinkCode b = blinkCode(ErrorCode::Unmapped, 12);
    CHECK(!b.active);
}

TEST(blink_local_code_without_a_line_falls_back_to_the_global_form) {
    // "Output register O5 is just used as an input" is a whole-patch analysis
    // error with no line; it still has to show its reason colour.
    BlinkCode b = blinkCode(ErrorCode::CableMisuse, 0);
    CHECK(b.active && b.global);
    for (int i = 0; i < 16; i++) CHECK(sameColor(b.led[i], kGreen));
}

TEST(blink_line_past_the_codeable_range_falls_back_to_the_global_form) {
    BlinkCode b = blinkCode(ErrorCode::UnknownCircuit, kMaxBlinkLine + 1);
    CHECK(b.active && b.global);
    for (int i = 0; i < 16; i++) CHECK(sameColor(b.led[i], kRed));
}

// --- the five states -----------------------------------------------------

TEST(status_no_patch_is_a_grey_ring_and_a_dark_matrix) {
    Report r;
    Status s = evaluate(r);
    CHECK(s.state == State::NoPatch);
    CHECK(s.ringVisible && sameColor(s.ring, kRingGrey));
    CHECK(s.matrix == Matrix::Dark);       // deviation: the hardware flashes yellow
    CHECK(!s.blink.active);
    CHECK(s.title == "No patch loaded");
    CHECK(s.line == 0);
}

TEST(status_running_shows_nothing_at_all) {
    Report r;
    r.havePatch = true;
    r.loadOk = true;
    Status s = evaluate(r);
    CHECK(s.state == State::Running);
    CHECK(!s.ringVisible);
    CHECK(s.matrix == Matrix::Mirror);
    CHECK(s.title.empty() && s.message.empty());
    CHECK(oneLine(s).empty());
}

TEST(status_load_failure_is_a_red_ring_plus_the_blink_code) {
    Report r;
    r.havePatch = true;
    r.loadOk = false;
    r.errorCount = 1;
    r.errorLine = 99;
    r.errorCode = ErrorCode::UnknownRegister;
    r.errorMessage = "Unknown register 'O9'. Allowed is O1 ... O8";
    Status s = evaluate(r);
    CHECK(s.state == State::LoadFailed);
    CHECK(s.ringVisible && sameColor(s.ring, kRingRed));
    CHECK(s.matrix == Matrix::Blink);
    CHECK(s.blink.active && !s.blink.global);
    CHECK(s.title == "LOAD ERROR · line 99");
    CHECK(s.message == "Unknown register 'O9'. Allowed is O1 ... O8");
    CHECK(s.line == 99);
    CHECK(oneLine(s) == "LOAD ERROR · line 99 — Unknown register 'O9'. Allowed is O1 ... O8");
}

TEST(status_load_failure_counts_the_errors_it_does_not_show) {
    Report r;
    r.havePatch = true;
    r.errorCount = 3;
    r.errorLine = 12;
    r.errorCode = ErrorCode::UnknownCircuit;
    r.errorMessage = "Unknown circuit 'lfoo'";
    Status s = evaluate(r);
    CHECK(s.message == "Unknown circuit 'lfoo' (+2 more)");
}

TEST(status_load_failure_without_a_line_drops_the_line_from_the_title) {
    Report r;
    r.havePatch = true;
    r.errorCount = 1;
    r.errorCode = ErrorCode::PatchTooBig;
    r.errorMessage = "patch exceeds the maximum size of 64000 bytes";
    Status s = evaluate(r);
    CHECK(s.title == "LOAD ERROR");
    CHECK(s.line == 0);
    CHECK(s.blink.global);
}

TEST(status_unreadable_file_is_the_hardware_patch_not_found_code) {
    Report r;
    r.havePatch = true;
    r.fileUnreadable = true;
    r.errorMessage = "cannot open /tmp/gone.ini";
    Status s = evaluate(r);
    CHECK(s.state == State::LoadFailed);
    CHECK(s.matrix == Matrix::Blink);
    CHECK(s.blink.global);
    for (int i = 0; i < 16; i++) CHECK(sameColor(s.blink.led[i], kYellow));
    CHECK(std::string(codeNames(s.code).code) == "patch_not_found");
    CHECK(std::string(codeNames(s.code).color) == "yellow");
}

TEST(status_unmapped_error_still_rings_red_but_leaves_the_matrix_dark) {
    Report r;
    r.havePatch = true;
    r.errorCount = 1;
    r.errorCode = ErrorCode::Unmapped;
    r.errorMessage = "something new";
    Status s = evaluate(r);
    CHECK(s.state == State::LoadFailed);
    CHECK(sameColor(s.ring, kRingRed));
    CHECK(s.matrix == Matrix::Dark);
    CHECK(!s.blink.active);
}

TEST(status_warnings_are_amber_and_keep_running) {
    Report r;
    r.havePatch = true;
    r.loadOk = true;
    r.warningCount = 2;
    r.warningMessage = "circuit 'copy' is deprecated";
    Status s = evaluate(r);
    CHECK(s.state == State::Warnings);
    CHECK(s.ringVisible && sameColor(s.ring, kRingAmber));
    CHECK(s.matrix == Matrix::Mirror);     // it runs; the LEDs mirror the jacks
    CHECK(!s.blink.active);
    CHECK(s.title == "Running with 2 warnings");
    CHECK(s.message == "circuit 'copy' is deprecated (+1 more)");
}

TEST(status_one_warning_is_singular) {
    Report r;
    r.havePatch = true;
    r.loadOk = true;
    r.warningCount = 1;
    r.warningMessage = "circuit 'copy' is deprecated";
    Status s = evaluate(r);
    CHECK(s.title == "Running with 1 warning");
    CHECK(s.message == "circuit 'copy' is deprecated");
}

TEST(status_chain_error_is_red_with_the_mirror_left_alone) {
    Report r;
    r.havePatch = true;
    r.loadOk = true;
    r.chainError = "controller 2: patch declares m4, chain has nothing";
    Status s = evaluate(r);
    CHECK(s.state == State::ChainError);
    CHECK(s.ringVisible && sameColor(s.ring, kRingRed));
    CHECK(s.matrix == Matrix::Mirror);     // paused: the mirror freezes by itself
    CHECK(!s.blink.active);
    CHECK(s.title == "CHAIN ERROR");
    CHECK(s.message == "controller 2: patch declares m4, chain has nothing");
    CHECK(s.line == 0);                    // nothing to open an editor at
}

TEST(status_a_failed_load_outranks_a_chain_error) {
    Report r;
    r.havePatch = true;
    r.loadOk = false;
    r.errorCount = 1;
    r.errorCode = ErrorCode::UnknownCircuit;
    r.errorMessage = "Unknown circuit 'lfoo'";
    r.errorLine = 4;
    r.chainError = "controller 1: patch declares p2b8, chain has nothing";
    Status s = evaluate(r);
    CHECK(s.state == State::LoadFailed);
}

TEST(status_a_chain_error_outranks_warnings) {
    Report r;
    r.havePatch = true;
    r.loadOk = true;
    r.warningCount = 1;
    r.warningMessage = "circuit 'copy' is deprecated";
    r.chainError = "controller 1: patch declares p2b8, chain has nothing";
    Status s = evaluate(r);
    CHECK(s.state == State::ChainError);
}

// --- fitting the words into the window (issue #46 review) ----------------
// Rack sizes a tooltip and a menu to their widest line, so the card's sentences
// are wrapped in the model. These pin the wrap rules the widget relies on.

TEST(wrap_short_text_is_unchanged) {
    CHECK(wrapText("LOAD ERROR") == "LOAD ERROR");
    CHECK(wrapLines("LOAD ERROR").size() == 1);
    CHECK(wrapText("") == "");
    // Exactly the width still fits on one line.
    std::string exact(kWrapWidth, 'x');
    CHECK(wrapText(exact) == exact);
}

TEST(wrap_breaks_at_spaces_never_mid_word) {
    std::string msg = "Circuit 'trigseq' is experimental (vcvoid only, not "
                      "available on DROID hardware). Enable \"Allow experimental "
                      "circuits\" in the module's context menu to load this patch.";
    std::vector<std::string> lines = wrapLines(msg);
    CHECK(lines.size() > 2);
    std::string rejoined;
    for (size_t i = 0; i < lines.size(); i++) {
        CHECK(lines[i].size() <= kWrapWidth);
        CHECK(lines[i].front() != ' ' && lines[i].back() != ' ');
        rejoined += (i ? " " : "") + lines[i];
    }
    CHECK(rejoined == msg);   // only spaces became breaks; no word was cut
}

TEST(wrap_keeps_a_long_token_whole_when_it_fits) {
    // A 40-character cable name is longer than any word, but shorter than the
    // column: it must not be split just because the line before it is full.
    std::string token(40, 'A');
    std::vector<std::string> lines = wrapLines("cable " + token + " is unused");
    CHECK(lines.size() == 1);
    lines = wrapLines("this message is padded out so the long name lands second "
                      + token);
    CHECK(lines.size() == 2);
    CHECK(lines[1] == token);
}

TEST(wrap_splits_a_token_longer_than_the_column) {
    std::string token(kWrapWidth + 10, 'A');
    std::vector<std::string> lines = wrapLines(token);
    CHECK(lines.size() == 2);
    CHECK(lines[0].size() == kWrapWidth);
    CHECK(lines[1].size() == 10);
}

TEST(wrap_respects_existing_newlines) {
    std::vector<std::string> lines = wrapLines("one\ntwo\n\nthree");
    CHECK(lines == std::vector<std::string>({"one", "two", "", "three"}));
    CHECK(wrapText("one\ntwo") == "one\ntwo");
}

TEST(wrap_trims_trailing_and_repeated_spaces) {
    CHECK(wrapText("  padded message   ") == "padded message");
    CHECK(wrapText("collapsed    run") == "collapsed run");
    CHECK(wrapText("trailing \nspace ") == "trailing\nspace");
}

TEST(elide_middle_keeps_both_ends_of_a_name) {
    CHECK(elideMiddle("droid.ini", 32) == "droid.ini");
    std::string name = "a-very-long-patch-file-name-that-will-not-fit.ini";
    std::string cut = elideMiddle(name, 32);
    CHECK(cut.size() == 32);
    CHECK(cut.substr(0, 8) == name.substr(0, 8));
    CHECK(cut.substr(cut.size() - 4) == ".ini");
    CHECK(cut.find("...") != std::string::npos);
}

// The tooltip's one-liner is what publishStatus() wraps, so a long message has
// to actually survive the round trip as several lines.
TEST(wrap_a_long_status_line_becomes_several_tooltip_lines) {
    Report r;
    r.havePatch = true;
    r.loadOk = false;
    r.errorCount = 1;
    r.errorLine = 15;
    r.errorCode = ErrorCode::UnknownCircuit;
    r.errorMessage = "Circuit 'trigseq' is experimental (vcvoid only, not "
                     "available on DROID hardware). Enable \"Allow experimental "
                     "circuits\" in the module's context menu to load this patch.";
    std::string tip = wrapText(oneLine(evaluate(r)));
    CHECK(tip.find('\n') != std::string::npos);
    size_t start = 0;
    while (start <= tip.size()) {
        size_t nl = tip.find('\n', start);
        size_t len = (nl == std::string::npos ? tip.size() : nl) - start;
        CHECK(len <= kWrapWidth);
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
}

// --- the words the model reports (GET /master/{id}/diagnostics, issue #49) ---
// The UAT bridge serialises this model rather than deriving a second verdict of
// its own, so the vocabulary it promises is pinned here.

TEST(status_severity_follows_the_state) {
    CHECK(severityFor(State::NoPatch) == Severity::Info);      // not an error
    CHECK(severityFor(State::LoadFailed) == Severity::Error);
    CHECK(severityFor(State::ChainError) == Severity::Error);
    CHECK(severityFor(State::Warnings) == Severity::Warning);
    CHECK(severityFor(State::Running) == Severity::Ok);
    CHECK(std::string(severityName(Severity::Ok)) == "ok");
    CHECK(std::string(severityName(Severity::Info)) == "info");
    CHECK(std::string(severityName(Severity::Warning)) == "warning");
    CHECK(std::string(severityName(Severity::Error)) == "error");
}

// codeNames() and errorColor() are the same manual table written twice — once
// in words, once in RGB. Walk every code and hold them equal, so a colour
// changed in one place cannot silently disagree with the other.
TEST(status_code_names_match_the_colour_table) {
    const ErrorCode all[] = {
        ErrorCode::Unmapped, ErrorCode::PatchNotFound, ErrorCode::TooManyControllers,
        ErrorCode::PatchTooBig, ErrorCode::OutOfMemory, ErrorCode::InvalidFirmware,
        ErrorCode::NoSdCard, ErrorCode::UnknownRegister, ErrorCode::UnknownParameter,
        ErrorCode::UnknownCircuit, ErrorCode::LineTooLong, ErrorCode::CableMisuse,
        ErrorCode::InvalidSyntax,
    };
    for (ErrorCode c : all) {
        CodeNames n = codeNames(c);
        RGB rgb;
        bool global = false;
        bool hasColor = errorColor(c, rgb, global);
        // A code has a name exactly when it has a hardware colour.
        CHECK(hasColor == (std::string(n.code) != ""));
        CHECK(hasColor == (std::string(n.color) != ""));
        if (!hasColor) continue;
        RGB named;
        CHECK(colorByName(n.color, named));
        CHECK(sameColor(named, rgb));
    }
    CHECK(std::string(codeNames(ErrorCode::Unmapped).code).empty());
}

// A state other than LoadFailed carries no code at all — nothing is wrong with
// the patch, so there is nothing for the matrix to spell.
TEST(status_only_a_failed_load_carries_an_error_code) {
    Report r;
    r.havePatch = true;
    r.loadOk = true;
    CHECK(evaluate(r).code == ErrorCode::Unmapped);
    r.warningCount = 1;
    r.warningMessage = "circuit 'copy' is deprecated";
    CHECK(evaluate(r).code == ErrorCode::Unmapped);
    r.chainError = "expected p2b8 at position 1, found m4";
    CHECK(evaluate(r).code == ErrorCode::Unmapped);
}

// --- end to end, through the REAL loader ----------------------------------
// Everything above feeds evaluate() a hand-written Report. These compile actual
// patch text instead, so the chain the panel and the bridge really walk — the
// loader's ErrorCode tag -> evaluate() -> the reported name and colour — is
// pinned end to end. A load error that stops being tagged, or is tagged with
// the wrong code, fails here rather than blinking the wrong colour on a panel.

// The same mapping DroidMasterBase::statusReport() does, minus the Rack fields.
static Report reportFor(const std::string& text) {
    droid::CompiledPatch cp;
    droid::LoadResult res = droid::compilePatch(text, droid::MasterType::Master16, cp);
    Report r;
    r.havePatch = true;
    r.loadOk = res.ok;
    r.errorCount = (int) res.errors.size();
    if (!res.errors.empty()) {
        r.errorLine = res.errors[0].line;
        r.errorCode = res.errors[0].code;
        r.errorMessage = res.errors[0].message;
    }
    r.warningCount = (int) res.warnings.size();
    if (!res.warnings.empty()) r.warningMessage = res.warnings[0];
    return r;
}

// patches/uat-err-register.ini's `square = O9` names a register no master has.
TEST(status_wire_unknown_register_is_yellow_and_carries_the_line) {
    Status s = evaluate(reportFor("[lfo]\n    hz = 2\n    square = O9\n"));
    CHECK(s.state == State::LoadFailed);
    CHECK(severityFor(s.state) == Severity::Error);
    CHECK(s.line == 3);                     // the offending line, not 0
    CHECK(std::string(codeNames(s.code).code) == "unknown_register");
    CHECK(std::string(codeNames(s.code).color) == "yellow");
    CHECK(!s.message.empty());
}

TEST(status_wire_unknown_circuit_is_red) {
    Status s = evaluate(reportFor("[nosuchcircuit]\n    input = I1\n"));
    CHECK(s.state == State::LoadFailed);
    CHECK(std::string(codeNames(s.code).code) == "unknown_circuit");
    CHECK(std::string(codeNames(s.code).color) == "red");
    CHECK(s.line == 1);
}

TEST(status_wire_cable_misuse_is_green) {
    Status s = evaluate(reportFor("[lfo]\n hz = 1\n square = _X\n"
                                  "[lfo]\n hz = 2\n square = _X\n"
                                  "[copy]\n input = _X\n output = O1\n"));
    CHECK(s.state == State::LoadFailed);
    CHECK(std::string(codeNames(s.code).code) == "cable_misuse");
    CHECK(std::string(codeNames(s.code).color) == "green");
}

TEST(status_wire_unknown_parameter_is_orange) {
    Status s = evaluate(reportFor("[copy]\n    nosuchparam = 1\n    output = O1\n"));
    CHECK(s.state == State::LoadFailed);
    CHECK(std::string(codeNames(s.code).code) == "unknown_parameter");
    CHECK(std::string(codeNames(s.code).color) == "orange");
}

TEST(status_wire_syntax_error_is_magenta) {
    Status s = evaluate(reportFor("this is not a patch\n"));
    CHECK(s.state == State::LoadFailed);
    CHECK(std::string(codeNames(s.code).code) == "invalid_syntax");
    CHECK(std::string(codeNames(s.code).color) == "magenta");
}

// Issue #41: the size gate is a GLOBAL error — hardware blue, all 16 LEDs, no
// line to encode — and the message names the measured DEPLOYED size.
TEST(status_wire_oversize_patch_is_a_global_blue_code_with_no_line) {
    // Grow until the ABBREVIATED (deployed) size passes the gate, which is the
    // size the master actually measures, not the verbose text length.
    std::string text = "[copy]\n    input = I1\n    output = O1\n";
    while (droid::deployedPatchSize(text) <= 64000)
        text += "[copy]\n    input = I2\n    output = _C" +
                std::to_string(text.size()) + "\n";
    Status s = evaluate(reportFor(text));
    CHECK(s.state == State::LoadFailed);
    CHECK(std::string(codeNames(s.code).code) == "patch_too_big");
    CHECK(std::string(codeNames(s.code).color) == "blue");
    CHECK(s.line == 0);
    CHECK(s.blink.global);
    CHECK(s.message.find("64000") != std::string::npos);
}

// A deprecated circuit is the one warning class a default master can reach (the
// memory-limit downgrades need the "ignore hardware memory limits" opt-in), and
// it is what the UAT smoke's `warnings` class provokes.
TEST(status_wire_deprecated_circuit_runs_with_a_warning) {
    Report r = reportFor("[p2b8]\n[togglebutton]\n    button = B1.1\n    led = L1.1\n");
    CHECK(r.loadOk);
    CHECK(r.warningCount == 1);
    Status s = evaluate(r);
    CHECK(s.state == State::Warnings);
    CHECK(severityFor(s.state) == Severity::Warning);
    CHECK(s.code == ErrorCode::Unmapped);
    CHECK(s.line == 0);
    CHECK(s.message.find("deprecated") != std::string::npos);
    CHECK(s.title == "Running with 1 warning");
}
