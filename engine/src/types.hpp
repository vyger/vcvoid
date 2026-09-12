#pragma once
#include <string>
#include <vector>

namespace droid {

enum class MasterType { Master16, Master18 };

// Which of the hardware's error codes a load error is (manual/basics.md §5.4).
// The MASTER shows the reason for a refused patch as a colour in its 4x4 LED
// matrix — one colour per row of the manual's two tables — so every error the
// loader can raise has to say WHICH of those it is, not just what went wrong in
// English. Two tables, hence two blocks:
//
//   Global — the whole patch is refused, all 16 LEDs flash the colour.
//   Local  — one line is at fault, some LEDs flash the colour and the rest
//            spell out the line number (see vcvoid::status::blinkCode).
//
// `Unmapped` is for errors vcvoid raises that the hardware has no code for
// (they come from the Forge's stricter static analysis, or from vcvoid's own
// "circuit not implemented yet" gate). They are real errors and are reported in
// full as text; they simply have no blink code to show. See the mapping table
// in plugin/src/MasterStatus.hpp.
enum class ErrorCode {
    Unmapped = 0,
    // --- global (all 16 LEDs in this colour) ---
    PatchNotFound,        // yellow
    TooManyControllers,   // red
    PatchTooBig,          // blue
    OutOfMemory,          // cyan
    InvalidFirmware,      // magenta   (no vcvoid equivalent)
    NoSdCard,             // white     (no vcvoid equivalent)
    // --- local (colour + line number in the LEDs) ---
    UnknownRegister,      // yellow
    UnknownParameter,     // orange
    UnknownCircuit,       // red
    LineTooLong,          // blue      (no vcvoid equivalent)
    CableMisuse,          // green
    InvalidSyntax,        // magenta
};

struct LoadError {
    int line = 0;                 // 1-based line in the patch text; 0 = global
    std::string message;
    ErrorCode code = ErrorCode::Unmapped;
};

struct LoadResult {
    bool ok = false;
    std::vector<LoadError> errors;
    std::vector<std::string> warnings;   // e.g. deprecated circuits
    unsigned ramUsed = 0;                // bytes, Forge-compatible accounting
};

// Experimental load switches (context-menu "Experimental" section, #13).
struct LoadOptions {
    // Downgrade the hardware memory limits — the RAM-budget walk and the
    // 64 000-byte patch-size cap — from load errors to warnings. ramUsed still
    // reports the honest footprint.
    bool ignoreMemoryLimits = false;

    // Allow vcvoid-only EXPERIMENTAL circuits (#12) to load. Off by default:
    // an experimental circuit does not exist on DROID hardware and is unknown
    // to the Forge, so a patch using one is refused unless the user opts in.
    // See docs/adr/0001-experimental-circuits.md.
    bool allowExperimental = false;
};

} // namespace droid
