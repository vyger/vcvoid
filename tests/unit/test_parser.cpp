#include "harness.hpp"
#include "src/parser.hpp"
using namespace droid;

static const ParamLine& firstParam(const ParseResult& pr) {
    static ParamLine dummy;
    if (pr.sections.empty() || pr.sections[0].params.empty()) { CHECK(false); return dummy; }
    return pr.sections[0].params[0];
}

TEST(parser_sections_and_comments) {
    auto pr = parsePatch("# top comment\n[lfo]\n  hz = 4 # trailing\n\n[contour]\n  gate = I1\n");
    CHECK(pr.errors.empty());
    CHECK(pr.sections.size() == 2);
    CHECK(pr.sections[0].name == "lfo" && pr.sections[0].line == 2);
    CHECK(pr.sections[1].name == "contour");
    const ParamLine& hz = pr.sections[0].params[0];
    CHECK(hz.name == "hz" && hz.line == 3);
    CHECK(hz.simple && hz.a.kind == Atom::Kind::Number);
    CHECK_NEAR(hz.a.number, 4.0, 1e-7);
    CHECK(!hz.b.fromSource && !hz.c.fromSource);   // implicit B=1, C=0
}

TEST(parser_abc_forms) {
    auto p1 = firstParam(parsePatch("[copy]\ninput = I1 * 0.5 + 0.1\n"));
    CHECK(p1.a.kind == Atom::Kind::Register && p1.a.reg.type == 'I');
    CHECK_NEAR(p1.b.number, 0.5, 1e-7);
    CHECK_NEAR(p1.c.number, 0.1, 1e-7);
    CHECK(!p1.simple && p1.b.fromSource && p1.c.fromSource);

    auto p2 = firstParam(parsePatch("[copy]\ninput = I1 / 12\n"));   // division shorthand
    CHECK_NEAR(p2.b.number, 1.0 / 12.0, 1e-7);

    auto p4 = firstParam(parsePatch("[copy]\ninput = I1 - I2\n"));   // reg subtraction: A=I2,B=-1,C=I1
    CHECK(p4.a.reg.num == 2);
    CHECK_NEAR(p4.b.number, -1.0, 1e-7);
    CHECK(p4.c.kind == Atom::Kind::Register && p4.c.reg.num == 1);
    // The Forge's form6 ("A - B") emits a literal AtomNumber(-1) that
    // countUniqueConstants counts, so our synthesized -1 must be fromSource=true.
    CHECK(p4.b.fromSource);

    // Unary negation glued to a non-number atom is REJECTED, matching the Forge
    // (droidcheck: `-P1.1` -> "Invalid (garbled) value"). Verified in every
    // position; a signed numeric literal (`-2`) is still fine (see below).
    CHECK(!parsePatch("[copy]\ninput = -P1.1\n").errors.empty());       // leading
    CHECK(!parsePatch("[copy]\ninput = I1 * -P1.1\n").errors.empty());  // after *
    CHECK(!parsePatch("[copy]\ninput = 5 + -P1.1\n").errors.empty());   // after +
    CHECK(!parsePatch("[copy]\ninput = -_CAB\n").errors.empty());       // -cable

    auto p5 = firstParam(parsePatch("[copy]\ninput = 10V - I1\n"));  // manual copy example
    CHECK(p5.a.kind == Atom::Kind::Register);
    CHECK_NEAR(p5.b.number, -1.0, 1e-7);
    CHECK_NEAR(p5.c.number, 1.0, 1e-7);                              // 10V == 1.0

    auto p6 = firstParam(parsePatch("[lfo]\nsquare = _TRIG\n"));     // cable atom
    CHECK(p6.simple && p6.a.kind == Atom::Kind::Cable && p6.a.cable == "_TRIG");
}

TEST(parser_errors) {
    CHECK(!parsePatch("[copy]\ninput = -P1.1 * I1\n").errors.empty());   // -REG rejected (garbled)
    CHECK(!parsePatch("[copy]\ninput = I1 / P1.1\n").errors.empty());    // dynamic division
    CHECK(!parsePatch("[copy]\ninput = I1 * I2 - I3\n").errors.empty()); // needs 2nd multiplication
    CHECK(!parsePatch("[copy]\ninput = .5\n").errors.empty());           // bad number
    CHECK(!parsePatch("garbage line\n").errors.empty());                 // not a section/param
    auto pr = parsePatch("[copy]\ninput = I1 +\n");                      // dangling op
    CHECK(!pr.errors.empty());
    CHECK(pr.errors[0].line == 2);
}

TEST(parser_strip) {
    CHECK(stripPatch("[copy] # c\n  input = I1  \n") == "[copy]\ninput=I1\n");
}

TEST(parser_strip_preserves_text_whitespace) {
    // The 64000-byte estimator must keep whitespace and '#' *inside* a quoted
    // text value (text params are supported — interned in parsePatch), so
    // text-heavy patches are not undercounted.
    CHECK(stripPatch("[display]\n header = \"Loud  Volume\"  # c\n")
          == "[display]\nheader=\"Loud  Volume\"\n");
    CHECK(stripPatch("[display]\n header = \"a#b\"\n")
          == "[display]\nheader=\"a#b\"\n");
}

