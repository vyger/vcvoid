#pragma once
// "Add missing controllers" (issue #69) — the MODEL half.
//
// Pure C++, NO Rack includes, so it links into the headless unit-test build as
// well as the Rack widget (same arrangement as MasterStatus.hpp). Everything
// that decides WHAT to create lives here as a pure function of two plain lists;
// the widget side (MasterBase.hpp) only executes the answer.
//
// The question it answers: the loaded patch declares a chain of controllers
// (and possibly an X7); the rack holds a physical chain to the master's right.
// Which modules must be CREATED, and where, so that the physical chain
// satisfies the patch?
//
// Three rules shape it, and they are what make the action safe to offer:
//
//   1. It only ever INSERTS. Nothing is removed, nothing is reordered, nothing
//      is retyped. A user's rack is theirs; the worst this action can do is
//      leave modules they did not want, which one undo takes back.
//   2. It is ALL-OR-NOTHING. A chain whose existing controllers contradict the
//      patch (controller 2 is a b32, the patch declares an m4) cannot be fixed
//      by inserting, so the plan carries a `blocker` and NO inserts — rather
//      than half-fixing a chain into a different wrong shape.
//   3. Surplus is fine. droid::chain::validateChain has prefix semantics: a
//      chain longer than the patch declares is valid, so extra physical
//      modules are never a reason to refuse.
//
// G8s are deliberately out of scope: a patch that uses G registers with no G8
// attached runs fine (manual/hardware.md §7), so a missing G8 is not a chain
// error and there is nothing to fix.
#include "src/chain.hpp"
#include <string>
#include <vector>

namespace vcvoid {
namespace chainplan {

// One module to create, and where to put it.
//
// `afterSlot` names the ANCHOR the new module is placed immediately to the
// right of: -1 means the master itself, otherwise it is an index into the
// `physical` slot list the plan was computed from. Inserts are applied in
// order, and consecutive inserts that share an anchor chain off each other —
// the second lands to the right of the first, not on top of it. That is the
// whole placement contract; see DroidMasterBaseWidget::addMissingControllers.
struct Insert {
    int afterSlot = -1;
    std::string model;   // Rack model slug == droid::chain::modelName ("p2b8", "x7", …)
};

struct Plan {
    std::vector<Insert> inserts;
    // Non-empty: the chain cannot be fixed by inserting. `inserts` is then
    // always empty (rule 2 above).
    std::string blocker;

    bool empty() const { return inserts.empty(); }
    // "p2b8, m4" — the models to be created, in creation order. What the menu
    // item and the tooltip name, so the user knows what they are agreeing to.
    std::string summary() const {
        std::string out;
        for (size_t i = 0; i < inserts.size(); i++) out += (i ? ", " : "") + inserts[i].model;
        return out;
    }
};

// declared  — the patch's controller declarations, in order, as
//             droid::Engine::declaredControllers() gives them (never "x7").
// wantX7    — the patch needs an X7: it declares one, or it uses MIDI on a
//             master whose only route to MIDI is an X7 (see MasterBase.hpp).
// physical  — the chain to the master's right, nearest-master FIRST, one entry
//             per chain module. droid::chain::None entries are pass-throughs
//             (the 1 HP bling): they hold a slot in the row — so appending
//             after the end of the chain lands after them — but they never
//             take a controller number.
inline Plan compute(const std::vector<std::string>& declared, bool wantX7,
                    const std::vector<droid::chain::ModelId>& physical) {
    Plan p;
    // --- the X7 -----------------------------------------------------------
    // It is not a controller: it takes no controller number and the master
    // requires it FIRST in the chain (manual/hardware.md §8). So it is planned
    // separately, and only when the patch actually wants one — an X7 attached
    // to a patch that ignores it is harmless and is left alone.
    int x7At = -1;
    for (size_t i = 0; i < physical.size(); i++)
        if (physical[i] == droid::chain::MX7) { x7At = (int)i; break; }
    if (wantX7) {
        if (x7At > 0) {
            // Fixing this means MOVING a module, which rule 1 forbids.
            p.blocker = "x7 is attached but not first in the chain";
            return p;
        }
        if (x7At < 0) p.inserts.push_back({-1, "x7"});
    }

    // --- the controllers ---------------------------------------------------
    // Walk the physical chain counting only the modules that take a controller
    // number (droid::chain::isControllerModel — so G8s, X7s and pass-throughs
    // never shift the numbering) and match it against the declarations. Every
    // declaration past the end of the physical chain is a module to create,
    // appended after the last chain member in declaration order.
    const int lastSlot = (int)physical.size() - 1;
    size_t di = 0;
    for (size_t s = 0; s < physical.size() && di < declared.size(); s++) {
        if (!droid::chain::isControllerModel(physical[s])) continue;
        std::string have = droid::chain::modelName(physical[s]);
        if (have != declared[di]) {
            p.inserts.clear();
            p.blocker = "controller " + std::to_string(di + 1) + " is a " + have +
                        ", patch declares " + declared[di];
            return p;
        }
        di++;
    }
    for (; di < declared.size(); di++)
        p.inserts.push_back({lastSlot, declared[di]});
    return p;
}

} // namespace chainplan
} // namespace vcvoid
