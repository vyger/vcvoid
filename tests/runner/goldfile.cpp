#include "goldfile.hpp"
#include "midinames.hpp"
#include "src/registers.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace droid::gold {

static std::string unquote(std::string s) {
    // Trim surrounding whitespace first so a trailing space after the closing
    // quote doesn't defeat the quote check (`  "msg"  ` -> `msg`).
    size_t b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r");
    s = s.substr(b, e - b + 1);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

GoldFile parse(const std::string& path) {
    GoldFile g;
    std::ifstream f(path);
    if (!f) { g.parseError = "cannot open " + path; return g; }
    std::string line;
    int lineNo = 0;
    bool inPatch = false;
    while (std::getline(f, line)) {
        lineNo++;
        if (inPatch) {
            if (line == ">>>") { inPatch = false; continue; }
            g.patch += line; g.patch += '\n';
            continue;
        }
        size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        std::istringstream ls(line);
        std::string word;
        if (!(ls >> word)) continue;

        if (word == "master")        ls >> g.master;
        else if (word == "x7")       ls >> g.x7;
        else if (word == "tickrate") ls >> g.tickrate;
        else if (word == "seed")     ls >> g.seed;
        else if (word == "expect_ram") { ls >> g.expectRam; g.hasExpectRam = true; }
        else if (word == "expect_load_error") { std::string rest; std::getline(ls, rest);
            size_t b = rest.find_first_not_of(' '); g.expectLoadError = unquote(rest.substr(b == std::string::npos ? 0 : b)); }
        else if (word == "expect_warning") { std::string rest; std::getline(ls, rest);
            size_t b = rest.find_first_not_of(' '); g.expectWarning = unquote(rest.substr(b == std::string::npos ? 0 : b)); }
        else if (word == "midifile") {
            int num = 0;
            std::string hex;
            if (!(ls >> num >> hex) || num < 1) {
                g.parseError = "midifile expects <n> <hexbytes> at line " + std::to_string(lineNo);
                return g;
            }
            if (hex.size() % 2) {
                g.parseError = "midifile hex has odd length at line " + std::to_string(lineNo);
                return g;
            }
            auto nib = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            std::vector<uint8_t>& bytes = g.midiFiles[num];
            for (size_t k = 0; k + 1 < hex.size(); k += 2) {
                int hi = nib(hex[k]), lo = nib(hex[k + 1]);
                if (hi < 0 || lo < 0) {
                    g.parseError = "midifile bad hex at line " + std::to_string(lineNo);
                    return g;
                }
                bytes.push_back(uint8_t((hi << 4) | lo));
            }
        }
        else if (word == "patch") { std::string marker; ls >> marker;
            if (marker != "<<<") { g.parseError = "patch must be followed by <<<"; return g; }
            inPatch = true; }
        else if (word[0] == '@') {
            std::string digits = word.substr(1);
            if (digits.empty() ||
                digits.find_first_not_of("0123456789") != std::string::npos) {
                g.parseError = "malformed tick '" + word + "' at line " + std::to_string(lineNo);
                return g;
            }
            Event ev;
            // digits is all-[0-9]; a 20+-digit token would overflow std::stol
            // (which throws). Parse defensively into a clean malformed-golden
            // error instead of an uncaught exception.
            errno = 0;
            char* endp = nullptr;
            long long tv = std::strtoll(digits.c_str(), &endp, 10);
            if (errno == ERANGE || *endp != '\0' || tv > 0x7fffffffLL) {
                g.parseError = "tick '" + word + "' out of range at line " +
                    std::to_string(lineNo);
                return g;
            }
            ev.tick = long(tv);
            ev.line = lineNo;
            std::string action;
            ls >> action >> ev.target;
            if (action == "set")          { ev.kind = Event::Kind::Set; ls >> ev.value; }
            else if (action == "unpatch") { ev.kind = Event::Kind::Unpatch; }
            else if (action == "probe") {
                // MASTER18 frequency probe: `probe <hz>` publishes a measured
                // frequency, `probe none` marks signal loss. The operand landed
                // in ev.target (the generic `action target` read above).
                ev.kind = Event::Kind::Probe;
                if (ev.target == "none") ev.probeNone = true;
                else {
                    char* end = nullptr;
                    ev.value = std::strtof(ev.target.c_str(), &end);
                    if (!end || *end != '\0') {
                        g.parseError = "'probe' expects a frequency or 'none' at line " +
                            std::to_string(lineNo);
                        return g;
                    }
                }
            }
            else if (action == "expect")  { ev.kind = Event::Kind::Expect; ls >> ev.value;
                std::string t; if (ls >> t && t == "tol") ls >> ev.tol; }
            else if (action == "turn" || action == "push") {
                // turn/push address a physical encoder handle, never a value
                // register (matching the seam the Rack adapter will drive).
                auto r = parseRegisterName(ev.target);
                if (!r || r->type != 'E') {
                    g.parseError = "'" + action + "' target '" + ev.target +
                        "' is not an encoder register (E<ctrl>.<num> or E<n>) at line " +
                        std::to_string(lineNo);
                    return g;
                }
                ev.kind = (action == "turn") ? Event::Kind::Turn : Event::Kind::Push;
                ls >> ev.value;
            }
            else if (action == "move" || action == "touch" || action == "hold") {
                // move/touch/hold address a physical motor fader by GLOBAL number
                // (F<n>, matching `fader = N`), never a value register.
                if (!parseFaderName(ev.target)) {
                    g.parseError = "'" + action + "' target '" + ev.target +
                        "' is not a fader handle (F<n>, n >= 1) at line " +
                        std::to_string(lineNo);
                    return g;
                }
                ev.kind = (action == "move") ? Event::Kind::Move
                        : (action == "touch") ? Event::Kind::Touch
                        : Event::Kind::Hold;
                ls >> ev.value;
            }
            else if (action == "expectdisplay") {
                // expectdisplay addresses a DB8E screen by GLOBAL number
                // (D<n>, 1-based, DB8Es only in chain order), matching Task 4's
                // Engine::displayState() seam — never a value register.
                bool okHandle = ev.target.size() >= 2 && ev.target[0] == 'D' &&
                    ev.target[1] != '0' &&
                    ev.target.find_first_not_of("0123456789", 1) == std::string::npos;
                if (!okHandle) {
                    g.parseError = "'expectdisplay' target '" + ev.target +
                        "' is not a display handle (D<n>, n >= 1) at line " +
                        std::to_string(lineNo);
                    return g;
                }
                ev.kind = Event::Kind::ExpectDisplay;
                if (!(ls >> ev.field)) {
                    g.parseError = "'expectdisplay' missing field at line " +
                        std::to_string(lineNo);
                    return g;
                }
                if (ev.field == "header" || ev.field == "text") {
                    // quoted string operand, may contain spaces; the closing
                    // quote must appear on the same line.
                    std::string rest; std::getline(ls, rest);
                    size_t open = rest.find('"');
                    size_t close = open == std::string::npos ?
                        std::string::npos : rest.find('"', open + 1);
                    if (open == std::string::npos || close == std::string::npos) {
                        g.parseError = "'expectdisplay " + ev.field +
                            "' expects a quoted string at line " +
                            std::to_string(lineNo);
                        return g;
                    }
                    ev.strValue = rest.substr(open + 1, close - open - 1);
                }
                else if (ev.field == "value") {
                    if (!(ls >> ev.value)) {
                        g.parseError = "'expectdisplay value' expects a number at line " +
                            std::to_string(lineNo);
                        return g;
                    }
                    std::string t; if (ls >> t && t == "tol") ls >> ev.tol;
                }
                else if (ev.field == "mode" || ev.field == "font") {
                    int iv;
                    if (!(ls >> iv)) {
                        g.parseError = "'expectdisplay " + ev.field +
                            "' expects an integer at line " + std::to_string(lineNo);
                        return g;
                    }
                    ev.value = float(iv);
                }
                else if (ev.field == "off") {
                    // no operand
                }
                else {
                    g.parseError = "'expectdisplay' unknown field '" + ev.field +
                        "' (want header|text|value|mode|font|off) at line " +
                        std::to_string(lineNo);
                    return g;
                }
            }
            else if (action == "midisend" || action == "expectmidi") {
                // `ev.target` already holds the port word. `trs`/`usb` are
                // aliases for the X7 ports (pre-Master18 goldens); the explicit
                // names cover the full physical table. The rest of the line is
                // the symbolic event (name + numeric args).
                if (ev.target == "trs" || ev.target == "x7trs")
                    ev.midiPort = (uint8_t)droid::MidiPort::X7Trs;
                else if (ev.target == "usb" || ev.target == "x7usb")
                    ev.midiPort = (uint8_t)droid::MidiPort::X7Usb;
                else if (ev.target == "m18usb")
                    ev.midiPort = (uint8_t)droid::MidiPort::M18Usb;
                else if (ev.target == "m18trs1")
                    ev.midiPort = (uint8_t)droid::MidiPort::M18Trs1;
                else if (ev.target == "m18trs2")
                    ev.midiPort = (uint8_t)droid::MidiPort::M18Trs2;
                else {
                    g.parseError = "'" + action + "' bad port '" + ev.target +
                        "' (want trs|usb|x7trs|x7usb|m18usb|m18trs1|m18trs2) at line " +
                        std::to_string(lineNo);
                    return g;
                }
                std::vector<std::string> words;
                std::string w2;
                while (ls >> w2) words.push_back(w2);
                if (action == "expectmidi" && words.size() == 1 && words[0] == "none") {
                    ev.kind = Event::Kind::ExpectMidi;
                    ev.midiNone = true;
                } else {
                    if (words.empty()) {
                        g.parseError = "'" + action + "' missing event at line " +
                            std::to_string(lineNo);
                        return g;
                    }
                    std::string merr;
                    if (!midiEventFromWords(words, ev.midi, merr)) {
                        g.parseError = "'" + action + "' " + merr + " at line " +
                            std::to_string(lineNo);
                        return g;
                    }
                    ev.kind = (action == "midisend") ? Event::Kind::MidiSend
                                                     : Event::Kind::ExpectMidi;
                }
            }
            else { g.parseError = "unknown action '" + action + "' at line " + std::to_string(lineNo); return g; }
            g.events.push_back(ev);
        }
        else { g.parseError = "unknown directive '" + word + "' at line " + std::to_string(lineNo); return g; }
    }
    // A gold that expects the patch to fail loading cannot also drive a timeline:
    // no engine exists to run events against. Reject the contradiction as
    // malformed rather than silently ignoring the events.
    if (!g.expectLoadError.empty() && !g.events.empty()) {
        g.parseError = "expect_load_error gold must not contain timeline events "
                       "(first at line " + std::to_string(g.events.front().line) + ")";
        return g;
    }
    std::stable_sort(g.events.begin(), g.events.end(),
                     [](const Event& a, const Event& b) { return a.tick < b.tick; });
    // Within a tick, sets/unpatches must precede expects (the runner applies
    // them in that order); reject files that violate this instead of silently
    // misinterpreting them.
    auto isAssert = [](Event::Kind k) {
        return k == Event::Kind::Expect || k == Event::Kind::ExpectDisplay ||
               k == Event::Kind::ExpectMidi;
    };
    for (size_t k = 1; k < g.events.size(); k++) {
        const Event& prev = g.events[k - 1];
        const Event& cur  = g.events[k];
        if (cur.tick == prev.tick &&
            isAssert(prev.kind) &&
            !isAssert(cur.kind)) {
            g.parseError = "set/unpatch after expect at tick " + std::to_string(cur.tick) +
                           " (line " + std::to_string(cur.line) +
                           ") -- sets must precede expects within a tick";
            return g;
        }
    }
    return g;
}

} // namespace droid::gold
