#pragma once
#include "types.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace droid {

struct RegId {
    char type = 0;      // 'I','O','N','G','P','B','L','S','R','X','E'
    uint8_t ctrl = 0;   // controller / G8 number; 0 for plain forms like I3, G5
    uint8_t num = 0;
    bool operator==(const RegId& o) const { return type == o.type && ctrl == o.ctrl && num == o.num; }
};

std::optional<RegId> parseRegisterName(const std::string& name);

// Motor faders are addressed by 1-based global number, not by a register (see
// controllerstate.hpp / motorfader.md). `F<n>` (n >= 1) is the harness handle;
// returns the number, or nullopt if the name is not a valid fader handle.
std::optional<int> parseFaderName(const std::string& name);
std::string toString(const RegId& r);
RegId canonicalize(RegId r, MasterType master);
inline uint32_t pack(const RegId& r) {
    return (uint32_t(uint8_t(r.type)) << 16) | (uint32_t(r.ctrl) << 8) | r.num;
}

class RegisterFile {
public:
    float get(const RegId& r) const;
    void set(const RegId& r, float v);   // clamps 'O' registers to [-1, 1]
    void setInputPatched(uint8_t n, bool p);
    bool inputPatched(uint8_t n) const;
private:
    std::unordered_map<uint32_t, float> values_;
    uint16_t patchedMask_ = 0;           // bit n-1 = I<n> has a "cable"
};

} // namespace droid
