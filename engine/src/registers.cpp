#include "registers.hpp"
#include <cctype>

namespace droid {

static bool parseUint8(const std::string& s, size_t from, size_t to, uint8_t& out) {
    if (from >= to) return false;
    unsigned v = 0;
    for (size_t i = from; i < to; i++) {
        if (!std::isdigit((unsigned char)s[i])) return false;
        v = v * 10 + unsigned(s[i] - '0');
        if (v > 255) return false;
    }
    out = uint8_t(v);
    return true;
}

std::optional<RegId> parseRegisterName(const std::string& name) {
    static const std::string kTypes = "IONGPBLSRXE";
    if (name.size() < 2) return std::nullopt;
    char t = char(std::toupper((unsigned char)name[0]));
    if (kTypes.find(t) == std::string::npos)
        return std::nullopt;
    RegId r;
    r.type = t;
    size_t dot = name.find('.');
    if (dot == std::string::npos) {
        if (!parseUint8(name, 1, name.size(), r.num)) return std::nullopt;
    } else {
        if (!parseUint8(name, 1, dot, r.ctrl)) return std::nullopt;
        if (!parseUint8(name, dot + 1, name.size(), r.num)) return std::nullopt;
        if (r.ctrl == 0) return std::nullopt;
    }
    // No register is 0-indexed, in any form. The Forge rejects a zero element
    // number (`P1.0`) with "The number of the register may not be less than 1";
    // reject it here for both plain (`P0`) and dotted (`P1.0`) forms so a
    // dotted num==0 cannot slip past into downstream validity checks.
    if (r.num == 0) return std::nullopt;
    return r;
}

std::optional<int> parseFaderName(const std::string& name) {
    if (name.size() < 2 || std::toupper((unsigned char)name[0]) != 'F') return std::nullopt;
    unsigned v = 0;
    for (size_t i = 1; i < name.size(); i++) {
        if (!std::isdigit((unsigned char)name[i])) return std::nullopt;
        v = v * 10 + unsigned(name[i] - '0');
        if (v > 100000) return std::nullopt;
    }
    if (v < 1) return std::nullopt;
    return int(v);
}

std::string toString(const RegId& r) {
    std::string s(1, r.type);
    if (r.ctrl) { s += std::to_string(r.ctrl); s += '.'; }
    s += std::to_string(r.num);
    return s;
}

RegId canonicalize(RegId r, MasterType master) {
    if (r.type == 'G' && r.ctrl == 0 && master == MasterType::Master16 && r.num <= 8)
        r.ctrl = 1;    // G5 == G1.5 on the MASTER
    return r;
}

float RegisterFile::get(const RegId& r) const {
    if (r.type == 'I' && r.ctrl == 0 && r.num >= 1 && r.num <= 8 && !inputPatched(r.num)) {
        RegId n{'N', 0, r.num};
        auto it = values_.find(pack(n));
        return it == values_.end() ? 0.0f : it->second;
    }
    auto it = values_.find(pack(r));
    return it == values_.end() ? 0.0f : it->second;
}

void RegisterFile::set(const RegId& r, float v) {
    if (r.type == 'O') {
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
    }
    values_[pack(r)] = v;
}

void RegisterFile::setInputPatched(uint8_t n, bool p) {
    if (n < 1 || n > 8) return;
    if (p) patchedMask_ |= uint16_t(1u << (n - 1));
    else   patchedMask_ &= uint16_t(~(1u << (n - 1)));
}

bool RegisterFile::inputPatched(uint8_t n) const {
    if (n < 1 || n > 8) return false;
    return patchedMask_ & (1u << (n - 1));
}

} // namespace droid
