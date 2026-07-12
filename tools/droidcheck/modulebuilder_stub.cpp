// Minimal, GUI-free stand-in for the two ModuleBuilder symbols the parser and
// Patch reference. The real ModuleBuilder lives in modules/ and drags in the
// whole QGraphicsItem/MainWindow GUI stack, which the problem checker does not
// need.
//
//  * controllerExists() — used by the parser to tell a controller header like
//    [p2b8] apart from a circuit header like [lfo]. Reproduces the exact list
//    from modules/modulebuilder.cpp.
//  * allRegistersOf()   — only ever called from GUI editing operations
//    (moveRegistersToOtherControllers / patchoperator), never from
//    updateProblems(). Provided as a no-op so the linker is satisfied.

#include "modulebuilder.h"
#include "registerlist.h"

#include <QStringList>

const QStringList &ModuleBuilder::allControllers()
{
    static const QStringList controllers{
        "p2b8", "p4b2", "b32", "p10", "s10", "p8s8", "e4", "m4", "db8e"};
    return controllers;
}

bool ModuleBuilder::controllerExists(QString name)
{
    return allControllers().contains(name);
}

void ModuleBuilder::allRegistersOf(QString, unsigned, unsigned, RegisterList &)
{
    // Not needed by the problem checker; see note above.
}
