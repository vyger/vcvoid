// patchsmoke: headless load + short-run smoke of arbitrary droid.ini files.
// Complements droidcheck (Forge oracle): this exercises OUR engine's parser,
// RAM accounting and first ticks. Exit code = number of failing patches.
#include "src/engine.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static bool smoke(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::printf("FAIL %s: cannot open\n", path.c_str());
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::string text = ss.str();

    droid::MasterType master = droid::MasterType::Master16;
    if (text.find("master=18") != std::string::npos ||
        text.find("master18") != std::string::npos)
        master = droid::MasterType::Master18;

    droid::Engine e(master);
    droid::LoadResult r = e.load(text);
    for (const auto& w : r.warnings)
        std::printf("  warn %s: %s\n", path.c_str(), w.c_str());
    if (!r.ok) {
        for (const auto& err : r.errors)
            std::printf("FAIL %s: line %d: %s\n", path.c_str(), err.line,
                        err.message.c_str());
        return false;
    }
    for (int i = 0; i < 6000; i++) e.tick();   // one simulated second
    std::printf("PASS %s (%u bytes RAM, %zu warnings)\n", path.c_str(),
                r.ramUsed, r.warnings.size());
    return true;
}

int main(int argc, char** argv) {
    int failed = 0;
    for (int i = 1; i < argc; i++)
        if (!smoke(argv[i])) failed++;
    return failed;
}
