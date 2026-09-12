#pragma once

#include <string>
#include <vector>

#include "src/types.hpp"   // droid::LoadResult / droid::LoadError

// One structured description of a master's condition (issues #46, #49).
//
// Pure logic, no Rack dependencies — unit-tested headless
// (tests/unit/test_masterdiagnostics.cpp). It is deliberately a FUNCTION of a
// snapshot, not a method on DroidMasterBase: everything it needs
// (`patchStatus`, `lastResult`, `stateStatus`, `chainError`, the MIDI
// diagnostic) is engineMutex-guarded state that a caller already has to copy
// out under the lock, and keeping the derivation pure is what lets the UAT
// bridge (`GET /master/{id}/diagnostics`) and the panel's own error display
// answer from the SAME record instead of each re-reading the status line with
// its own regex.
//
// The vocabulary is the one the panel work (#46) needs:
//
//   state     which of the five distinguishable conditions the master is in;
//   severity  how loudly to say it (nothing / a note / a warning / an error);
//   code      the HARDWARE error code this maps onto, plus the colour the
//             MASTER's 4x4 matrix blinks for it (manual/basics.md §5.4), so a
//             blink-code renderer needs no table of its own. Empty when the
//             condition has no hardware code or we cannot classify it —
//             deliberately empty rather than guessed;
//   line      the 1-based patch line a local error points at (0 = global /
//             not applicable), i.e. the number the matrix encodes in its
//             input/output LEDs;
//   title     a short label ("Patch load failed");
//   message   the one sentence that says what is wrong;
//   warnings  every warning the load produced, verbatim, plus the MIDI
//             diagnostic when it applies.
//
// Nothing here formats voltages, LEDs or pixels: how a state is *shown* is the
// panel's business, and the UAT bridge asserts on this record instead of on
// anything visual.

