#pragma once
// The master's visible error state (issue #46) — the MODEL half.
//
// Pure C++, NO Rack includes, so it links into the headless unit-test build as
// well as the Rack widget (same arrangement as droidcolor.hpp). Everything that
// decides WHAT to show lives here as pure functions of a plain `Report`; the
// widget side (MasterBase.hpp, StatusRing.hpp, DroidMaster.cpp) only paints the
// answer.
//
// Two things are modelled:
//
//   1. `evaluate()` — the five states a master can be in (design: issue #46,
//      option C "module halo") and, for each, the ring colour, what the 4x4
//      matrix does, and the words for the tooltip and the context-menu card.
//
//   2. `blinkCode()` — the hardware's LED blink codes (manual/basics.md §5.3
//      and §5.4), an exact encoder from (error code, line number) to the
//      colours of the MASTER's 16 matrix LEDs.
//
// ---------------------------------------------------------------------------
// How vcvoid's load errors map onto the hardware's error codes
// ---------------------------------------------------------------------------
// The hardware has a fixed, small vocabulary of error codes; vcvoid's loader
// (and the Forge's stricter static analysis that it mirrors) raises a longer
// list. droid::ErrorCode is the join: every push_back in the loader/parser is
// tagged with the hardware code it belongs to.
//
//   engine error                                          -> droid::ErrorCode
//   ------------------------------------------------------------------------
//   patch exceeds the maximum size                           PatchTooBig (blue, global)
//   this circuit exceeds the available memory                OutOfMemory (cyan, global)
//   cannot open <file>                                       PatchNotFound (yellow, global)
//   Unknown circuit '…'                                      UnknownCircuit (red)
//   Circuit '…' is experimental                              UnknownCircuit (red)
//   Circuit '…' needs a MASTER18                             UnknownCircuit (red)
//   circuit '…' is not yet implemented in vcvoid             UnknownCircuit (red)
//   Circuit '…' has no parameter '…'                         UnknownParameter (orange)
//   There is no register … / refers to controller …          UnknownRegister (yellow)
//   register … cannot be used as an output                   UnknownRegister (yellow)
//   output must be a single output register or cable         InvalidSyntax (magenta)
//   every parser syntax error (header, '=', value, text)     InvalidSyntax (magenta)
//   Duplicate usage of <reg> as output                       CableMisuse (green)
//   Output register <reg> is just used as an input           CableMisuse (green)
//   Patch cable … never used as an in/output, duplicate      CableMisuse (green)
//
// Approximations, deliberately: the hardware's green is worded "internal patch
// cable misused", but the Forge reports the three O-register misuse errors in
// the same breath and they are the same kind of fault (a signal wired wrong,
// not a name spelled wrong), so they share green. "output must be a single
// output register" is a bad VALUE for a parameter, which is the third meaning
// of the hardware's magenta.
//
// Hardware codes vcvoid never raises: TooManyControllers (red, global — vcvoid
// has no 16-controller ceiling check), InvalidFirmware (magenta, global — there
// is no firmware file to corrupt), NoSdCard (white, global — no card), and
// LineTooLong (blue, local — vcvoid does not enforce the 63-character line
// limit). They are in the enum so the table is complete and so the colours are
// documented where the encoder lives.
//
// Errors with no hardware code at all get ErrorCode::Unmapped and no blink
// code: the ring still turns red and the tooltip and menu card still carry the
// full text, the matrix simply has nothing to spell. At the time of writing
// every error the loader raises IS mapped; Unmapped is the safe default for
// ones added later.
#include "src/types.hpp"   // droid::ErrorCode, droid::LoadError (via -I../engine)
#include <string>
#include <vector>

