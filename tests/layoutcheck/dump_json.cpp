// Dumps Layout.hpp as JSON for the Python art-alignment check (Layer 2).
#include "Layout.hpp"
#include <cstdio>

int main() {
    const char types[] = {'I','N','O','G','B','L','P','E','S','R','X'};
    std::printf("{");
    bool firstM = true;
    for (const auto& m : droid::layout::kModules) {
        std::printf("%s\"%s\":{\"hp\":%g,\"controls\":[", firstM ? "" : ",", m.name, m.hp);
        firstM = false;
        bool firstC = true;
        for (char t : types)
            for (unsigned n = 1; n <= m.num(t); n++) {
                auto p = m.pos(t, n);
                std::printf("%s{\"type\":\"%c\",\"n\":%u,\"x\":%g,\"y\":%g,\"size\":%g}",
                            firstC ? "" : ",", t, n, p.x, p.y, m.size(t, n));
                firstC = false;
            }
        std::printf("]}");
    }
    std::printf("}\n");
    return 0;
}