TEST(parser_section_name_not_trimmed) {
    // Section names keep whatever is between the brackets (no trimming). The
    // Forge's regex is [a-zA-Z0-9]+ and rejects `[ lfo ]`; the untrimmed " lfo "
    // then fails circuit resolution in the loader — both REJECT (parity).
    auto pr = parsePatch("[ lfo ]\n hz = 5\n");
    CHECK(pr.sections.size() == 1);
    CHECK(pr.sections[0].name == " lfo ");
}

TEST(parser_text_params) {
    auto pr = parsePatch(
        "[display]\n"
        "    header = \"Volume\"\n"
        "    text = \"Loud\"\n"
        "[display]\n"
        "    header = \"Volume\"\n");   // duplicate string -> same number
    CHECK(pr.errors.empty());
    CHECK(pr.texts.size() == 3);            // slot 0 reserved + 2 unique texts
    CHECK(pr.texts[1] == "Volume");
    CHECK(pr.texts[2] == "Loud");
    // atoms carry the 1-based intern number as the A-atom (a plain number).
    const ParamLine& h1 = pr.sections[0].params[0];
    CHECK(h1.name == "header" && h1.simple && h1.a.kind == Atom::Kind::Number);
    CHECK_NEAR(h1.a.number, 1.0, 1e-7);
    CHECK(h1.a.fromSource);
    const ParamLine& t1 = pr.sections[0].params[1];
    CHECK(t1.name == "text");
    CHECK_NEAR(t1.a.number, 2.0, 1e-7);
    const ParamLine& h2 = pr.sections[1].params[0];    // duplicate -> reuse 1
    CHECK_NEAR(h2.a.number, 1.0, 1e-7);
}

TEST(parser_text_input_math) {
    // A text is otherwise an ordinary number: input math (B/C) still applies.
    auto p = firstParam(parsePatch("[display]\n    header = \"Loud\" * 2 + 1\n"));
    CHECK_NEAR(p.a.number, 1.0, 1e-7);     // first text -> number 1
    CHECK_NEAR(p.b.number, 2.0, 1e-7);
    CHECK_NEAR(p.c.number, 1.0, 1e-7);
    CHECK(!p.simple);
}

TEST(parser_text_hash_is_literal) {
    // A '#' inside quotes is text, not a comment (Forge parity: droidcheck
    // accepts `input = "a#b"`).
    auto pr = parsePatch("[display]\n    header = \"a#b\"\n");
    CHECK(pr.errors.empty());
    CHECK(pr.texts.size() == 2);
    CHECK(pr.texts[1] == "a#b");
}

TEST(parser_text_unterminated) {
    auto pr = parsePatch("[display]\n    header = \"oops\n");
    CHECK(!pr.errors.empty());               // unterminated quote is a parse error
}

// --- Extra edge cases beyond the brief ---------------------------------------

TEST(parser_extra_edge_cases) {
    // signed number after '+': A=I1, B=1, C=-0.5
    auto e1 = firstParam(parsePatch("[copy]\ninput = I1 + -0.5\n"));
    CHECK(e1.a.kind == Atom::Kind::Register && e1.a.reg.type == 'I');
    CHECK_NEAR(e1.b.number, 1.0, 1e-7);
    CHECK_NEAR(e1.c.number, -0.5, 1e-7);
    CHECK(!e1.simple);

    // number minus register: A=I1, B=-1, C=5  (same as 10V - I1)
    auto e2 = firstParam(parsePatch("[copy]\ninput = 5 - I1\n"));
    CHECK(e2.a.kind == Atom::Kind::Register && e2.a.reg.num == 1);
    CHECK_NEAR(e2.b.number, -1.0, 1e-7);
    CHECK_NEAR(e2.c.number, 5.0, 1e-7);

    // negative number as B: A=I1, B=-2, C=0.  B is a real source number.
    auto e3 = firstParam(parsePatch("[copy]\ninput = I1 * -2\n"));
    CHECK(e3.a.kind == Atom::Kind::Register);
    CHECK_NEAR(e3.b.number, -2.0, 1e-7);
    CHECK(e3.b.fromSource);

    // cable minus cable: A=_B, B=-1, C=_A
    auto e4 = firstParam(parsePatch("[copy]\ninput = _A - _B\n"));
    CHECK(e4.a.kind == Atom::Kind::Cable && e4.a.cable == "_B");
    CHECK_NEAR(e4.b.number, -1.0, 1e-7);
    CHECK(e4.c.kind == Atom::Kind::Cable && e4.c.cable == "_A");

    // mixed-case parameter name is lowercased
    auto e5 = firstParam(parsePatch("[copy]\nInput = I1\n"));
    CHECK(e5.name == "input");
    CHECK(e5.simple);
}
