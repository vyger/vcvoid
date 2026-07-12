// Layer-4 geometry sanity on Layout.hpp: every control inside the panel,
// no accidental duplicates, sane minimum spacing. Runs headless in `make test`.
#include "harness.hpp"
#include "../../plugin/src/Layout.hpp"
#include <cmath>

using droid::layout::kModules;
using droid::layout::Pos;

static const char kTypes[] = {'I','N','O','G','B','L','P','E','S','R','X'};

TEST(layout_bounds) {
    const float panelHp = droid::layout::kPanelMm / droid::layout::kHPmm; // 25.295
    for (const auto& m : kModules) {
        for (char t : kTypes) {
            for (unsigned n = 1; n <= m.num(t); n++) {
                Pos p = m.pos(t, n);
                float raw = m.size(t, n);
                // The 'X' type (e.g. master's X1 display) is the Forge's
                // REGISTER_EXTRA. Module::registerRect() (module.cpp) applies
                // a module-wide 2/3 render scale to all register types; X is
                // exempt from strict bounds checking because Module::registerAt()
                // skips REGISTER_EXTRA ("This should not be clickable") — it's
                // non-interactive. X is the only type where raw size (11.5 HP)
                // exceeds the panel (8 HP), making the 2/3 scale discrepancy
                // visible on-panel; smaller types hide the difference.
                float s = (t == 'X') ? raw * (2.0f / 3.0f) / 2 : raw / 2;
                CHECK(p.x - s > -0.6f);          // Forge markers may kiss the edge;
                CHECK(p.x + s < m.hp + 0.6f);    // allow slight bleed, catch gross errors
                CHECK(p.y - s > 0.f);
                CHECK(p.y + s < panelHp + 0.1f);
            }
        }
    }
}

TEST(layout_no_duplicates_within_type) {
    for (const auto& m : kModules) {
        for (char t : kTypes) {
            unsigned cnt = m.num(t);
            for (unsigned a = 1; a <= cnt; a++)
                for (unsigned b = a + 1; b <= cnt; b++) {
                    // DB8E: B9/L9 is the encoder push at the encoder position;
                    // B/L 1..8 are the button grid — only same-role pairs must differ.
                    Pos pa = m.pos(t, a), pb = m.pos(t, b);
                    float d = std::hypot(pa.x - pb.x, pa.y - pb.y);
                    CHECK(d > 0.5f);   // any two same-type controls >= 0.5 HP apart
                }
        }
    }
}

// Some modules deliberately co-locate controls of DIFFERENT register types at
// one faceplate center — a lit button (B) with its LED (L), an encoder (E) that
// also acts as a button, a MASTER input jack (I) sharing its hole with the
// input-normalization register (N). These are the ONLY intentional cross-type
// co-locations; every entry was derived empirically from Layout.hpp (all are
// exactly distance 0). Type order within a pair is irrelevant (matched both
// ways). NOTE: G8/X7 gate in+out stacking is NOT here — at the layout level a
// gate is a single 'G' register; the in/out PortWidget pair (and its split
// hit-boxes) lives in the widget code (G8.cpp), invisible to this test.
struct Coloc { const char* module; char t1; char t2; };
static const Coloc kColocExempt[] = {
    {"master", 'I', 'N'},                     // input jack shares its hole with N
    {"p2b8", 'B', 'L'}, {"p4b2", 'B', 'L'},   // lit buttons: LED under the cap
    {"p8s8", 'P', 'L'}, {"b32",  'B', 'L'},
    {"m4",   'B', 'L'},
    {"e4",   'B', 'L'}, {"e4",   'B', 'E'}, {"e4", 'L', 'E'},   // encoder = push button + LED
    {"db8e", 'B', 'L'}, {"db8e", 'B', 'E'}, {"db8e", 'L', 'E'}, // encoder push (B9/L9) at the encoder
};
static bool colocExempt(const char* module, char a, char b) {
    for (const auto& c : kColocExempt)
        if (!std::strcmp(c.module, module) &&
            ((c.t1 == a && c.t2 == b) || (c.t1 == b && c.t2 == a)))
            return true;
    return false;
}

// layout_no_duplicates_within_type only guards SAME-type spacing, so two
// controls of different types landing on the identical spot (e.g. a future
// typo stacking an unrelated jack on a pot) would slip through. This flags any
// two DIFFERENT-type controls sharing a center, except the documented
// intentional pairs above — so an accidental co-location fails loudly.
TEST(layout_no_cross_type_colocation) {
    for (const auto& m : kModules) {
        for (unsigned i = 0; i < sizeof(kTypes); i++)
            for (unsigned j = i + 1; j < sizeof(kTypes); j++) {
                char t1 = kTypes[i], t2 = kTypes[j];
                for (unsigned a = 1; a <= m.num(t1); a++)
                    for (unsigned b = 1; b <= m.num(t2); b++) {
                        Pos pa = m.pos(t1, a), pb = m.pos(t2, b);
                        float d = std::hypot(pa.x - pb.x, pa.y - pb.y);
                        if (d < 0.15f)                       // identical center
                            CHECK(colocExempt(m.name, t1, t2));
                    }
            }
    }
}

TEST(layout_interactive_minimum_size) {
    // Everything the mouse must hit is at least 1.5 HP (7.6 mm) — jacks,
    // pots, buttons, switches, encoders. LEDs (L/R) and the display (X) exempt.
    for (const auto& m : kModules) {
        for (char t : {'I','N','O','G','B','P','E','S'}) {
            for (unsigned n = 1; n <= m.num(t); n++)
                CHECK(m.size(t, n) >= 1.5f);
        }
    }
}
