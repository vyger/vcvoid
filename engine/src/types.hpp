#pragma once
#include <string>
#include <vector>

namespace droid {

enum class MasterType { Master16, Master18 };

struct LoadError {
    int line = 0;                 // 1-based line in the patch text; 0 = global
    std::string message;
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
