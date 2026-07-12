#pragma once
#include "loader.hpp"

namespace droid {

// Forge-compatible RAM accounting. Returns total bytes used and appends one
// LoadError per circuit that crosses the memory budget (in patch order).
//
// Rule source (droidforge, firmware blue-7):
//   patch/jackdeduplicator.cpp   - per-jack byte costs by ramhint + input opts
//   patch/patch.cpp              - updateMemoryProblems (763-833),
//                                  countUniqueConstants (1097-1122)
//   patch/circuit.cpp            - Circuit::RAMUsage (base + jack costs)
unsigned computeRam(const CompiledPatch& p, MasterType master,
                    std::vector<LoadError>& errorsOut);

} // namespace droid
