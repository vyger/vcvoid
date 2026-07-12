#pragma once
// Panel layout of every DROID module, transcribed 1:1 from the Droid Forge's
// modules/module<name>.cpp (github.com/Zarkuun/droidforge, GPL-3). Positions
// are control CENTERS in HP units (1 HP = 5.08 mm), origin at the panel's
// top-left; `number` is 1-based; `size` is the control extent in HP. Register
// type codes are the Forge's register_type_t chars: I N O G B L P E S R X.
// Verified exactly against the Forge source by `make layoutcheck` — edit that
// harness, not just this file, when the Forge changes.
// Rack-free on purpose: headless tests include this.
#include <cstring>

namespace droid {
namespace layout {

constexpr float kHPmm = 5.08f;      // 1 HP in mm
constexpr float kPanelMm = 128.5f;  // Eurorack 3U panel height

struct Pos { float x, y; };

struct ModuleLayout {
    const char* name;
    float hp;
    unsigned (*num)(char type);
    Pos (*pos)(char type, unsigned number);
    float (*size)(char type, unsigned number);
};

// ---- master (modulemaster.cpp, 8 HP) ----------------------------------
inline const ModuleLayout kMaster = {"master", 8,
    [](char t) -> unsigned {
        if (t == 'I' || t == 'O' || t == 'N') return 8;
        if (t == 'R') return 16;
        if (t == 'X') return 1;
        return 0;
    },
    [](char t, unsigned n) -> Pos {
        int column = (n - 1) % 4, row = (n - 1) / 4;
        float x = column * 2.00f + 0.98f, y;
        if (t == 'I' || t == 'N') y = row * 2.55f + 14.47f;
        else if (t == 'O')        y = row * 2.55f + 19.57f;
        else if (t == 'R') {
            y = row * 1.775f + 5.88f;
            if (row >= 2) y += 0.37f;
            x = column * 1.775f + 1.35f;
        }
        else return {4.0f, 8.70f};   // X1
        return {x, y};
    },
    [](char t, unsigned) -> float {
        if (t == 'R') return 2.1f;
        if (t == 'X') return 11.5f;
        return 2.5f;
    }};

// ---- master18 (modulemaster18.cpp, 6 HP) ------------------------------
inline const ModuleLayout kMaster18 = {"master18", 6,
    [](char t) -> unsigned {
        if (t == 'I') return 2;
        if (t == 'O') return 8;
        if (t == 'G') return 4;
        return 0;
    },
    [](char t, unsigned n) -> Pos {
        int row, column;
        if (t == 'I')      { row = n - 1; column = 0; }
        else if (t == 'G') { row = n + 1; column = 0; }
        else               { row = 2 + (n - 1) / 2; column = 1 + (n - 1) % 2; }
        return {column * 2.00f + 0.98f, row * 2.55f + 9.37f};
    },
    [](char, unsigned) -> float { return 2.5f; }};

// ---- x7 (modulex7.cpp, 4 HP) -------------------------------------------
inline const ModuleLayout kX7 = {"x7", 4,
    [](char t) -> unsigned {
        if (t == 'G') return 4;
        if (t == 'R') return 8;
        return 0;
    },
    [](char t, unsigned n) -> Pos {
        int column = (n - 1) % 2, row = (n - 1) / 2;
        if (t == 'G')
            return {column * 2.00f + 0.98f, (row + 2) * 2.55f + 14.47f};
        float y = row * 1.775f + 5.88f;
        if (row >= 2) y += 0.37f;
        return {column * 1.775f + 1.15f, y};
    },
    [](char t, unsigned) -> float { return t == 'R' ? 2.1f : 2.5f; }};

// ---- g8 (moduleg8.cpp, 4 HP) -------------------------------------------
inline const ModuleLayout kG8 = {"g8", 4,
    [](char t) -> unsigned { return (t == 'G' || t == 'R') ? 8u : 0u; },
    [](char t, unsigned n) -> Pos {
        int column = (n - 1) % 2, row = (n - 1) / 2;
        if (t == 'G')
            return {column * 2.00f + 0.98f, row * 2.55f + 14.47f};
        float y = row * 1.775f + 5.88f;
        if (row >= 2) y += 0.37f;
        return {column * 1.775f + 1.15f, y};
    },
    [](char t, unsigned) -> float { return t == 'R' ? 2.1f : 2.5f; }};

// ---- p2b8 (modulep2b8.cpp, 5 HP) ---------------------------------------
inline const ModuleLayout kP2B8 = {"p2b8", 5,
    [](char t) -> unsigned {
        if (t == 'B' || t == 'L') return 8;
        if (t == 'P') return 2;
        return 0;
    },
    [](char t, unsigned n) -> Pos {
        if (t == 'P') return {5.f / 2, 4.91f * (n - 1) + 3.50f};
        unsigned column = (n - 1) % 2, row = (n - 1) / 2;
        return {column * 2.5f + 1.27f, row * 2.97f + 13.43f};
    },
    [](char t, unsigned) -> float { return t == 'P' ? 4.1f : 2.7f; }};

// ---- p4b2 (modulep4b2.cpp, 5 HP) ---------------------------------------
inline const ModuleLayout kP4B2 = {"p4b2", 5,
    [](char t) -> unsigned {
        if (t == 'B' || t == 'L') return 2;
        if (t == 'P') return 4;
        return 0;
    },
    [](char t, unsigned n) -> Pos {
        if (t == 'P') return {5.f / 2, 4.91f * (n - 1) + 3.50f};
        unsigned column = (n - 1) % 2;
        return {column * 2.5f + 1.27f, 3 * 2.97f + 13.43f};
    },
    [](char t, unsigned) -> float { return t == 'P' ? 4.1f : 2.7f; }};

// ---- p10 (modulep10.cpp, 5 HP) -----------------------------------------
inline const ModuleLayout kP10 = {"p10", 5,
    [](char t) -> unsigned { return t == 'P' ? 10u : 0u; },
    [](char, unsigned n) -> Pos {
        if (n <= 2) return {5.f / 2, 4.91f * (n - 1) + 3.50f};
        unsigned column = (n - 3) % 2, row = (n - 3) / 2;
        return {column * 2.42f + 1.30f, row * 2.94f + 12.93f};
    },
    [](char, unsigned n) -> float { return n <= 2 ? 4.1f : 1.5f; }};

// ---- s10 (modules10.cpp, 5 HP) -----------------------------------------
inline const ModuleLayout kS10 = {"s10", 5,
    [](char t) -> unsigned { return t == 'S' ? 10u : 0u; },
    [](char, unsigned n) -> Pos {
        if (n <= 2) return {5.f / 2, 4.91f * (n - 1) + 3.50f};
        unsigned column = (n - 3) % 2, row = (n - 3) / 2;
        return {column * 2.36f + 1.32f, row * 2.964f + 13.35f};
    },
    [](char, unsigned n) -> float { return n <= 2 ? 4.1f : 2.0f; }};

// ---- p8s8 (modulep8s8.cpp, 8 HP) ---------------------------------------
inline const ModuleLayout kP8S8 = {"p8s8", 8,
    [](char t) -> unsigned { return (t == 'P' || t == 'L' || t == 'S') ? 8u : 0u; },
    [](char t, unsigned n) -> Pos {
        float x = (((n - 1) % 4) * 1.985f + 1.05f) * (8.f / 8);
        float y;
        if (t == 'P' || t == 'L') y = ((n - 1) / 4) * 7.87f + 8.08f;
        else                      y = ((n - 1) / 4) * 2.82f + 19.45f;
        return {x, y};
    },
    [](char t, unsigned) -> float { return t == 'S' ? 2.7f : 1.99f; }};

// ---- b32 (moduleb32.cpp, 10 HP) ----------------------------------------
inline const ModuleLayout kB32 = {"b32", 10,
    [](char t) -> unsigned { return (t == 'B' || t == 'L') ? 32u : 0u; },
    [](char, unsigned n) -> Pos {
        unsigned row = (n - 1) / 4, column = (n - 1) % 4;
        return {column * 2.50f + 1.258f, row * 2.767f + 2.99f};
    },
    [](char, unsigned) -> float { return 2.7f; }};

// ---- e4 (modulee4.cpp, 6 HP) -------------------------------------------
inline const ModuleLayout kE4 = {"e4", 6,
    [](char t) -> unsigned { return (t == 'E' || t == 'B' || t == 'L') ? 4u : 0u; },
    [](char, unsigned n) -> Pos { return {3.0f, 5.490f * (n - 1) + 4.33f}; },
    [](char t, unsigned) -> float { return (t == 'E' || t == 'B') ? 4.1f : 7.0f; }};

// ---- m4 (modulem4.cpp, 14 HP) ------------------------------------------
inline const ModuleLayout kM4 = {"m4", 14,
    [](char t) -> unsigned { return (t == 'P' || t == 'B' || t == 'L') ? 4u : 0u; },
    [](char t, unsigned n) -> Pos {
        float x = 3.498f * (n - 1) + 1.83f;
        if (t == 'P') return {x - 0.08f, 18.37f};
        return {x, 22.94f};
    },
    [](char t, unsigned) -> float { return t == 'P' ? 3.0f : 2.65f; }};

// ---- db8e (moduledb8e.cpp, 6 HP) ---------------------------------------
inline const ModuleLayout kDB8E = {"db8e", 6,
    [](char t) -> unsigned {
        if (t == 'E') return 1;
        if (t == 'B' || t == 'L') return 9;
        return 0;
    },
    [](char t, unsigned n) -> Pos {
        if (t == 'E' || n == 9) return {6.f / 2, 20.8f};
        unsigned column = (n - 1) % 2, row = (n - 1) / 2;
        return {column * 3.f + 1.5f, row * 2.97f + 7.95f};
    },
    [](char t, unsigned n) -> float {
        if (t == 'E' || (t == 'B' && n == 9)) return 4.1f;
        if (t == 'L' && n == 9) return 7.0f;
        if (t == 'B' || t == 'L') return 2.7f;
        return 20.0f;
    }};

// ---- bling (blind panel, 1 HP, no registers; no Forge layout class) -----
inline const ModuleLayout kBling = {"bling", 1,
    [](char) -> unsigned { return 0u; },
    [](char, unsigned) -> Pos { return {0.5f, 12.65f}; },
    [](char, unsigned) -> float { return 0.f; }};

inline const ModuleLayout kModules[14] = {
    kMaster, kMaster18, kX7, kG8, kP2B8, kP4B2, kP10,
    kS10, kP8S8, kB32, kE4, kM4, kDB8E, kBling};

inline const ModuleLayout* find(const char* name) {
    for (const auto& m : kModules)
        if (!std::strcmp(m.name, name)) return &m;
    return nullptr;
}

} // namespace layout
} // namespace droid
