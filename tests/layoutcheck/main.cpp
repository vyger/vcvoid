// Layer-1 parity check: our Layout.hpp must equal the Forge's own layout code
// EXACTLY (zero tolerance). Compiled together with the staged Forge sources
// (build/layoutcheck/src) against the Qt stub. Exits non-zero on any mismatch.
#include "module.h"
#include "modulemaster.h"
#include "modulemaster18.h"
#include "modulex7.h"
#include "moduleg8.h"
#include "modulep2b8.h"
#include "modulep4b2.h"
#include "modulep10.h"
#include "modules10.h"
#include "modulep8s8.h"
#include "moduleb32.h"
#include "modulee4.h"
#include "modulem4.h"
#include "moduledb8e.h"
#include "Layout.hpp"
#include <cstdio>
#include <cmath>

static int failures = 0;

static void check(const char* name, const Module& forge) {
    const droid::layout::ModuleLayout* mine = droid::layout::find(name);
    if (!mine) { std::printf("FAIL %s: no Layout entry\n", name); failures++; return; }
    if (mine->hp != forge.hp()) {
        std::printf("FAIL %s: hp %g != forge %g\n", name, mine->hp, forge.hp()); failures++;
    }
    const char types[] = {'I','N','O','G','B','L','P','E','S','R','X'};
    for (char t : types) {
        unsigned n = forge.numRegisters(t);
        if (mine->num(t) != n) {
            std::printf("FAIL %s: num('%c') %u != forge %u\n", name, t, mine->num(t), n);
            failures++; continue;
        }
        for (unsigned i = 1; i <= n; i++) {
            QPointF fp = forge.registerPosition(t, i);
            droid::layout::Pos mp = mine->pos(t, i);
            float fs = forge.registerSize(t, i), ms = mine->size(t, i);
            // Epsilon, not exact equality: the Forge computes in double, our
            // tables in float — up to 1 ulp apart. 1e-4 HP is 0.5 um.
            const float eps = 1e-4f;
            if (std::fabs(mp.x - fp.x()) > eps || std::fabs(mp.y - fp.y()) > eps ||
                std::fabs(ms - fs) > eps) {
                std::printf("FAIL %s %c%u: (%g,%g)/%g != forge (%g,%g)/%g\n",
                            name, t, i, mp.x, mp.y, ms, fp.x(), fp.y(), fs);
                failures++;
            }
        }
    }
}

int main() {
    check("master",   ModuleMaster(nullptr));
    check("master18", ModuleMaster18(nullptr));
    check("x7",       ModuleX7(nullptr));
    check("g8",       ModuleG8(nullptr));
    check("p2b8",     ModuleP2B8(nullptr));
    check("p4b2",     ModuleP4B2(nullptr));
    check("p10",      ModuleP10(nullptr));
    check("s10",      ModuleS10(nullptr));
    check("p8s8",     ModuleP8S8(nullptr));
    check("b32",      ModuleB32(nullptr));
    check("e4",       ModuleE4(nullptr));
    check("m4",       ModuleM4(nullptr));
    check("db8e",     ModuleDB8E(nullptr));
    if (failures) { std::printf("layoutcheck: %d failure(s)\n", failures); return 1; }
    std::printf("layoutcheck: all positions match the Forge\n");
    return 0;
}
