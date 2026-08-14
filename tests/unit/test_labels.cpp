#include "harness.hpp"
#include "src/labels.hpp"
using namespace droid;

// Every expectation here is Forge behaviour (parser/patchparser.cpp), including
// the ones that look like bugs — see test_labels_section_header_style.

TEST(labels_basic) {
    auto l = parseRegisterLabels(
        "# My patch\n"
        "#\n"
        "#   O1: [CLK] master clock, 8 Hz square\n"
        "#   I2: external reset\n"
        "[lfo]\n"
        "  hz = 4\n");
    CHECK(l.title == "My patch");
    CHECK(l.labels.size() == 2);

    const RegisterLabel* o1 = l.find('O', 0, 0, 1);
    CHECK(o1 != nullptr);
    if (o1) {
        CHECK(o1->shorthand == "CLK");
        CHECK(o1->text == "master clock, 8 Hz square");
    }
    const RegisterLabel* i2 = l.find('I', 0, 0, 2);
    CHECK(i2 != nullptr);
    if (i2) {
        CHECK(i2->shorthand.empty());        // no [SHORT] given
        CHECK(i2->text == "external reset");
    }
}

TEST(labels_controller_and_gate_numbering) {
    auto l = parseRegisterLabels(
        "# T\n"
        "# P2.4: filter cutoff\n"
        "# B1.3: mute\n"
        "# L1.3: lit while muted\n"
        "# G3: kick\n"          // old style -> first G8
        "# G2.5: snare\n"       // G8 #2, gate 5
        "# R5: clock LED\n");
    const RegisterLabel* p = l.find('P', 2, 0, 4);
    CHECK(p != nullptr && p->text == "filter cutoff");
    CHECK(l.find('B', 1, 0, 3) != nullptr);
    const RegisterLabel* led = l.find('L', 1, 0, 3);
    CHECK(led != nullptr && led->text == "lit while muted");
    CHECK(l.find('G', 0, 1, 3) != nullptr);   // G3 rewritten to G1.3
    const RegisterLabel* g25 = l.find('G', 0, 2, 5);
    CHECK(g25 != nullptr && g25->text == "snare");
    CHECK(l.find('R', 0, 0, 5) != nullptr);
}

TEST(labels_lowercase_register_letter) {
    auto l = parseRegisterLabels("# T\n# o3: bass\n");
    CHECK(l.find('O', 0, 0, 3) != nullptr);   // the Forge upper-cases the letter
}

// The header style used by patches/uat-core.ini: a title line, then indented
// label lines. Blank lines and a controller declaration must NOT end the
// description, or the labels below them are lost.
TEST(labels_survive_blanks_and_controllers) {
    auto l = parseRegisterLabels(
        "# UAT core\n"
        "# Rack setup: [master | p2b8]\n"
        "#\n"
        "# What to look for:\n"
        "#   O1: sine, 1..10 Hz\n"
        "[p2b8]\n"
        "\n"
        "#   O2: euclid triggers\n"
        "[lfo]\n"
        "  hz = 2\n"
        "#   O3: never seen, we are past the header\n");
    CHECK(l.find('O', 0, 0, 1) != nullptr);
    CHECK(l.find('O', 0, 0, 2) != nullptr);
    CHECK(l.find('O', 0, 0, 3) == nullptr);   // after the first real circuit
    CHECK(l.labels.size() == 2);
}

// The header style used by patches/mine-01 / mine-02: the file opens with a
// "# -----" separator, so everything inside is a SECTION header, and the
// register lines are section prose. The Forge shows no labels for these.
TEST(labels_section_header_style) {
    auto l = parseRegisterLabels(
        "# -----------------------------------------\n"
        "# mine-02-trigseq-drums\n"
        "#\n"
        "# OUTPUTS\n"
        "#   O1: master clock\n"
        "#   O2: kick\n"
        "# -----------------------------------------\n"
        "[lfo]\n"
        "  hz = 8\n");
    CHECK(l.empty());
}

TEST(labels_not_labels) {
    auto l = parseRegisterLabels(
        "# T\n"
        "# R1 (input-1 LED on the matrix): flashes at 3 Hz\n"  // no ':' after R1
        "# 1) Pot input math: hz = P1.1\n"                     // does not start with a letter
        "# OUTPUTS:\n"                                          // meta comment
        "# O0: zero is not a register number\n");               // [1-9] required
    CHECK(l.empty());
}

TEST(labels_meta_comment_is_not_a_title) {
    auto l = parseRegisterLabels("# LABELS: master=18; firmware=blue-7\n# O1: clock\n");
    CHECK(l.title.empty());
    CHECK(l.find('O', 0, 0, 1) != nullptr);
}

TEST(labels_disabled_circuit_ends_the_header) {
    auto l = parseRegisterLabels(
        "# T\n"
        "# O1: live\n"
        "# [lfo]\n"          // a commented-out circuit still closes the header
        "# O2: ignored\n");
    CHECK(l.find('O', 0, 0, 1) != nullptr);
    CHECK(l.find('O', 0, 0, 2) == nullptr);
}

TEST(labels_duplicate_replaces_in_place) {
    auto l = parseRegisterLabels("# T\n# O1: first\n# I1: other\n# O1: second\n");
    CHECK(l.labels.size() == 2);           // O1 replaced in place, not appended
    const RegisterLabel* o1 = l.find('O', 0, 0, 1);
    CHECK(o1 != nullptr && o1->text == "second");
}

TEST(labels_multiline_label_keeps_only_the_first_line) {
    auto l = parseRegisterLabels(
        "# T\n"
        "#   O4: envelope retriggering at 2 Hz while I1 is\n"
        "#       unpatched; patching a clock must take over\n");
    const RegisterLabel* o4 = l.find('O', 0, 0, 4);
    CHECK(o4 != nullptr);
    if (o4) CHECK(o4->text == "envelope retriggering at 2 Hz while I1 is");
    CHECK(l.labels.size() == 1);
}

TEST(labels_shorthand_edge_cases) {
    auto a = parseRegisterLabels("# T\n# O1: [ONLY]\n");           // shorthand, no text
    const RegisterLabel* o1 = a.find('O', 0, 0, 1);
    CHECK(o1 != nullptr && o1->shorthand == "ONLY" && o1->text.empty());

    auto b = parseRegisterLabels("# T\n# O2: []empty brackets\n"); // [] is not a shorthand
    const RegisterLabel* o2 = b.find('O', 0, 0, 2);
    CHECK(o2 != nullptr && o2->shorthand.empty() && o2->text == "[]empty brackets");

    auto c = parseRegisterLabels("# T\n# O3:no space after colon\n");
    const RegisterLabel* o3 = c.find('O', 0, 0, 3);
    CHECK(o3 != nullptr && o3->text == "no space after colon");
}

TEST(labels_no_title_when_patch_starts_blank) {
    auto l = parseRegisterLabels("\n# O1: clock\n");
    CHECK(l.title.empty());                   // blank first line -> straight to description
    CHECK(l.find('O', 0, 0, 1) != nullptr);
}

TEST(labels_crlf_and_no_trailing_newline) {
    auto l = parseRegisterLabels("# T\r\n#   O1: [CLK] clock\r\n# O2: last");
    const RegisterLabel* o1 = l.find('O', 0, 0, 1);
    CHECK(o1 != nullptr && o1->shorthand == "CLK" && o1->text == "clock");
    CHECK(l.find('O', 0, 0, 2) != nullptr);
}

TEST(labels_empty_and_commentless_patches) {
    CHECK(parseRegisterLabels("").empty());
    CHECK(parseRegisterLabels("[lfo]\n  hz = 4\n").empty());
}
