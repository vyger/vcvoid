// droidcheck — headless runner for the DROID Forge patch "problem checker".
//
// Reuses the Forge's own parser + Patch::updateProblems() so the results match
// what the GUI shows in its problem panel. GUI-only bits (modules/rackview) are
// not linked; the two ModuleBuilder symbols the checker references are provided
// by modulebuilder_stub.cpp.
//
// Usage: droidcheck patch1.ini [patch2.ini ...]
// Exit code: number of patches that have at least one problem (0 = all clean).

#include "patchparser.h"
#include "patch.h"
#include "patchproblem.h"
#include "parseexception.h"
#include "imagecache.h"
#include "droidfirmware.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QTextStream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("droidcheck");
    QCoreApplication::setApplicationName("droidcheck");

    ImageCache imageCache;     // registers the_image_cache
    DroidFirmware firmware;    // registers the_firmware (loads :droidfirmware.json)

    QTextStream out(stdout);
    out << "DROID Forge problem checker  (firmware " << firmware.version() << ")\n";
    out << "======================================================\n";

    int filesWithProblems = 0;

    for (int i = 1; i < argc; i++) {
        QString path = QString::fromLocal8Bit(argv[i]);
        QString base = QFileInfo(path).fileName();

        Patch patch;
        PatchParser parser;
        try {
            parser.parseFile(path, &patch);
        }
        catch (ParseException &e) {
            out << "\nFAIL  " << base << "\n";
            out << "        parse error: " << e.toString() << "\n";
            filesWithProblems++;
            continue;
        }

        patch.updateProblems();
        const QList<PatchProblem *> &problems = patch.allProblems();

        if (problems.isEmpty()) {
            out << "\nOK    " << base << "  — no problems\n";
        }
        else {
            filesWithProblems++;
            out << "\nFAIL  " << base << "  — " << problems.count() << " problem(s)\n";
            for (auto *p : problems) {
                const CursorPosition &pos = p->getCursorPosition();
                out << "        [section " << p->getSection()
                    << ", circuit " << pos.circuitNr
                    << ", row " << pos.row
                    << ", col " << pos.column << "] "
                    << p->getReason() << "\n";
            }
        }
    }

    out << "\n======================================================\n";
    out << (argc - 1) << " patch(es) checked, "
        << filesWithProblems << " with problems.\n";
    out.flush();
    return filesWithProblems;
}
