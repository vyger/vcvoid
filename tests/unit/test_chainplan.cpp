#include "harness.hpp"
#include "ChainPlan.hpp"

// "Add missing controllers" (issue #69), model half. The planner decides which
// modules the action creates and where they go; these pin the four things that
// keep the action safe to offer from a context menu: it only inserts, it is
// all-or-nothing, it tolerates surplus hardware, and non-controllers (G8, X7,
// the bling pass-through) never shift the controller numbering.

using namespace vcvoid::chainplan;
using namespace droid::chain;

static std::vector<std::string> decl(std::initializer_list<const char*> l) {
    return std::vector<std::string>(l.begin(), l.end());
}
static std::vector<ModelId> phys(std::initializer_list<ModelId> l) {
    return std::vector<ModelId>(l);
}

// --- nothing to do -------------------------------------------------------

TEST(chainplan_a_satisfied_chain_plans_nothing) {
    Plan p = compute(decl({"p2b8", "m4"}), false, phys({MP2B8, MM4}));
    CHECK(p.blocker.empty());
    CHECK(p.empty());
    CHECK(p.summary() == "");
}

TEST(chainplan_a_patch_declaring_no_controllers_plans_nothing) {
    Plan p = compute({}, false, phys({MP2B8}));
    CHECK(p.blocker.empty() && p.empty());
}

// --- inserting -----------------------------------------------------------

TEST(chainplan_an_empty_chain_gets_every_declared_controller_after_the_master) {
    Plan p = compute(decl({"p2b8", "b32", "m4", "m4"}), false, {});
    CHECK(p.blocker.empty());
    CHECK(p.inserts.size() == 4);
    CHECK(p.summary() == "p2b8, b32, m4, m4");
    // The physical list is empty, so the anchor is the master (-1) for all of
    // them; the executor chains consecutive inserts off each other.
    for (auto& i : p.inserts) CHECK(i.afterSlot == -1);
}

TEST(chainplan_a_partial_prefix_appends_only_the_missing_tail) {
    Plan p = compute(decl({"p2b8", "m4", "b32"}), false, phys({MP2B8}));
    CHECK(p.blocker.empty());
    CHECK(p.inserts.size() == 2);
    CHECK(p.summary() == "m4, b32");
    // Both append after the last chain member (slot 0, the p2b8).
    CHECK(p.inserts[0].afterSlot == 0);
    CHECK(p.inserts[1].afterSlot == 0);
}

TEST(chainplan_surplus_physical_modules_are_tolerated) {
    // validateChain has prefix semantics: a longer chain than the patch
    // declares is valid, so there is nothing to plan.
    Plan p = compute(decl({"p2b8"}), false, phys({MP2B8, MM4, MB32}));
    CHECK(p.blocker.empty() && p.empty());
}

// --- non-controllers do not take a controller number ----------------------

TEST(chainplan_g8s_interleaved_do_not_shift_the_numbering) {
    // master | g8 | p2b8 | g8 | m4 satisfies a patch declaring p2b8 then m4.
    Plan p = compute(decl({"p2b8", "m4"}), false, phys({MG8, MP2B8, MG8, MM4}));
    CHECK(p.blocker.empty() && p.empty());
}

TEST(chainplan_appending_lands_after_a_trailing_non_controller) {
    // The bling (droid::chain::None) is a 1 HP pass-through: it holds a slot in
    // the row but takes no controller number, so the new m4 is planned after
    // it rather than shoved into the middle of the chain.
    Plan p = compute(decl({"p2b8", "m4"}), false, phys({MP2B8, None}));
    CHECK(p.blocker.empty());
    CHECK(p.inserts.size() == 1);
    CHECK(p.inserts[0].model == "m4");
    CHECK(p.inserts[0].afterSlot == 1);   // the last slot, i.e. the bling
}

// --- the x7 ---------------------------------------------------------------

TEST(chainplan_a_wanted_x7_is_inserted_at_the_head) {
    Plan p = compute(decl({"p2b8", "m4"}), true, phys({MP2B8}));
    CHECK(p.blocker.empty());
    CHECK(p.inserts.size() == 2);
    CHECK(p.summary() == "x7, m4");
    CHECK(p.inserts[0].afterSlot == -1);   // straight off the master
    CHECK(p.inserts[1].afterSlot == 0);    // …and the m4 after the chain's end
}

TEST(chainplan_a_wanted_x7_already_at_the_head_plans_nothing) {
    Plan p = compute(decl({"p2b8"}), true, phys({MX7, MP2B8}));
    CHECK(p.blocker.empty() && p.empty());
}

TEST(chainplan_a_misplaced_x7_blocks) {
    Plan p = compute(decl({"p2b8"}), true, phys({MP2B8, MX7}));
    CHECK(p.blocker == "x7 is attached but not first in the chain");
    CHECK(p.empty());   // all-or-nothing: not even the p2b8 half is planned
}

TEST(chainplan_an_unwanted_x7_is_left_alone) {
    // An X7 nobody asked for is harmless, and it takes no controller number,
    // so the chain still satisfies the patch.
    Plan p = compute(decl({"p2b8"}), false, phys({MX7, MP2B8}));
    CHECK(p.blocker.empty() && p.empty());
    // …even misplaced: not this action's business to police it.
    Plan q = compute(decl({"p2b8"}), false, phys({MP2B8, MX7}));
    CHECK(q.blocker.empty() && q.empty());
}

// --- blocked --------------------------------------------------------------

TEST(chainplan_a_wrong_type_blocks_and_plans_nothing) {
    Plan p = compute(decl({"p2b8", "m4"}), false, phys({MP2B8, MB32}));
    CHECK(p.blocker == "controller 2 is a b32, patch declares m4");
    CHECK(p.empty());
}

TEST(chainplan_a_wrong_type_blocks_even_when_a_tail_is_also_missing) {
    // Inserting the missing b32 at the end would leave the chain just as
    // wrong, so nothing is planned at all.
    Plan p = compute(decl({"m4", "b32"}), false, phys({MP2B8}));
    CHECK(p.blocker == "controller 1 is a p2b8, patch declares m4");
    CHECK(p.empty());
}

TEST(chainplan_a_block_discards_an_already_planned_x7) {
    Plan p = compute(decl({"m4"}), true, phys({MP2B8}));
    CHECK(p.blocker == "controller 1 is a p2b8, patch declares m4");
    CHECK(p.empty());
}
