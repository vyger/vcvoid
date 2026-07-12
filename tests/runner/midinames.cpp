#include "midinames.hpp"
#include <cstdio>

namespace droid::gold {

// Parse a decimal integer in [lo, hi]. On failure, writes a reason to `err`
// (naming `what`) and returns false.
static bool parseInt(const std::string& s, long lo, long hi, const char* what,
                     long& out, std::string& err) {
    if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos) {
        err = std::string(what) + " '" + s + "' is not a non-negative integer";
        return false;
    }
    long v = 0;
    try { v = std::stol(s); }
    catch (...) { err = std::string(what) + " '" + s + "' out of range"; return false; }
    if (v < lo || v > hi) {
        err = std::string(what) + " " + std::to_string(v) + " out of range [" +
              std::to_string(lo) + ".." + std::to_string(hi) + "]";
        return false;
    }
    out = v;
    return true;
}

// Channel-voice helper: parse the 1..16 channel and OR it into the status base.
static bool chanStatus(const std::string& s, uint8_t base, uint8_t& status,
                       std::string& err) {
    long ch;
    if (!parseInt(s, 1, 16, "channel", ch, err)) return false;
    status = uint8_t(base | uint8_t(ch - 1));
    return true;
}

bool midiEventFromWords(const std::vector<std::string>& w,
                        droid::MidiEvent& out, std::string& err) {
    out = droid::MidiEvent{};
    if (w.empty()) { err = "empty MIDI event"; return false; }
    const std::string& ev = w[0];

    auto need = [&](size_t n) -> bool {
        if (w.size() != n) {
            err = "'" + ev + "' expects " + std::to_string(n - 1) +
                  " arg(s), got " + std::to_string(w.size() - 1);
            return false;
        }
        return true;
    };

    // Channel-voice, three bytes: <ch> <data1> <data2>.
    auto note = [&](uint8_t base, const char* d1name, const char* d2name) -> bool {
        if (!need(4)) return false;
        long d1, d2;
        if (!chanStatus(w[1], base, out.status, err)) return false;
        if (!parseInt(w[2], 0, 127, d1name, d1, err)) return false;
        if (!parseInt(w[3], 0, 127, d2name, d2, err)) return false;
        out.data1 = uint8_t(d1);
        out.data2 = uint8_t(d2);
        return true;
    };
    // Channel-voice, two bytes: <ch> <data1>.
    auto one = [&](uint8_t base, const char* d1name) -> bool {
        if (!need(3)) return false;
        long d1;
        if (!chanStatus(w[1], base, out.status, err)) return false;
        if (!parseInt(w[2], 0, 127, d1name, d1, err)) return false;
        out.data1 = uint8_t(d1);
        return true;
    };
    // Realtime, status only.
    auto realtime = [&](uint8_t status) -> bool {
        if (!need(1)) return false;
        out.status = status;
        return true;
    };

    if (ev == "noteon")         return note(0x90, "note", "velocity");
    if (ev == "noteoff")        return note(0x80, "note", "velocity");
    if (ev == "polyaftertouch") return note(0xA0, "note", "value");
    if (ev == "cc")             return note(0xB0, "controller", "value");
    if (ev == "program")        return one(0xC0, "program");
    if (ev == "aftertouch")     return one(0xD0, "value");
    if (ev == "bend") {
        if (!need(3)) return false;
        long v14;
        if (!chanStatus(w[1], 0xE0, out.status, err)) return false;
        if (!parseInt(w[2], 0, 16383, "bend value", v14, err)) return false;
        out.data1 = uint8_t(v14 & 0x7F);
        out.data2 = uint8_t(v14 >> 7);
        return true;
    }
    if (ev == "clock")    return realtime(0xF8);
    if (ev == "start")    return realtime(0xFA);
    if (ev == "continue") return realtime(0xFB);
    if (ev == "stop")     return realtime(0xFC);

    err = "unknown MIDI event '" + ev + "'";
    return false;
}

const char* midiPortName(uint8_t p) {
    switch ((droid::MidiPort)p) {
        case droid::MidiPort::M18Usb:  return "m18usb";
        case droid::MidiPort::M18Trs1: return "m18trs1";
        case droid::MidiPort::M18Trs2: return "m18trs2";
        case droid::MidiPort::X7Usb:   return "x7usb";
        case droid::MidiPort::X7Trs:   return "x7trs";
    }
    return "?";
}

std::string midiEventToString(droid::MidiEvent ev) {
    uint8_t st = ev.status;
    // Realtime / system messages: status only.
    switch (st) {
        case 0xF8: return "clock";
        case 0xFA: return "start";
        case 0xFB: return "continue";
        case 0xFC: return "stop";
        default: break;
    }
    int ch = (st & 0x0F) + 1;
    uint8_t hi = st & 0xF0;
    char buf[64];
    switch (hi) {
        case 0x90: std::snprintf(buf, sizeof buf, "noteon %d %d %d", ch, ev.data1, ev.data2); return buf;
        case 0x80: std::snprintf(buf, sizeof buf, "noteoff %d %d %d", ch, ev.data1, ev.data2); return buf;
        case 0xA0: std::snprintf(buf, sizeof buf, "polyaftertouch %d %d %d", ch, ev.data1, ev.data2); return buf;
        case 0xB0: std::snprintf(buf, sizeof buf, "cc %d %d %d", ch, ev.data1, ev.data2); return buf;
        case 0xC0: std::snprintf(buf, sizeof buf, "program %d %d", ch, ev.data1); return buf;
        case 0xD0: std::snprintf(buf, sizeof buf, "aftertouch %d %d", ch, ev.data1); return buf;
        case 0xE0: {
            int v14 = int(ev.data1) | (int(ev.data2) << 7);
            std::snprintf(buf, sizeof buf, "bend %d %d", ch, v14);
            return buf;
        }
        default: break;
    }
    std::snprintf(buf, sizeof buf, "midi 0x%02X 0x%02X 0x%02X", ev.status, ev.data1, ev.data2);
    return buf;
}

} // namespace droid::gold
