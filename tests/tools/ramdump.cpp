// ramdump — print the engine's RAM figure for each patch in both deploy modes:
//
//     RAM <file> plain  <bytes>      (sharing off — the Forge's default)
//     RAM <file> shared <bytes>      ("Detect and share duplicate values for
//                                      inputs", compression/deduplicate_jacks)
//
// `make ramcheck` diffs this against `droidcheck --ram`, which prints the same
// two lines straight out of the Forge's own Patch::usedRAM (issue #88).
//
// Memory limits are ignored here on purpose: the point is to measure, and a
// patch that is over budget still has an honest footprint to compare.
// Experimental circuits are allowed for the same reason — droidcheck's --ram
// also measures them, having merged the overlay into its firmware.
#include "src/loader.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

// Mirrors Patch::typeOfMaster (patch/patch.cpp:236): the type comes from the
// `master` label of a `# LABELS: ...` header comment, and defaults to 16.
// PatchParser::parseLabels splits that header on ';' and reads `key=value`.
static droid::MasterType masterOf(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        size_t hash = line.find('#');
        if (hash == std::string::npos) continue;
        size_t key = line.find("LABELS:", hash);
        if (key == std::string::npos) continue;
        std::string rest = line.substr(key + 7);
        for (size_t i = 0; i + 9 <= rest.size(); i++) {
            if (rest.compare(i, 7, "master=") != 0) continue;
            if (rest.compare(i + 7, 2, "18") == 0) return droid::MasterType::Master18;
        }
    }
    return droid::MasterType::Master16;
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        std::string path = argv[i];
        std::ifstream f(path);
        if (!f) { std::fprintf(stderr, "cannot read %s\n", path.c_str()); return 2; }
        std::stringstream ss;
        ss << f.rdbuf();
        std::string text = ss.str();

        std::string base = path;
        size_t slash = base.find_last_of('/');
        if (slash != std::string::npos) base = base.substr(slash + 1);

        droid::MasterType master = masterOf(text);
        for (int shared = 0; shared <= 1; shared++) {
            droid::LoadOptions opts;
            opts.ignoreMemoryLimits = true;
            opts.allowExperimental = true;
            opts.shareInputValues = shared == 1;
            droid::CompiledPatch cp;
            droid::LoadResult r = droid::compilePatch(text, master, cp, opts);
            // A patch we REFUSE is not comparable: the Forge keeps a jack whose
            // register is bogus (and charges for it) where we drop the whole
            // parameter, so the two disagree by a few bytes on the deliberately
            // broken UAT fixtures in patches/. The figure is never shown for
            // such a patch anyway — nothing loads. Say which ones were skipped
            // so the parity check cannot quietly shrink.
            if (!r.ok) {
                if (shared == 0)
                    std::fprintf(stderr, "ramdump: %s not measured — %s\n", base.c_str(),
                                 r.errors.empty() ? "?" : r.errors[0].message.c_str());
                continue;
            }
            std::printf("RAM %s %s %u\n", base.c_str(), shared ? "shared" : "plain",
                        cp.ramUsed);
        }
    }
    return 0;
}
