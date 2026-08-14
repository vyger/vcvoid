#pragma once
// Register labels: the human names a patch gives its jacks and controls, written
// as header comments and rendered by the Droid Forge on the module faces.
//
//     # My patch title
//     #
//     #   O1: [CLK] master clock, 8 Hz square
//     #   P2.4: filter cutoff
//
// This is a SEPARATE pass over the raw patch text, not part of parser.cpp: the
// engine parser strips comments before anything sees them (parser.cpp §"Comment
// strip"), and labels must not influence patch semantics in any way. Nothing in
// the engine reads these — they exist for the Rack plugin's tooltips and panel
// chips (issue #26).
//
// Parity target is the Forge's own PatchParser (parser/patchparser.cpp), which
// only recognises labels in one specific place: the patch HEADER, after the
// title comment and before the first section separator or circuit. The same
// text inside a "# ------" section-header block is section prose, not a label.
#include <string>
#include <vector>

namespace droid {

// One label. `type` is the Forge's register_type_t char (I N O G B L P E S R X);
// `controller` is 0 for the master, else the controller number (P2.4 -> 2);
// `g8` is the G8 number for gate registers (G2.5 -> 2, and a bare G3 -> G1.3,
// matching the Forge's old-style rewrite).
struct RegisterLabel {
    char type = 0;
    unsigned controller = 0;
    unsigned g8 = 0;
    unsigned number = 0;
    std::string shorthand;   // the "[SHORT]" prefix, empty when absent
    std::string text;        // the remaining comment text
};

struct PatchLabels {
    std::string title;                  // first comment line of the patch, if any
    std::vector<RegisterLabel> labels;  // patch order; a repeated register replaces in place

    const RegisterLabel* find(char type, unsigned controller, unsigned g8,
                              unsigned number) const;
    bool empty() const { return labels.empty(); }
};

PatchLabels parseRegisterLabels(const std::string& text);

} // namespace droid