namespace vcvoid {
namespace status {

// --- fitting the words into a window -------------------------------------
// The messages are sentences, and some of them are long ones ("Circuit 'x' is
// experimental (vcvoid only, …). Enable "Allow experimental circuits" in the
// module's context menu to load this patch."). Rack sizes both a tooltip and a
// menu to the widest line it is given, so an unwrapped message drags the error
// card clean off the screen. Everything the card and the tooltip show is
// therefore laid out to a fixed column here, in the model, so the two can never
// disagree about where a line ends.
//
// 64 characters is the width the card is built around: wide enough for the
// quoted patch line (63 characters is the hardware's own line limit, manual
// §5.1) and narrow enough that the menu stays roughly as wide as the code quote
// it already showed.
constexpr size_t kWrapWidth = 64;

inline bool isWrapSpace(char c) { return c == ' ' || c == '\t'; }

// Greedy word wrap. Breaks only at spaces; a token longer than the width (a
// path, a cable name) is split at the width rather than allowed to push the
// line out. Existing newlines are kept as breaks — including blank lines — and
// runs of spaces collapse, which is what trims the trailing whitespace a
// concatenated message tends to carry.
inline std::vector<std::string> wrapLines(const std::string& text,
                                          size_t width = kWrapWidth) {
    std::vector<std::string> out;
    if (width == 0) { out.push_back(text); return out; }
    size_t pos = 0;
    for (;;) {
        size_t nl = text.find('\n', pos);
        std::string para = (nl == std::string::npos) ? text.substr(pos)
                                                     : text.substr(pos, nl - pos);
        std::string cur;
        bool any = false;
        size_t i = 0;
        while (i < para.size()) {
            while (i < para.size() && isWrapSpace(para[i])) i++;
            size_t j = i;
            while (j < para.size() && !isWrapSpace(para[j])) j++;
            if (j == i) break;
            std::string word = para.substr(i, j - i);
            i = j;
            while (word.size() > width) {     // unbreakable token: hard split
                if (!cur.empty()) { out.push_back(cur); cur.clear(); any = true; }
                out.push_back(word.substr(0, width));
                any = true;
                word = word.substr(width);
            }
            if (cur.empty()) cur = word;
            else if (cur.size() + 1 + word.size() <= width) cur += " " + word;
            else { out.push_back(cur); cur = word; }
        }
        if (!cur.empty()) { out.push_back(cur); any = true; }
        if (!any) out.push_back(std::string());   // an empty line stays a line
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return out;
}

// The same wrap as one string. rack::ui::Tooltip renders '\n', so this is the
// tooltip's form of the card's stack of labels.
inline std::string wrapText(const std::string& text, size_t width = kWrapWidth) {
    std::vector<std::string> lines = wrapLines(text, width);
    std::string out;
    for (size_t i = 0; i < lines.size(); i++) {
        if (i) out += "\n";
        out += lines[i];
    }
    return out;
}

// For the one thing that must NOT wrap: a file name, on a row (or a menu item
// label) that is a single line by construction. Both ends of a path carry
// meaning, so the middle goes.
inline std::string elideMiddle(const std::string& text,
                               size_t width = kWrapWidth) {
    const std::string ell = "...";
    if (text.size() <= width || width <= ell.size() + 1) return text;
    size_t keep = width - ell.size();
    size_t head = (keep + 1) / 2;
    return text.substr(0, head) + ell + text.substr(text.size() - (keep - head));
}

struct RGB { float r = 0.f, g = 0.f, b = 0.f; };

// The LED colours the manual's two error tables name. Same hues as the
// patch-facing value->colour table in droidcolor.hpp, so a blink code and an
// R-register override of "the same colour" really do look alike.
constexpr RGB kDark   {0.f, 0.f, 0.f};
constexpr RGB kYellow {1.f, 1.f, 0.f};
constexpr RGB kOrange {1.f, 0.5f, 0.f};
constexpr RGB kRed    {1.f, 0.f, 0.f};
constexpr RGB kBlue   {0.f, 0.f, 1.f};
constexpr RGB kGreen  {0.f, 1.f, 0.f};
constexpr RGB kMagenta{1.f, 0.f, 1.f};
constexpr RGB kCyan   {0.f, 1.f, 1.f};
constexpr RGB kWhite  {1.f, 1.f, 1.f};

// Ring colours, from the design canvas ("Chosen: C"): #e8402c / #e8a92c /
// #7a7a7a. Red means "not running" and is shared by a failed load and a chain
// error — the words tell them apart, not the colour.
constexpr RGB kRingRed  {0.910f, 0.251f, 0.173f};
constexpr RGB kRingAmber{0.910f, 0.663f, 0.173f};
constexpr RGB kRingGrey {0.478f, 0.478f, 0.478f};

// How long one full on/off cycle of the blink code takes, in seconds. The
// manual does not specify a rate; this matches the design mock.
constexpr float kBlinkPeriod = 0.9f;

enum class State {
    NoPatch,      // no patch file on this master
    LoadFailed,   // the patch was refused; nothing is running
    Warnings,     // running, but the load raised warnings
    ChainError,   // loaded and valid, but the controller chain does not match
    Running,      // nothing to report
};

// Stable machine-readable name for one state, for the UAT bridge's
// /master/status (and anything else that has to assert on it in a test).
inline const char* stateName(State st) {
    switch (st) {
        case State::NoPatch:    return "no-patch";
        case State::LoadFailed: return "load-failed";
        case State::Warnings:   return "warnings";
        case State::ChainError: return "chain-error";
        case State::Running:    return "running";
    }
    return "running";
}

// What the MASTER's 4x4 LED matrix does. (The MASTER18 has no matrix; on the
// hardware its four rear LEDs carry a reduced code, which is not visible in
// Rack at all, so there the ring carries everything.)
enum class Matrix {
    Mirror,   // the normal jack mirror / R-register override
    Dark,     // all 16 LEDs off
    Blink,    // the hardware blink code, flashing
};

// The ring colour for one state. Returns false for Running, which draws no ring
// at all — a rack full of healthy masters stays quiet. Kept separate from
// evaluate() so the widget can paint the ring every frame from the one state
// enum it reads lock-free, without rebuilding the whole (string-carrying)
// verdict.
inline bool ringColor(State st, RGB& out) {
    switch (st) {
        case State::NoPatch:    out = kRingGrey;  return true;
        case State::LoadFailed: out = kRingRed;   return true;
        case State::ChainError: out = kRingRed;   return true;
        case State::Warnings:   out = kRingAmber; return true;
        case State::Running:    break;
    }
    return false;
}

// The 4x4 matrix as a blink code paints it, row-major and in the same order as
// the R registers: 0..7 are the input LEDs I1..I8 (R1..R8), 8..15 the output
// LEDs O1..O8 (R9..R16).
struct BlinkCode {
    bool active = false;   // false: nothing to show, leave the matrix dark
    bool global = false;   // true: all 16 LEDs in the reason colour
    RGB led[16] = {};
};

// The hardware colour for one error code, and whether it is a global
// (all-LEDs) or a local (colour + line number) code. Returns false for a code
// with no hardware equivalent.
inline bool errorColor(droid::ErrorCode code, RGB& out, bool& global) {
    using C = droid::ErrorCode;
    switch (code) {
        // manual/basics.md §5.4, "All LEDs flashing at once (global error)"
        case C::PatchNotFound:      out = kYellow;  global = true;  return true;
        case C::TooManyControllers: out = kRed;     global = true;  return true;
        case C::PatchTooBig:        out = kBlue;    global = true;  return true;
        case C::OutOfMemory:        out = kCyan;    global = true;  return true;
        case C::InvalidFirmware:    out = kMagenta; global = true;  return true;
        case C::NoSdCard:           out = kWhite;   global = true;  return true;
        // manual/basics.md §5.4, "Just some of the LEDs flashing (local error)"
        case C::UnknownRegister:    out = kYellow;  global = false; return true;
        case C::UnknownParameter:   out = kOrange;  global = false; return true;
        case C::UnknownCircuit:     out = kRed;     global = false; return true;
        case C::LineTooLong:        out = kBlue;    global = false; return true;
        case C::CableMisuse:        out = kGreen;   global = false; return true;
        case C::InvalidSyntax:      out = kMagenta; global = false; return true;
        case C::Unmapped:           break;
    }
    return false;
}
// NOTE on a contradiction in the manual: the worked examples just above the
// tables caption the all-cyan picture "the SD card was not found" and the
// all-red one "too many circuits or out of memory", which is not what the table
// on the facing page says (cyan = out of memory, red = too many controllers,
// white = no SD card). The tables are the normative list — §5.4 is literally
// headed "Table of error codes" — so the tables win here, and the captions are
// treated as a slip in the manual.

// The LEDs that spell one decimal digit (or the hundreds count): LED n stands
// for n and the flashing LEDs are ADDED UP. Returns a bitmask of LEDs 1..8 in
// bits 0..7.
//
// Greedy from 8 downwards, which reproduces the manual's rule exactly — "if a 9
// is needed, then 8 + 1 will flash" — and extends it to the hundreds, where the
// manual's own example is LED 2 + LED 8 = 10 hundreds = line 1000. The largest
// representable count is 1+2+…+8 = 36, i.e. line 3699 with the tens and ones.
inline unsigned ledDigits(int value) {
    unsigned mask = 0;
    for (int n = 8; n >= 1 && value > 0; n--)
        if (value >= n) { mask |= 1u << (n - 1); value -= n; }
    return mask;
}

// The highest line number the blink code can spell (manual §5.3).
constexpr int kMaxBlinkLine = 3699;

// The hardware blink code for one error: the colours of the 16 matrix LEDs.
//
// Global codes flash all 16 LEDs in the reason colour. Local codes flash the
// reason colour on the input LEDs for the TENS of the line and the output LEDs
// for the ONES, with white input LEDs for the HUNDREDS.
//
// Two judgement calls the manual leaves open:
//   - A local code with no line number (vcvoid raises a couple of whole-patch
//     analysis errors that the hardware would never see, e.g. "Output register
//     O5 is just used as an input"), or a line past 3699, cannot be spelled.
//     It falls back to the GLOBAL form — all 16 LEDs in the reason colour — so
//     the reason is still readable; the exact line is in the tooltip and the
//     context-menu card either way.
//   - When a hundreds LED and a tens LED land on the same input LED, white
//     wins: white is the unmistakable "add a hundred" marker, and losing it
//     would misreport the line by hundreds, where losing a tens LED misreports
//     it by tens and the reader can still see the code's colour elsewhere.
inline BlinkCode blinkCode(droid::ErrorCode code, int line) {
    BlinkCode b;
    RGB c;
    bool global = false;
    if (!errorColor(code, c, global)) return b;   // no hardware code: nothing to show
    b.active = true;
    if (global || line <= 0 || line > kMaxBlinkLine) {
        b.global = true;
        for (RGB& l : b.led) l = c;
        return b;
    }
    unsigned hundreds = ledDigits(line / 100);
    unsigned tens     = ledDigits((line % 100) / 10);
    unsigned ones     = ledDigits(line % 10);
    for (int i = 0; i < 8; i++) {
        if (hundreds & (1u << i))  b.led[i] = kWhite;
        else if (tens & (1u << i)) b.led[i] = c;
        if (ones & (1u << i))      b.led[8 + i] = c;
    }
    return b;
}

// Everything the master knows about its own health, flattened into plain data
// so the verdict below is a pure function of it.
struct Report {
    bool havePatch = false;        // a patch file is assigned to this master
    bool fileUnreadable = false;   // …but it could not be opened
    bool loadOk = false;           // the last load produced a runnable engine
    int errorCount = 0;
    int errorLine = 0;             // 0 = the error is not tied to a line
    droid::ErrorCode errorCode = droid::ErrorCode::Unmapped;
    std::string errorMessage;      // the FIRST error; the rest are in the copy text
    int warningCount = 0;
    std::string warningMessage;    // the first warning
    std::string chainError;        // non-empty: the chain does not match the patch
    std::string fileName;          // basename, for the card
};

// The verdict: what to paint, and what to say.
struct Status {
    State state = State::NoPatch;
    bool ringVisible = false;
    RGB ring;
    Matrix matrix = Matrix::Mirror;
    BlinkCode blink;
    std::string title;     // bold card title, e.g. "LOAD ERROR · line 99"
    std::string message;   // the detail line; empty when there is nothing to add
    int line = 0;          // the offending line, 0 = none (no "open at line")
};

// One line for a tooltip / an LED description: title, plus the message when
// there is one. Kept here rather than in the widget so the tooltip, the LED
// descriptions and the menu card can never drift apart.
inline std::string oneLine(const Status& s) {
    if (s.title.empty()) return s.message;
    if (s.message.empty()) return s.title;
    return s.title + " — " + s.message;
}

inline Status evaluate(const Report& r) {
    Status s;
    // Precedence: a patch that would not load is the strongest signal — it is
    // why nothing runs — so it outranks a chain mismatch, which in turn
    // outranks warnings, which by definition still run.
    if (!r.havePatch) {
        s.state = State::NoPatch;
        s.ringVisible = ringColor(s.state, s.ring);
        // Deliberate deviation from the hardware, which flashes all-yellow
        // "patch not found" forever: a freshly added Rack module has no patch
        // by definition, and a module that strobes on sight is noise, not
        // information. The grey ring and the tooltip carry the state instead.
        s.matrix = Matrix::Dark;
        s.title = "No patch loaded";
        return s;
    }
    if (r.fileUnreadable || !r.loadOk) {
        s.state = State::LoadFailed;
        s.ringVisible = ringColor(s.state, s.ring);
        droid::ErrorCode code = r.fileUnreadable
            ? droid::ErrorCode::PatchNotFound : r.errorCode;
        int line = r.fileUnreadable ? 0 : r.errorLine;
        s.blink = blinkCode(code, line);
        s.matrix = s.blink.active ? Matrix::Blink : Matrix::Dark;
        s.line = line;
        s.title = line > 0 ? "LOAD ERROR · line " + std::to_string(line)
                           : std::string("LOAD ERROR");
        s.message = r.errorMessage;
        if (r.errorCount > 1)
            s.message += " (+" + std::to_string(r.errorCount - 1) + " more)";
        return s;
    }
    if (!r.chainError.empty()) {
        // The engine is loaded and valid but paused with its outputs held, so
        // the panel otherwise looks exactly like a running master. Red, and the
        // words say which kind of red it is.
        s.state = State::ChainError;
        s.ringVisible = ringColor(s.state, s.ring);
        s.matrix = Matrix::Mirror;   // frozen by the pause, not by us
        s.title = "CHAIN ERROR";
        s.message = r.chainError;
        return s;
    }
    if (r.warningCount > 0) {
        s.state = State::Warnings;
        s.ringVisible = ringColor(s.state, s.ring);
        s.title = r.warningCount == 1
            ? std::string("Running with 1 warning")
            : "Running with " + std::to_string(r.warningCount) + " warnings";
        s.message = r.warningMessage;
        if (r.warningCount > 1)
            s.message += " (+" + std::to_string(r.warningCount - 1) + " more)";
        return s;
    }
    s.state = State::Running;
    s.ringVisible = ringColor(s.state, s.ring);
    return s;
}

}  // namespace status
}  // namespace vcvoid
