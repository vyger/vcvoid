// labeldump — print the register labels our extractor finds in each patch, in
// the same line format droidcheck's --labels emits from the Forge's own parser.
// `make labelcheck` diffs the two outputs; any difference is a parity bug in
// engine/src/labels.cpp.
//
//     LABEL <file> <type> <controller> <g8> <number> |<shorthand>|<text>
#include "src/labels.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        std::string path = argv[i];
        std::ifstream f(path);
        if (!f) { std::fprintf(stderr, "cannot read %s\n", path.c_str()); return 2; }
        std::stringstream ss;
        ss << f.rdbuf();

        // droidcheck prints the basename; match it.
        std::string base = path;
        size_t slash = base.find_last_of('/');
        if (slash != std::string::npos) base = base.substr(slash + 1);

        for (const auto& l : droid::parseRegisterLabels(ss.str()).labels)
            std::printf("LABEL %s %c %u %u %u |%s|%s\n", base.c_str(), l.type,
                        l.controller, l.g8, l.number, l.shorthand.c_str(),
                        l.text.c_str());
    }
    return 0;
}
