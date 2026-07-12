#include "harness.hpp"
int main() {
    for (auto& t : allTests()) t.fn();
    if (failCount()) { std::printf("%d FAILURE(S)\n", failCount()); return 1; }
    std::printf("OK: %zu tests passed\n", allTests().size());
    return 0;
}
