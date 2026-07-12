#include "parser.hpp"
#include "numbers.hpp"
#include <cctype>
#include <sstream>

namespace droid {

namespace {

// Intern a text into the 1-based table (slot 0 == ""); the empty string is 0.
// Returns the text number to use as the atom's numeric value.
int internText(const std::string& content, std::vector<std::string>& texts) {
    if (content.empty()) return 0;
    for (size_t k = 1; k < texts.size(); ++k)
        if (texts[k] == content) return int(k);
    texts.push_back(content);
    return int(texts.size() - 1);
}

// One value token: quoted text / number / register / cable name. A double-quoted
// token (tokenizeExpr keeps the quotes) interns to the text table and becomes an
// ordinary source number (its 1-based text number). Errors via ok=false.
bool parseAtomToken(const std::string& tok, Atom& out, std::vector<std::string>& texts) {
    if (tok.size() >= 2 && tok.front() == '"' && tok.back() == '"') {
        int idx = internText(tok.substr(1, tok.size() - 2), texts);
        out = Atom::num(float(idx), true);
        return true;
    }
    float v;
    if (parsePatchNumber(tok, v)) { out = Atom::num(v, true); return true; }
    if (!tok.empty() && tok[0] == '_') {
        out.kind = Atom::Kind::Cable; out.cable = tok; out.fromSource = true;
        return true;
    }
    if (auto r = parseRegisterName(tok)) {
        out.kind = Atom::Kind::Register; out.reg = *r; out.fromSource = true;
        return true;
    }
    return false;
}

// Split a value expression into tokens; operators are their own tokens.
// A '-' is an operator only when a value token precedes it; otherwise it is
// glued to the following token as a sign ("-P1.1", "-0.5"). '.' inside a token
// (P1.2, 0.5) is not an operator, so registers/decimals stay intact.
// A double-quoted string is one token that keeps its surrounding quotes (so
// parseAtomToken can recognise it). The quote runs to the next '"' on the same
// line; an opening quote with no closing quote sets `unterminated`.
std::vector<std::string> tokenizeExpr(const std::string& s, bool& unterminated) {
    std::vector<std::string> toks;
    size_t i = 0;
    auto prevIsValue = [&]() {
        return !toks.empty() && toks.back() != "*" && toks.back() != "/" &&
               toks.back() != "+" && toks.back() != "-";
    };
    while (i < s.size()) {
        char ch = s[i];
        if (std::isspace((unsigned char)ch)) { i++; continue; }
        if (ch == '"') {
            size_t close = s.find('"', i + 1);
            if (close == std::string::npos) { unterminated = true; break; }
            toks.push_back(s.substr(i, close - i + 1));   // includes both quotes
            i = close + 1;
            continue;
        }
        if (ch == '*' || ch == '/' || ch == '+') { toks.push_back(std::string(1, ch)); i++; continue; }
        if (ch == '-' && prevIsValue()) { toks.push_back("-"); i++; continue; }
        size_t start = i;
        if (ch == '-') i++;                         // sign glued to token
        while (i < s.size() && !std::isspace((unsigned char)s[i]) &&
               s[i] != '*' && s[i] != '/' && s[i] != '+' && s[i] != '-' && s[i] != '"')
            i++;
        toks.push_back(s.substr(start, i - start));
    }
    return toks;
}

// Canonicalize token list into A*B+C. Returns error text or "".
//
// fromSource bookkeeping (RAM accounting depends on it):
//   - Atoms that come from real source tokens (registers, cables, and numbers
//     the user wrote, including a division divisor and a negative literal `-2`)
//     carry fromSource=true.
//   - The implicit B=1 and C=0 carry fromSource=false: they are structural, the
//     Forge never materializes them (its atoms[1]/atoms[2] are null), so they
//     are not constants it counts.
//   - Unary negation glued to a non-number atom (`-P1.1`, `-_CABLE`) is REJECTED,
//     matching the Forge (patch/jackassignmentinput.cpp): such a token matches
//     NONE of its A*B+C regex forms (RATOM has no leading-minus-register
//     production), so the Forge parses it to an AtomInvalid → "Invalid (garbled)
//     value". We align (reject) for parity; verified against droidcheck in all
//     positions (leading `-P1.1`, after `*`, after `+`). A leading minus on a
//     numeric literal (`-2`, `-0.5`) is still fine.
//   - Register-subtraction `X - REG` is DIFFERENT: the Forge's form6
//     ("A - B") literally sets a = "-1", producing a real AtomNumber(-1) that
//     countUniqueConstants DOES count. So the -1 we synthesize for that shorthand
//     carries fromSource=true (see below), to match the Forge byte-for-byte.
//   - A folded division divisor (`I1 / 12` -> B=1/12) keeps fromSource=true and
//     is flagged isFraction=true. The Forge stores it as an ATOM_NUMBER_FRACTION
//     (value 1/12), and countUniqueConstants adds n, -n, 1/n and -1/n for it.
std::string canonicalize(const std::vector<std::string>& toks, ParamLine& p,
                         std::vector<std::string>& texts) {
    const std::string kNotABC = "cannot be written in the form A * B + C";
    size_t i = 0;
    bool multiplied = false;   // one multiplication (or a -1 factor) already used

    auto takeAtom = [&](Atom& out) -> std::string {
        if (i >= toks.size()) return "unexpected end of expression";
        std::string t = toks[i];
        if (t.size() > 1 && t[0] == '-' && t[1] != '"') {   // signed token
            float v;
            // A minus glued to a non-number atom (`-P1.1`, `-_CABLE`) is not a
            // valid A*B+C atom; the Forge rejects it as "Invalid (garbled)
            // value". Match that. A signed numeric literal (`-2`) parses fine.
            if (!parsePatchNumber(t, v)) return "invalid value '" + t + "'";
        }
        Atom a;
        if (!parseAtomToken(t, a, texts)) return "invalid value '" + toks[i] + "'";
        i++;
        out = a;
        return "";
    };

    p.b = Atom::num(1.0f, false);
    p.c = Atom::num(0.0f, false);

    std::string err = takeAtom(p.a);
    if (!err.empty()) return err;

    if (i < toks.size() && (toks[i] == "*" || toks[i] == "/")) {
        if (multiplied) return kNotABC;               // e.g. -P1.1 * I1
        bool division = toks[i] == "/";
        i++;
        Atom b;
        err = takeAtom(b);
        if (!err.empty()) return err;
        if (division) {
            if (b.kind != Atom::Kind::Number || b.number == 0.0f)
                return "division is only allowed by a fixed non-zero number";
            b.fractionDenom = b.number;               // keep the exact divisor for RAM
            b.number = 1.0f / b.number;               // fold; keep fromSource=true
            b.isFraction = true;                      // Forge: n and 1/n both count
        }
        multiplied = true;
        p.b = b;
    }

    if (i < toks.size() && (toks[i] == "+" || toks[i] == "-")) {
        bool minus = toks[i] == "-";
        i++;
        Atom cAtom;
        bool savedMult = multiplied;
        err = takeAtom(cAtom);
        if (!err.empty()) return err;
        // A signed register/cable in the C slot (`X + -REG`) would burn the one
        // multiplication for the offset term — not expressible as A*B+C.
        if (multiplied != savedMult) return kNotABC;
        if (minus) {
            if (cAtom.kind == Atom::Kind::Number) {
                cAtom.number = -cAtom.number;
                p.c = cAtom;
            } else {
                // X - REG  ==  A=REG, B=-1, C=X   (uses the one multiplication).
                // The Forge's form6 emits a real AtomNumber(-1) here, so this -1
                // is a counted constant: fromSource=true (see header note).
                if (multiplied) return kNotABC;
                p.c = p.a;
                p.a = cAtom;
                p.b = Atom::num(-1.0f, true);
            }
        } else {
            p.c = cAtom;
        }
    }

    if (i != toks.size()) return kNotABC;              // trailing tokens
    // simple = the source was a single plain atom with no operators. (A leading
    // minus now only survives on a numeric literal like `-2`, which is still a
    // single simple atom; `-REG`/`-_CABLE` are rejected in takeAtom.)
    p.simple = (toks.size() == 1);
    return "";
}

} // namespace

ParseResult parsePatch(const std::string& text) {
    ParseResult pr;
    std::istringstream in(text);
    std::string raw;
    int lineNo = 0;
    CircuitSection* cur = nullptr;

    while (std::getline(in, raw)) {
        lineNo++;
        std::string line = raw;
        // Comment strip, but a '#' inside a double-quoted text is literal, not a
        // comment (Forge parity: droidcheck accepts `input = "a#b"`).
        {
            bool inQuote = false;
            for (size_t k = 0; k < line.size(); ++k) {
                if (line[k] == '"') inQuote = !inQuote;
                else if (line[k] == '#' && !inQuote) { line.erase(k); break; }
            }
        }
        // trim
        size_t b = line.find_first_not_of(" \t\r");
        if (b == std::string::npos) continue;
        size_t e = line.find_last_not_of(" \t\r");
        line = line.substr(b, e - b + 1);

        if (line.front() == '[') {
            if (line.back() != ']' || line.size() < 3) {
                pr.errors.push_back({lineNo, "malformed section header"});
                continue;
            }
            pr.sections.push_back({line.substr(1, line.size() - 2), lineNo, {}});
            cur = &pr.sections.back();
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            pr.errors.push_back({lineNo, "expected 'parameter = value' or '[circuit]'"});
            continue;
        }
        if (!cur) {
            pr.errors.push_back({lineNo, "parameter outside of any [circuit] section"});
            continue;
        }
        ParamLine p;
        p.line = lineNo;
        // parameter name: trim + lowercase
        {
            std::string n = line.substr(0, eq);
            size_t nb = n.find_first_not_of(" \t");
            size_t ne = n.find_last_not_of(" \t");
            if (nb == std::string::npos) { pr.errors.push_back({lineNo, "missing parameter name"}); continue; }
            n = n.substr(nb, ne - nb + 1);
            for (auto& c : n) c = char(std::tolower((unsigned char)c));
            p.name = n;
        }
        bool unterminated = false;
        auto toks = tokenizeExpr(line.substr(eq + 1), unterminated);
        if (unterminated) { pr.errors.push_back({lineNo, "unterminated text parameter"}); continue; }
        if (toks.empty()) { pr.errors.push_back({lineNo, "missing value"}); continue; }
        std::string err = canonicalize(toks, p, pr.texts);
        if (!err.empty()) { pr.errors.push_back({lineNo, err}); continue; }
        cur->params.push_back(std::move(p));
    }
    return pr;
}

std::string stripPatch(const std::string& text) {
    std::istringstream in(text);
    std::string raw, out;
    while (std::getline(in, raw)) {
        // Quote-aware: whitespace and '#' inside a double-quoted text value are
        // literal content and must be preserved (text parameters are supported —
        // interned in parsePatch), so the size estimate counts them. Outside
        // quotes, '#' starts a comment and whitespace is dropped.
        std::string kept;
        bool inQuote = false;
        for (char c : raw) {
            if (c == '"') { inQuote = !inQuote; kept += c; continue; }
            if (inQuote) { kept += c; continue; }
            if (c == '#') break;
            if (!std::isspace((unsigned char)c)) kept += c;
        }
        if (!kept.empty()) { out += kept; out += '\n'; }
    }
    return out;
}

} // namespace droid
