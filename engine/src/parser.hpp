#pragma once
#include "atom.hpp"
#include "types.hpp"
#include <vector>

namespace droid {

struct ParamLine {
    std::string name;    // raw, as written (may be a short alias / array form)
    int line = 0;
    Atom a, b, c;        // canonical A*B+C
    bool simple = false; // single atom, no operators
    // The source was the `X - REG` shorthand. We store it as A=REG, B=-1, C=X;
    // the Forge's form6 stores the same expression as A=-1, B=REG, C=X. The
    // arithmetic is identical, but the Forge's shared-value key is built from
    // the atoms IN SLOT ORDER, so RAM accounting has to know to swap A and B
    // back when it builds that key (ram.cpp, issue #88).
    bool subtractForm = false;
};

struct CircuitSection {
    std::string name;
    int line = 0;
    std::vector<ParamLine> params;
};

struct ParseResult {
    std::vector<CircuitSection> sections;
    std::vector<LoadError> errors;
    // Interned text table (manual/basics.md §5.8). Slot 0 is reserved as the
    // empty text so table[n] == text number n; unique quoted strings get
    // 1,2,3,… in patch-read order, and the same string reuses its number.
    std::vector<std::string> texts{std::string()};
};

// Intern a text into a 1-based table (slot 0 == ""); the empty string is 0.
// Returns the text number. Shared with the loader, which interns the derived
// DB8E auto-headers into the same table after parsing (issue #19).
int internText(const std::string& content, std::vector<std::string>& texts);

ParseResult parsePatch(const std::string& text);
// Comments and layout removed, the way the master stores a patch. The size the
// 64 000-byte limit is enforced on abbreviates parameter names on top of this —
// see droid::deployedPatchSize (loader.hpp).
std::string stripPatch(const std::string& text);

} // namespace droid
