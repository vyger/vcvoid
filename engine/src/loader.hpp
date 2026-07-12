#pragma once
#include "parser.hpp"
#include "../gen/jacktables.gen.hpp"
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

LoadResult compilePatch(const std::string& text, MasterType master, CompiledPatch& out);

} // namespace droid