namespace vcvoid {
namespace diag {

// The five conditions a master can be in, in the order they take precedence
// when more than one applies (a patch that failed to load is "load_failed"
// even if the chain is also wrong — nothing is running either way, and the
// load error is the actionable one).
enum class State {
    NoPatch,      // no patch has been loaded into this master at all
    LoadFailed,   // the last load was rejected; the engine is stopped
    ChainError,   // loaded, but the controller chain does not match the patch
    Warnings,     // running, with something the user should know about
    Running,      // running clean
};

enum class Severity { Ok, Info, Warning, Error };

inline const char* stateName(State s) {
    switch (s) {
        case State::NoPatch:    return "no_patch";
        case State::LoadFailed: return "load_failed";
        case State::ChainError: return "chain_error";
        case State::Warnings:   return "warnings";
        case State::Running:    return "running";
    }
    return "running";
}

inline const char* severityName(Severity s) {
    switch (s) {
        case Severity::Ok:      return "ok";
        case Severity::Info:    return "info";
        case Severity::Warning: return "warning";
        case Severity::Error:   return "error";
    }
    return "ok";
}

// A snapshot of the master's state. Every field is something the caller reads
// under engineMutex in one go (see DroidMasterBase / Bridge::handleMasterStatus).
struct Input {
    std::string patchPath;      // empty = never loaded a patch
    std::string statusLine;     // DroidMasterBase::patchStatus
    std::string stateLine;      // DroidMasterBase::stateStatus (issue #42)
    std::string chainError;     // DroidMasterBase::chainError, empty = ok
    bool midiWarning = false;   // patch uses MIDI, no MIDI hardware reachable
    bool engineRunning = false; // a live engine exists (the last load succeeded)
    droid::LoadResult load;     // DroidMasterBase::lastResult
};

struct Diagnostics {
    State state = State::NoPatch;
    Severity severity = Severity::Info;
    std::string code;        // hardware error-code name, "" when unclassified
    std::string codeColor;   // the matrix blink colour for `code`, "" with it
    int line = 0;            // 1-based patch line, 0 = global / not applicable
    std::string title;
    std::string message;
    std::vector<std::string> warnings;
    std::string patchPath;
    std::string stateLine;
};

// Hardware error code + the colour the MASTER's matrix blinks for it
// (manual/basics.md §5.4 "Table of error codes"). `global` means the hardware
// flashes ALL 16 LEDs (a whole-patch problem) rather than encoding a line
// number in a subset.
struct CodeMatch {
    const char* name = "";
    const char* color = "";
    bool global = false;
};

namespace detail {

inline bool contains(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

}  // namespace detail

// Map one engine load-error message onto the hardware's error-code table.
// Ordered most-specific first; an unrecognised message yields an empty match
// rather than a plausible-looking guess, so a renderer can tell "we know the
// hardware code" from "we only have the text".
inline CodeMatch classifyError(const droid::LoadError& e) {
    using detail::contains;
    const std::string& m = e.message;
    // Global codes (all LEDs flash; no line number encoded).
    if (contains(m, "exceeds the maximum size"))
        return {"patch_too_big", "blue", true};
    if (contains(m, "exceeds the available memory"))
        return {"out_of_memory", "cyan", true};
    // Local codes (a subset flashes and encodes the line number).
    if (contains(m, "patch cable"))
        return {"cable_misuse", "green", false};
    if (contains(m, "Unknown circuit") || contains(m, "is not yet implemented") ||
        contains(m, "is experimental") || contains(m, "needs a MASTER18"))
        return {"unknown_circuit", "red", false};
    if (contains(m, "has no parameter"))
        return {"unknown_parameter", "orange", false};
    if (contains(m, "Invalid input number") || contains(m, "Invalid output number") ||
        contains(m, "Invalid gate number") || contains(m, "normalization registers") ||
        contains(m, "Duplicate usage of") || contains(m, "register") ||
        contains(m, "used as an input") || contains(m, "used as an output"))
        return {"unknown_register", "yellow", false};
    if (contains(m, "malformed section header") || contains(m, "expected 'parameter") ||
        contains(m, "parameter outside of any") || contains(m, "missing parameter name") ||
        contains(m, "missing value") || contains(m, "unterminated"))
        return {"invalid_syntax", "magenta", false};
    return {};
}

// "cannot open <path>" is written by loadPatchFile when the file itself could
// not be read — the hardware's global yellow "Patch not found".
inline bool isPatchNotFound(const std::string& statusLine) {
    return statusLine.rfind("cannot open ", 0) == 0;
}

inline Diagnostics diagnose(const Input& in) {
    Diagnostics d;
    d.patchPath = in.patchPath;
    d.stateLine = in.stateLine;
    d.warnings = in.load.warnings;
    if (in.midiWarning)
        d.warnings.push_back(
            "patch uses MIDI but no MIDI hardware is reachable");

    if (!in.engineRunning) {
        if (in.patchPath.empty()) {
            d.state = State::NoPatch;
            d.severity = Severity::Info;
            d.title = "No patch loaded";
            d.message = "No patch loaded.";
            // The hardware blinks global yellow ("patch not found") for a
            // master that has never loaded a patch; a freshly placed Rack
            // module is not an error condition, so no code is reported and the
            // panel is free to stay quiet.
            return d;
        }
        d.state = State::LoadFailed;
        d.severity = Severity::Error;
        if (isPatchNotFound(in.statusLine)) {
            d.code = "patch_not_found";
            d.codeColor = "yellow";
            d.title = "Patch not found";
            d.message = in.statusLine;
            return d;
        }
        d.title = "Patch load failed";
        if (!in.load.errors.empty()) {
            const droid::LoadError& e = in.load.errors.front();
            CodeMatch c = classifyError(e);
            d.code = c.name;
            d.codeColor = c.color;
            d.line = c.global ? 0 : e.line;
            d.message = e.message;
        } else {
            // No structured error survived (e.g. the status line is all we
            // have): report the line as unknown rather than inventing one.
            d.message = in.statusLine.empty() ? "Patch load failed." : in.statusLine;
        }
        return d;
    }

    if (!in.chainError.empty()) {
        d.state = State::ChainError;
        d.severity = Severity::Error;
        d.title = "Chain error";
        d.message = in.chainError;
        return d;
    }

    if (!d.warnings.empty()) {
        d.state = State::Warnings;
        d.severity = Severity::Warning;
        d.title = d.warnings.size() == 1 ? "Patch loaded with 1 warning"
                                         : "Patch loaded with " +
                                           std::to_string(d.warnings.size()) +
                                           " warnings";
        d.message = d.warnings.front();
        return d;
    }

    d.state = State::Running;
    d.severity = Severity::Ok;
    d.title = "Running";
    d.message = in.statusLine;
    return d;
}

}  // namespace diag
}  // namespace vcvoid
