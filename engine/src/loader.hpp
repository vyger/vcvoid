#pragma once
#include "parser.hpp"
#include "../gen/jacktables.gen.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace droid {

struct CompiledParam {
    const gen::JackDef* def = nullptr;
    int arrayIndex = 1;
    Atom a, b, c;
    bool simple = false;
    int line = 0;
};

struct CompiledCircuit {
    const gen::CircuitDef* def = nullptr;
    int line = 0;
    std::vector<CompiledParam> params;
};

struct CompiledPatch {
    std::vector<CompiledCircuit> circuits;      // patch order
    std::vector<std::string> controllers;       // declaration order
    std::vector<std::string> cableNames;        // sorted, unique
    std::vector<std::string> texts;             // interned text table (slot 0 == "")
    unsigned ramUsed = 0;                       // filled by RAM accounting (Task 7)
};

// The hardware patch-size limit (manual/basics.md; the Forge's MAX_DROID_INI).
constexpr size_t kMaxPatchSize = 64000;

// Size of the patch as the master receives it, which is what kMaxPatchSize
// applies to: comments and layout stripped AND every parameter name abbreviated
// to its firmware short form, the way the Forge deploys a patch to the SD card
// (Patch::toDeployString -> DroidFirmware::jackShortname). `text` itself is
// never rewritten — a patch may be stored verbose and still fit. Mirrors
// tools/inicompress.py.
size_t deployedPatchSize(const std::string& text);

LoadResult compilePatch(const std::string& text, MasterType master, CompiledPatch& out,
                        const LoadOptions& opts = {});

} // namespace droid
