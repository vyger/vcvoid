#include "harness.hpp"
#include "src/registers.hpp"
using namespace droid;

static RegId rid(const std::string& s) {
    auto r = parseRegisterName(s);
    CHECK(r.has_value());
    return r.value_or(RegId{});
}

TEST(regid_parse) {
    RegId r = rid("P1.2");
    CHECK(r.type == 'P' && r.ctrl == 1 && r.num == 2);
    r = rid("I3");  CHECK(r.type == 'I' && r.ctrl == 0 && r.num == 3);
    r = rid("G2.7"); CHECK(r.type == 'G' && r.ctrl == 2 && r.num == 7);
    r = rid("R17"); CHECK(r.type == 'R' && r.num == 17);
    r = rid("X1");  CHECK(r.type == 'X' && r.num == 1);
    CHECK(!parseRegisterName("_CABLE").has_value());
    CHECK(!parseRegisterName("hz").has_value());
    CHECK(!parseRegisterName("Q1").has_value());
    CHECK(!parseRegisterName("I").has_value());
    CHECK(!parseRegisterName("P1.").has_value());
    CHECK(toString(rid("P1.2")) == "P1.2");
    CHECK(toString(rid("O4")) == "O4");
}

TEST(regid_parse_lowercase) {
    // F0c: the Forge uppercases the captured type letter, so lowercase
    // register names in patches must parse identically to uppercase ones.
    RegId r = rid("p1.2");
    CHECK(r.type == 'P' && r.ctrl == 1 && r.num == 2);
    r = rid("i3");  CHECK(r.type == 'I' && r.ctrl == 0 && r.num == 3);
    r = rid("g2.7"); CHECK(r.type == 'G' && r.ctrl == 2 && r.num == 7);
    r = rid("o4");  CHECK(r.type == 'O' && r.num == 4);
    CHECK(!parseRegisterName("q1").has_value());   // still invalid lowercase
    CHECK(toString(rid("p1.2")) == "P1.2");        // toString round-trips uppercase
    CHECK(toString(rid("o4")) == "O4");
    CHECK(parseFaderName("f3").has_value());
    CHECK(parseFaderName("f3").value() == 3);
}

TEST(regid_canonicalize_g8) {
    RegId g5 = canonicalize(rid("G5"), MasterType::Master16);
    CHECK(g5.ctrl == 1 && g5.num == 5);              // G5 == G1.5 on MASTER
    RegId g9 = canonicalize(rid("G9"), MasterType::Master16);
    CHECK(g9.ctrl == 0 && g9.num == 9);              // X7 gate: unchanged
    CHECK(pack(canonicalize(rid("G1.5"), MasterType::Master16)) == pack(g5));
}

TEST(registerfile_basics) {
    RegisterFile rf;
    CHECK_NEAR(rf.get(rid("O1")), 0.0, 1e-9);        // unset reads 0
    rf.set(rid("O1"), 0.25f);
    CHECK_NEAR(rf.get(rid("O1")), 0.25, 1e-7);
    rf.set(rid("O1"), 1.7f);                         // O clamps to ±1.0 (±10 V)
    CHECK_NEAR(rf.get(rid("O1")), 1.0, 1e-7);
    rf.set(rid("O1"), -2.0f);
    CHECK_NEAR(rf.get(rid("O1")), -1.0, 1e-7);
    rf.set(rid("P1.1"), 0.5f);                       // non-O: unclamped storage
    CHECK_NEAR(rf.get(rid("P1.1")), 0.5, 1e-7);
}

TEST(registerfile_normalization) {
    RegisterFile rf;
    rf.set(rid("N2"), 0.3f);
    CHECK_NEAR(rf.get(rid("I2")), 0.3, 1e-7);        // unpatched I2 reads N2
    rf.setInputPatched(2, true);
    rf.set(rid("I2"), 0.9f);
    CHECK_NEAR(rf.get(rid("I2")), 0.9, 1e-7);        // patched I2 reads I2
    rf.setInputPatched(2, false);
    CHECK_NEAR(rf.get(rid("I2")), 0.3, 1e-7);        // back to N2
}
