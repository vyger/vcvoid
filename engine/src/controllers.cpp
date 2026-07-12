#include "controllers.hpp"

namespace droid {

// Verified against manual/hardware.md (firmware blue-7):
//   p2b8 §6.4 : 2 pots P1.1–P1.2, 8 buttons B1.1–B1.8, 8 LEDs L1.1–L1.8.
//   p4b2 §6.5 : 4 pots P1.1–P1.4, 2 buttons B1.1–B1.2, 2 LEDs L1.1–L1.2.
//   p10  §6.6 : 10 pots P1.1–P1.10 (2 big + 8 small, all plain P); no B/L/S.
//   s10  §6.7 : 10 switches S1.1–S1.10 (2 rotary 0–7, 8 three-way); no pots.
//   p8s8 §6.8 : 8 sliders as pots P1.1–P1.8, 8 slider LEDs L1.1–L1.8,
//               8 three-way switches S1.1–S1.8; no buttons.
//   b32  §6.9 : 32 buttons B1.1–B1.32, 32 LEDs L1.1–L1.32; no pots/switches.
//   e4   §6.10: 4 encoders E1.1–E1.4, 4 push buttons B1.1–B1.4, 4 ring LEDs
//               L1.1–L1.4; no pots/switches.
//   m4   §6.11: 4 motor faders; touch plates are buttons B1.1–B1.4; 4 LEDs
//               L1.1–L1.4 (R registers for colour are out of M4a scope).
//   db8e §6.12: 8 push buttons B1.1–B1.8 (with LEDs L1.1–L1.8, like a P2B8) plus
//               the encoder's own push as B1.9; 1 encoder E1.1 whose ring LED is
//               L1.9 (Forge moduledb8e.cpp: numRegisters BUTTON=9, LED=9, ENC=1).
//               The display itself has no register.
static const ControllerModel kModels[] = {
    // name    pots buttons leds switches enc faders permissive
    {"p2b8",   2,   8,      8,   0,       0,  0,     false},
    {"p4b2",   4,   2,      2,   0,       0,  0,     false},
    {"p10",   10,   0,      0,   0,       0,  0,     false},
    {"s10",    0,   0,      0,  10,       0,  0,     false},
    {"p8s8",   8,   0,      8,   8,       0,  0,     false},
    {"b32",    0,  32,     32,   0,       0,  0,     false},
    {"e4",     0,   4,      4,   0,       4,  0,     false},
    {"m4",     0,   4,      4,   0,       0,  4,     false},
    {"db8e",   0,   9,      9,   0,       1,  0,     false},
};

const ControllerModel* findControllerModel(const std::string& name) {
    for (auto& m : kModels)
        if (name == m.name) return &m;
    return nullptr;
}

bool controllerHasElement(const ControllerModel& m, char t, unsigned num) {
    if (m.permissive) return num >= 1;
    uint8_t max = 0;
    switch (t) {
        case 'P': max = m.pots;     break;
        case 'B': max = m.buttons;  break;
        case 'L': max = m.leds;     break;
        case 'S': max = m.switches; break;
        case 'E': max = m.encoders; break;
        default:  return false;
    }
    return num >= 1 && num <= max;
}

} // namespace droid
