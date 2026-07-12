#include "signal.hpp"

namespace droid {

float Operand::value(const EngineState& s) const {
    switch (kind) {
        case Kind::Const:    return constant;
        case Kind::Register: return s.regs.get(reg);
        case Kind::Cable:    return s.cables[cableIndex];
    }
    return 0.0f;
}

void Output::set(EngineState& s, float v) const {
    if (!present_) return;
    if (target_.kind == Operand::Kind::Register) s.regs.set(target_.reg, v);
    else if (target_.kind == Operand::Kind::Cable) s.cables[target_.cableIndex] = v;
}

} // namespace droid
