#pragma once
#include "registers.hpp"
#include <string>

namespace droid {

struct Atom {
    enum class Kind { None, Number, Register, Cable };
    Kind kind = Kind::None;
    float number = 0.0f;
    RegId reg;
    std::string cable;           // full name including leading '_'
    bool fromSource = false;     // false for synthesized implicit B=1 / C=0
    bool isText = false;         // came from a quoted string: `number` is its text
                                 // number (parser.cpp internText). The Forge models
                                 // this as AtomText, NOT AtomNumber, so RAM accounting
                                 // must charge it as a text (6 bytes) and must NOT
                                 // count it as a constant or apply the 0/1 jack
                                 // optimization — see ram.cpp.
    bool isFraction = false;     // division divisor: Forge counts n AND 1/n as constants
    float fractionDenom = 0.0f;  // isFraction only: the ORIGINAL divisor as written
                                 // (e.g. 12 for `X / 12`). RAM accounting recomputes
                                 // the folded value 1/denom at double precision to
                                 // match the Forge byte-for-byte (see ram.cpp); the
                                 // runtime `number` stays the folded float multiplier.

    static Atom num(float v, bool src) { Atom a; a.kind = Kind::Number; a.number = v; a.fromSource = src; return a; }
};

} // namespace droid
