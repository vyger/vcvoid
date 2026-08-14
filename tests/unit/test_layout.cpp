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

// ---- register-label chips (issue #26) -----------------------------------

TEST(layout_label_rect_matches_forge_formula) {
    using namespace droid::layout;
    const ModuleLayout* m = find("master");
    CHECK(m != nullptr);
    if (!m) return;
    // Forge paintRegisterLabel: the chip is labelWidth wide and LABEL_HEIGHT
    // tall, centred on the control and offset from the BOTTOM of the control's
    // drawn rect — which is 2/3 of the nominal register size (registerRect).
    Rect r = labelRect(*m, 'O', 1);
    Pos c = m->pos('O', 1);
    float drawn = m->size('O', 1) * 2.f / 3.f;      // aspect 0 for a jack: square
    CHECK_NEAR(r.w, 1.93f, 1e-4f);                  // RACV_JACK_LABEL_WIDTH
    CHECK_NEAR(r.h, 0.70f, 1e-4f);                  // RACV_LABEL_HEIGHT
    CHECK_NEAR(r.x, c.x - 1.93f / 2.f, 1e-4f);      // centred on the jack
    CHECK_NEAR(r.y, c.y + drawn / 2.f - 2.45f, 1e-4f);
    // Negative labelDistance puts the chip ABOVE the jack on every module.
    CHECK(r.y + r.h < c.y);
}

TEST(layout_label_rect_uses_aspect_for_non_square_controls) {
    using namespace droid::layout;
    // The M4's faders are tall (rectAspect 2.2), so their chip clears a much
    // taller control than a square one would give.
    const ModuleLayout* m4 = find("m4");
    CHECK(m4 != nullptr);
    if (!m4) return;
    Rect r = labelRect(*m4, 'P', 1);
    Pos c = m4->pos('P', 1);
    float drawn = m4->size('P', 1) * 2.f / 3.f;
    CHECK_NEAR(r.y, c.y + drawn * 2.2f / 2.f - 15.34f, 1e-4f);
}

TEST(layout_label_rect_honours_explicit_positions) {
    using namespace droid::layout;
    // The P8S8 is the only module with labelPosition overrides: its slider
    // chips are staggered, and an explicit position is absolute, not an offset.
    const ModuleLayout* p = find("p8s8");
    CHECK(p != nullptr);
    if (!p) return;
    Rect r1 = labelRect(*p, 'P', 1);
    CHECK_NEAR(r1.x, 0.05f, 1e-4f);
    CHECK_NEAR(r1.y, 3.90f, 1e-4f);
    Rect r5 = labelRect(*p, 'P', 5);          // second row: +7.80 HP
    CHECK_NEAR(r5.y, 3.90f + 7.80f, 1e-4f);
    // Its switches have no explicit position, so they fall back to the offset.
    Rect s1 = labelRect(*p, 'S', 1);
    CHECK_NEAR(s1.x, p->pos('S', 1).x - 2.00f / 2.f, 1e-4f);
}

TEST(layout_label_chips_stay_on_the_panel) {
    using namespace droid::layout;
    // A chip that fell off the panel edge would be clipped away by Rack. The
    // Forge's own numbers keep every chip inside its module, so a transcription
    // slip that pushed one out of bounds is a bug worth catching here.
    const float panelHP = kPanelMm / kHPmm;
    for (const auto& m : kModules) {
        for (char t : {'I','N','O','G','B','L','P','E','S','R','X'}) {
            for (unsigned n = 1; n <= m.num(t); n++) {
                Rect r = labelRect(m, t, n);
                CHECK(r.x >= -0.5f);
                CHECK(r.x + r.w <= m.hp + 0.5f);
                CHECK(r.y >= 0.f);
                CHECK(r.y + r.h <= panelHP);
            }
        }
    }
}
