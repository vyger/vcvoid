#pragma once
#include "types.hpp"
#include <array>
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

// Dense-storage bounds for RegisterFile's fast path. Chosen from real DROID
// limits (manual/basics.md): up to 16 controllers on the chain, and the
// widest per-controller register count is B32's 32 buttons. Anything outside
// these bounds (exotic/malformed ids) falls back to an unordered_map so
// behavior is unchanged, just slower.
inline constexpr int kRegTypeCount = 11;      // "IONGPBLSRXE"
inline constexpr int kRegCtrlCount = 17;      // ctrl 0..16 (0 = plain form)
inline constexpr int kRegNumCount = 33;       // num 0..32 (0 unused, 1-indexed)

// Returns the type's index into the dense table, or -1 if not one of the
// known register type letters.
inline int regTypeIndex(char type) {
    switch (type) {
        case 'I': return 0;
        case 'O': return 1;
        case 'N': return 2;
        case 'G': return 3;
        case 'P': return 4;
        case 'B': return 5;
        case 'L': return 6;
        case 'S': return 7;
        case 'R': return 8;
        case 'X': return 9;
        case 'E': return 10;
        default: return -1;
    }
}

// Computes the dense slot for r, or -1 if r falls outside the dense bounds
// (caller must then use the overflow map).
inline int denseRegSlot(const RegId& r) {
    int t = regTypeIndex(r.type);
    if (t < 0) return -1;
    if (r.ctrl >= kRegCtrlCount) return -1;
    if (r.num >= kRegNumCount) return -1;
    return (t * kRegCtrlCount + r.ctrl) * kRegNumCount + r.num;
}

class RegisterFile {
public:
    float get(const RegId& r) const;
    void set(const RegId& r, float v);   // clamps 'O' registers to [-1, 1]
    void setInputPatched(uint8_t n, bool p);
    bool inputPatched(uint8_t n) const;
private:
    std::array<float, kRegTypeCount * kRegCtrlCount * kRegNumCount> dense_{};
    std::unordered_map<uint32_t, float> overflow_;
    uint16_t patchedMask_ = 0;           // bit n-1 = I<n> has a "cable"
};

} // namespace droid
