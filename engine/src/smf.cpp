#include "smf.hpp"
#include <algorithm>

namespace droid {

namespace {
struct Reader {
    const uint8_t* p;
    size_t n;
    size_t pos = 0;
    bool fail = false;

    uint8_t u8() {
        if (pos >= n) { fail = true; return 0; }
        return p[pos++];
    }
    uint32_t u16() { uint32_t a = u8(), b = u8(); return (a << 8) | b; }
    uint32_t u32() { uint32_t a = u16(), b = u16(); return (a << 16) | b; }
    // MIDI variable-length quantity (max 4 bytes).
    uint32_t varint() {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) {
            uint8_t b = u8();
            v = (v << 7) | (b & 0x7f);
            if (!(b & 0x80)) return v;
        }
        fail = true;
        return v;
    }
    void skip(uint32_t k) {
        if (pos + k > n) { fail = true; pos = n; }
        else pos += k;
    }
    bool magic(const char* m) {
        for (int i = 0; i < 4; i++)
            if (u8() != (uint8_t)m[i]) return false;
        return !fail;
    }
};

// Data-byte count for a channel-event status (0x80..0xEF).
int dataBytes(uint8_t status) {
    uint8_t hi = status & 0xf0;
    return (hi == 0xc0 || hi == 0xd0) ? 1 : 2;
}
} // namespace

bool parseSmf(const uint8_t* data, size_t len, SmfFile& out) {
    out = SmfFile{};
    Reader r{data, len};
    if (!r.magic("MThd")) return false;
    uint32_t hlen = r.u32();
    if (hlen < 6) return false;
    r.u16();                          // format 0/1/2 — all accepted
    uint32_t ntrks = r.u16();
    uint32_t division = r.u16();
    if (r.fail || ntrks == 0) return false;
    if (division & 0x8000) return false;    // SMPTE timing unsupported
    if (division == 0) return false;
    out.division = (uint16_t)division;
    r.skip(hlen - 6);                 // tolerate an oversized header

    for (uint32_t t = 0; t < ntrks && !r.fail; t++) {
        if (!r.magic("MTrk")) return false;
        uint32_t tlen = r.u32();
        if (r.fail || r.pos + tlen > r.n) return false;
        size_t trackEnd = r.pos + tlen;

        SmfTrack trk;
        trk.byteLength = tlen;
        uint32_t tick = 0;
        uint8_t running = 0;
        bool sawEot = false;
        while (r.pos < trackEnd && !r.fail && !sawEot) {
            tick += r.varint();
            uint8_t b = r.u8();
            if (b == 0xff) {                          // meta
                uint8_t type = r.u8();
                uint32_t mlen = r.varint();
                if (type == 0x51 && mlen == 3) {
                    uint32_t us = (uint32_t)r.u8() << 16;
                    us |= (uint32_t)r.u8() << 8;
                    us |= r.u8();
                    if (us == 0) return false;
                    trk.tempos.push_back({tick, us});
                } else if (type == 0x2f) {
                    r.skip(mlen);
                    trk.lengthTicks = tick;
                    sawEot = true;
                } else {
                    r.skip(mlen);
                }
                running = 0;                          // meta cancels running status
            } else if (b == 0xf0 || b == 0xf7) {      // sysex: length-prefixed
                r.skip(r.varint());
                running = 0;
            } else {
                uint8_t status, d1;
                if (b & 0x80) { status = b; d1 = r.u8(); }
                else {                                 // running status
                    if (running < 0x80) return false;
                    status = running;
                    d1 = b;
                }
                if (status >= 0xf0) return false;      // stray system status
                running = status;
                uint8_t d2 = dataBytes(status) == 2 ? r.u8() : 0;
                if (r.fail) return false;
                // Data bytes must have the high bit clear (0x00..0x7f). A byte
                // >= 0x80 here means a malformed file; reject it rather than
                // store an out-of-range value that downstream code (e.g. the
                // note gate arrays in midifileplayer) would use as an index.
                if ((d1 & 0x80) || (d2 & 0x80)) return false;
                trk.events.push_back({tick, status, d1, d2});
                trk.hasChannelEvents = true;
            }
        }
        if (r.fail || !sawEot) return false;
        r.pos = trackEnd;                              // skip anything after EOT
        for (const SmfTempo& tp : trk.tempos) out.tempoMap.push_back(tp);
        out.tracks.push_back(std::move(trk));
    }
    std::stable_sort(out.tempoMap.begin(), out.tempoMap.end(),
                     [](const SmfTempo& a, const SmfTempo& b) { return a.tick < b.tick; });
    return !r.fail;
}

} // namespace droid
