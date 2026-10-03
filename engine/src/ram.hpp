#pragma once
#include "loader.hpp"

namespace droid {

// Forge-compatible RAM accounting. Returns total bytes used and appends one
// LoadError per circuit that crosses the memory budget (in patch order).
//
// `shareInputValues` mirrors the Forge's deploy preference "Detect and share
// duplicate values for inputs" (compression/deduplicate_jacks, issue #88): with
// it on, an input jack whose value already appeared earlier in the patch is
// deployed as a reference and costs nothing, and its texts are not charged.
//
// Rule source (droidforge, firmware blue-7):
//   patch/jackdeduplicator.cpp   - per-jack byte costs by ramhint + input opts,
//                                  and the whole sharing rule
//   patch/patch.cpp              - updateMemoryProblems (763-833), usedRAM
//                                  (1006-1075), countUniqueConstants (1097-1122)
//   patch/circuit.cpp            - Circuit::RAMUsage (base + jack costs)
unsigned computeRam(const CompiledPatch& p, MasterType master, bool shareInputValues,
                    std::vector<LoadError>& errorsOut);

// The key the Forge shares input values on: JackAssignmentInput::
// valueToCanonicalString() (patch/jackassignmentinput.cpp:127). Exposed for the
// unit tests, which pin the equivalence classes directly.
std::string canonicalInputValue(const CompiledParam& p,
                                const std::vector<std::string>& texts);

} // namespace droid
