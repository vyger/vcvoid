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
std::string stripPatch(const std::string& text);   // for the 64 000-byte limit

} // namespace droid
