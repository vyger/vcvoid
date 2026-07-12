// Equivalence check: load two patches, tick both with the same seed,
// compare O1..O8 every tick for one simulated minute.
#include "src/engine.hpp"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

static std::string slurp(const char* p) {
    std::ifstream f(p); std::stringstream ss; ss << f.rdbuf(); return ss.str();
}

int main(int argc, char** argv) {
    if (argc != 3) { std::fprintf(stderr, "usage: eqcheck a.ini b.ini\n"); return 2; }
    droid::Engine a, b;
    if (!a.load(slurp(argv[1])).ok) { std::printf("A failed to load\n"); return 1; }
    if (!b.load(slurp(argv[2])).ok) { std::printf("B failed to load\n"); return 1; }
    char reg[4] = "O1";
    for (long t = 0; t < 360000; t++) {
        a.tick(); b.tick();
        for (int o = 1; o <= 8; o++) {
            std::snprintf(reg, sizeof reg, "O%d", o);
            float va = a.getValue(reg), vb = b.getValue(reg);
            if (std::fabs(va - vb) > 1e-9f) {
                std::printf("DIVERGE tick %ld %s: %g vs %g\n", t, reg, va, vb);
                return 1;
            }
        }
    }
    std::printf("IDENTICAL over 360000 ticks\n");
    return 0;
}
