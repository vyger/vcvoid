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

} // namespace droid
