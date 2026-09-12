// patchsize — print how big each patch is by the two measures that matter:
// the stripped VERBOSE text, and the stripped text with abbreviated parameter
// names, which is what the master actually receives and what the 64 000-byte
// limit applies to (issue #41).
//
//     SIZE <file> <verbose> <deployed>
//
// `make sizecheck` diffs the deployed column against tools/inicompress.py,
// which performs the same abbreviation in Python from the Forge's firmware file.
#include "src/loader.hpp"
#include "src/parser.hpp"
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
        std::string text = ss.str();

        std::string base = path;
        size_t slash = base.find_last_of('/');
        if (slash != std::string::npos) base = base.substr(slash + 1);

        std::printf("SIZE %s %zu %zu\n", base.c_str(), droid::stripPatch(text).size(),
                    droid::deployedPatchSize(text));
    }
    return 0;
}
