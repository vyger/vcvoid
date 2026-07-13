// bench: headless per-circuit CPU benchmark (issue #5).
//
// Loads a droid.ini patch into the engine, runs it for a fixed number of
// ticks with per-circuit profiling enabled, and reports where the CPU time
// goes — aggregated by circuit TYPE (not instance) so optimization work can
// target the biggest offenders. No VCV Rack required.
//
// Usage: bench <patch.ini> [ticks]   (default 200000 ticks)
#include "src/engine.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct TypeAgg {
    std::string name;
    int count = 0;
    double sumUsPerTick = 0.0;   // sum of each instance's avg us/tick
};

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <patch.ini> [ticks]\n", argv[0]);
        return 2;
    }
    const std::string path = argv[1];
    uint64_t ticks = 200000;
    if (argc >= 3) ticks = std::strtoull(argv[2], nullptr, 10);

    std::ifstream f(path);
    if (!f) {
        std::fprintf(stderr, "bench: cannot open %s\n", path.c_str());
        return 2;
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
    if (!r.ok) {
        if (!r.errors.empty())
            std::fprintf(stderr, "bench: load failed: line %d: %s\n",
                         r.errors[0].line, r.errors[0].message.c_str());
        else
            std::fprintf(stderr, "bench: load failed (no error detail)\n");
        return 2;
    }

    // Warm up unprofiled: cold caches / branch predictors distort the first
    // measured ticks otherwise.
    for (uint64_t i = 0; i < 5000; i++) e.tick();

    e.setProfiling(true);   // clears accumulators

    auto wallStart = std::chrono::steady_clock::now();
    for (uint64_t i = 0; i < ticks; i++) e.tick();
    auto wallEnd = std::chrono::steady_clock::now();
    double wallSeconds = std::chrono::duration<double>(wallEnd - wallStart).count();

    std::vector<droid::Engine::CircuitProfile> snap = e.profileSnapshot();

    // Aggregate by circuit type name.
    std::map<std::string, TypeAgg> byType;
    double engineTotalUsPerTick = 0.0;
    for (const auto& cp : snap) {
        std::string name = cp.name ? cp.name : "?";
        double avgUsPerTick = cp.ticks > 0 ? (cp.totalUs / double(cp.ticks)) : 0.0;
        TypeAgg& agg = byType[name];
        agg.name = name;
        agg.count += 1;
        agg.sumUsPerTick += avgUsPerTick;
        engineTotalUsPerTick += avgUsPerTick;
    }

    std::vector<TypeAgg> rows;
    rows.reserve(byType.size());
    for (auto& kv : byType) rows.push_back(kv.second);
    std::sort(rows.begin(), rows.end(), [](const TypeAgg& a, const TypeAgg& b) {
        return a.sumUsPerTick > b.sumUsPerTick;
    });

    std::printf("%-20s %6s %10s %8s %12s\n", "type", "count", "us/tick", "pct", "us/instance");
    for (const auto& row : rows) {
        double pct = engineTotalUsPerTick > 0.0 ? (100.0 * row.sumUsPerTick / engineTotalUsPerTick) : 0.0;
        double perInstance = row.count > 0 ? (row.sumUsPerTick / row.count) : 0.0;
        std::printf("%-20s %6d %10.2f %7.1f%% %12.3f\n",
                    row.name.c_str(), row.count, row.sumUsPerTick, pct, perInstance);
    }

    double ticksPerSec = wallSeconds > 0.0 ? (double(ticks) / wallSeconds) : 0.0;
    std::printf("TOTAL %zu circuits, %.2f us/tick, %.0f ticks/sec\n",
                snap.size(), engineTotalUsPerTick, ticksPerSec);
    std::printf("ramUsed=%u bytes\n", e.ramUsed());

    for (const auto& row : rows) {
        std::printf("BENCH %s %d %.4f\n", row.name.c_str(), row.count, row.sumUsPerTick);
    }

    return 0;
}
