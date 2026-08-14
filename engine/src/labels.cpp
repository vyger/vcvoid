#include "labels.hpp"
#include "controllers.hpp"
#include <cctype>

namespace droid {

namespace {

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char) s[a])) a++;
    while (b > a && std::isspace((unsigned char) s[b - 1])) b--;
    return s.substr(a, b - a);
}

// [1-9][0-9]* — a positive decimal with no leading zero, as the Forge's regexes
// spell it. Advances `i` past the digits; returns false (leaving `i`) if none.
bool readNumber(const std::string& s, size_t& i, unsigned& out) {
    if (i >= s.size() || s[i] < '1' || s[i] > '9') return false;
    unsigned v = 0;
    while (i < s.size() && std::isdigit((unsigned char) s[i])) {
        v = v * 10 + unsigned(s[i] - '0');
        i++;
    }
    out = v;
    return true;
}

// sectionSeparator, "^----*$" — three or more dashes and nothing else.
bool isSectionSeparator(const std::string& c) {
    if (c.size() < 3) return false;
    for (char ch : c)
        if (ch != '-') return false;
    return true;
}

// maybeParseMetaComment, "^[[:space:]]*([A-Z][A-Z 0-9]*):[[:space:]]*(.*)$".
// We only need its boolean: a meta comment is swallowed rather than becoming
// the patch title. The register-label test runs FIRST wherever both apply, so
// this never shadows a label.
bool isMetaComment(const std::string& c) {
    size_t i = 0;
    if (i >= c.size() || !(c[i] >= 'A' && c[i] <= 'Z')) return false;
    i++;
    while (i < c.size() && ((c[i] >= 'A' && c[i] <= 'Z') || c[i] == ' ' ||
                            (c[i] >= '0' && c[i] <= '9')))
        i++;
    return i < c.size() && c[i] == ':';
}

// disabledJackLine, "^[a-zA-z]+([1-9][0-9]*)?[[:space:]]*=.*" — a commented-out
// parameter line inside a circuit. (The Forge's character class really is
// [a-zA-z], which also admits the punctuation between 'Z' and 'a'; kept as-is
// so a patch that round-trips through the Forge behaves identically here.)
bool isDisabledJackLine(const std::string& c) {
    size_t i = 0;
    while (i < c.size() && c[i] >= 'A' && c[i] <= 'z') i++;
    if (i == 0) return false;
    unsigned dummy = 0;
    readNumber(c, i, dummy);
    while (i < c.size() && std::isspace((unsigned char) c[i])) i++;
    return i < c.size() && c[i] == '=';
}

// parseCircuitLine's two forms: "[name]" and "[name] # comment". Returns false
// when the line matches neither (the Forge throws there, leaving its comment
// state untouched, so we must not transition either).
bool readCircuitName(const std::string& line, std::string& name) {
    if (line.empty() || line[0] != '[') return false;
    size_t close = line.find(']');
    if (close == std::string::npos || close == 1) return false;
    for (size_t i = 1; i < close; i++)
        if (!std::isalnum((unsigned char) line[i])) return false;
    size_t i = close + 1;
    while (i < line.size() && std::isspace((unsigned char) line[i])) i++;
    if (i != line.size() && line[i] != '#') return false;
    name = line.substr(1, close - 1);
    for (char& ch : name) ch = char(std::tolower((unsigned char) ch));
    return true;
}

// maybeParseRegisterLabel:
//   "^([a-zA-Z])([1-9][0-9]*)[.]?([1-9][0-9]*)?:[[:space:]]*(.*)$"
// plus the shorthand split "^\[([^]]+)\][[:space:]]*(.*)$" on the text.
bool parseLabelComment(const std::string& c, RegisterLabel& out) {
    size_t i = 0;
    if (i >= c.size() || !std::isalpha((unsigned char) c[i])) return false;
    char type = char(std::toupper((unsigned char) c[i]));
    i++;

    unsigned first = 0, second = 0;
    if (!readNumber(c, i, first)) return false;
    bool haveSecond = false;
    if (i < c.size() && c[i] == '.') i++;
    if (readNumber(c, i, second)) haveSecond = true;
    if (i >= c.size() || c[i] != ':') return false;
    i++;
    while (i < c.size() && std::isspace((unsigned char) c[i])) i++;
    std::string text = c.substr(i);

    out = RegisterLabel{};
    out.type = type;
    if (!haveSecond) {
        out.number = first;
    } else {
        out.number = second;
        // Gate registers are numbered per G8 expander, everything else per
        // controller: "G2.5" is G8 #2, "P2.4" is controller #2.
        if (type == 'G' && second >= 1 && second <= 8) out.g8 = first;
        else out.controller = first;
    }
    // Old-style bare gates ("G3") mean the first G8 ("G1.3").
    if (type == 'G' && out.g8 == 0 && out.number <= 8) out.g8 = 1;

    if (!text.empty() && text[0] == '[') {
        size_t close = text.find(']');
        if (close != std::string::npos && close > 1) {
            out.shorthand = text.substr(1, close - 1);
            size_t j = close + 1;
            while (j < text.size() && std::isspace((unsigned char) text[j])) j++;
            text = text.substr(j);
        }
    }
    out.text = text;
    return true;
}

} // namespace

const RegisterLabel* PatchLabels::find(char type, unsigned controller,
                                       unsigned g8, unsigned number) const {
    for (const auto& l : labels)
        if (l.type == type && l.controller == controller && l.g8 == g8 &&
            l.number == number)
            return &l;
    return nullptr;
}

PatchLabels parseRegisterLabels(const std::string& text) {
    // The Forge's PatchParser::commentState, minus the states we cannot reach
    // by only tracking labels. DESCRIPTION is the ONLY state in which a
    // register-label comment counts.
    enum State { AwaitingTitle, Description, SectionHeaderActive, CircuitHeader };
    State state = AwaitingTitle;
    bool haveCircuit = false;    // the Forge's `circuit` pointer, as a flag
    PatchLabels out;

    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        std::string raw = text.substr(pos, nl == std::string::npos ? std::string::npos
                                                                   : nl - pos);
        pos = (nl == std::string::npos) ? text.size() + 1 : nl + 1;
        std::string line = trim(raw);

        if (line.empty()) {
            // parseEmptyLine: an empty line before any comment means the patch
            // has no title, so the description starts here. Inside the
            // description it only flushes collected prose — state is unchanged,
            // which is why a blank line between header comments does not stop
            // later labels from being recognised.
            if (state == AwaitingTitle) state = Description;
            continue;
        }

        if (line[0] == '#') {
            std::string comment = trim(line.substr(1));

            // A commented-out circuit still ends the header, exactly as a live
            // one does.
            std::string name;
            if (!comment.empty() && comment[0] == '[') {
                if (readCircuitName(comment, name) && !findControllerModel(name))
                    state = CircuitHeader;
                continue;
            }
            // A commented-out parameter line is skipped whole (only once a
            // circuit is open, matching the Forge's `if (circuit)` guard).
            if (haveCircuit && isDisabledJackLine(comment)) continue;

            if (isSectionSeparator(comment)) {
                state = (state == SectionHeaderActive) ? CircuitHeader
                                                       : SectionHeaderActive;
                continue;
            }
            if (state == AwaitingTitle) {
                if (!isMetaComment(comment)) out.title = comment;
                state = Description;
            } else if (state == Description) {
                RegisterLabel l;
                if (parseLabelComment(comment, l)) {
                    // A repeated register replaces its earlier label, as the
                    // Forge's QMap assignment does.
                    bool replaced = false;
                    for (auto& existing : out.labels) {
                        if (existing.type == l.type &&
                            existing.controller == l.controller &&
                            existing.g8 == l.g8 && existing.number == l.number) {
                            existing = l;
                            replaced = true;
                            break;
                        }
                    }
                    if (!replaced) out.labels.push_back(l);
                }
            }
            continue;
        }

        if (line[0] == '[') {
            // Declaring a controller does NOT close the header — only a real
            // circuit does. So "[p2b8]" between the title and the labels leaves
            // the labels below it live.
            std::string name;
            if (readCircuitName(line, name) && !findControllerModel(name)) {
                state = CircuitHeader;
                haveCircuit = true;
            }
            continue;
        }
        // Anything else is a parameter line or a syntax error; neither moves
        // the comment state.
    }
    return out;
}

} // namespace droid
