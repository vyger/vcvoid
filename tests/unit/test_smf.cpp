#include "harness.hpp"
#include "src/smf.hpp"
#include <vector>
using namespace droid;

// Byte-building helpers for hand-crafted SMF fixtures.
static void be32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x);
}
static void be16(std::vector<uint8_t>& v, uint32_t x) { v.push_back(x >> 8); v.push_back(x); }
static void str4(std::vector<uint8_t>& v, const char* s) { for (int i = 0; i < 4; i++) v.push_back(s[i]); }
static std::vector<uint8_t> smf(uint16_t division, const std::vector<std::vector<uint8_t>>& tracks) {
    std::vector<uint8_t> v;
    str4(v, "MThd"); be32(v, 6); be16(v, tracks.size() > 1 ? 1 : 0);
    be16(v, tracks.size()); be16(v, division);
    for (auto& t : tracks) {
        str4(v, "MTrk"); be32(v, t.size());
        v.insert(v.end(), t.begin(), t.end());
    }
    return v;
}

TEST(smf_basic_track) {
    // delta 0 noteon 60 vel 100; delta 96 noteoff; EOT.
    std::vector<uint8_t> trk = {0x00, 0x90, 0x3c, 0x64, 0x60, 0x80, 0x3c, 0x00,
                                0x00, 0xff, 0x2f, 0x00};
    auto bytes = smf(96, {trk});
    SmfFile f;
    CHECK(parseSmf(bytes.data(), bytes.size(), f));
    CHECK(f.division == 96);
    CHECK(f.tracks.size() == 1);
    const SmfTrack& t = f.tracks[0];
    CHECK(t.hasChannelEvents);
    CHECK(t.events.size() == 2);
    CHECK(t.events[0].tick == 0 && t.events[0].status == 0x90 && t.events[0].data1 == 60);
    CHECK(t.events[1].tick == 96 && t.events[1].status == 0x80);
    CHECK(t.lengthTicks == 96);
    CHECK(t.byteLength == trk.size());
}

TEST(smf_running_status_and_tempo) {
    // Set Tempo 250000 (240 BPM); noteon 60 then RUNNING-STATUS noteon 64;
    // program change (1 data byte); EOT at 192.
    std::vector<uint8_t> trk = {
        0x00, 0xff, 0x51, 0x03, 0x03, 0xd0, 0x90,   // tempo 0x03d090 = 250000
        0x00, 0x90, 0x3c, 0x64,
        0x30, 0x40, 0x64,                            // running status: noteon 64 @48
        0x00, 0xc0, 0x05,                            // program change 5
        0x81, 0x10, 0xff, 0x2f, 0x00};               // delta varint 0x90 = 144 -> EOT @192
    auto bytes = smf(96, {trk});
    SmfFile f;
    CHECK(parseSmf(bytes.data(), bytes.size(), f));
    const SmfTrack& t = f.tracks[0];
    CHECK(t.events.size() == 3);
    CHECK(t.events[1].tick == 48 && t.events[1].status == 0x90 && t.events[1].data1 == 64);
    CHECK(t.events[2].status == 0xc0 && t.events[2].data1 == 5);
    CHECK(t.tempos.size() == 1 && t.tempos[0].usPerQuarter == 250000);
    CHECK(f.tempoMap.size() == 1);
    CHECK(t.lengthTicks == 192);
}

TEST(smf_meta_only_track_and_format1) {
    // Track 1: name meta only (empty). Track 2: one note.
    std::vector<uint8_t> meta = {0x00, 0xff, 0x03, 0x03, 'A', 'B', 'C',
                                 0x00, 0xff, 0x2f, 0x00};
    std::vector<uint8_t> notes = {0x00, 0x90, 0x3c, 0x64, 0x60, 0x80, 0x3c, 0x00,
                                  0x00, 0xff, 0x2f, 0x00};
    auto bytes = smf(96, {meta, notes});
    SmfFile f;
    CHECK(parseSmf(bytes.data(), bytes.size(), f));
    CHECK(f.tracks.size() == 2);
    CHECK(!f.tracks[0].hasChannelEvents);
    CHECK(f.tracks[1].hasChannelEvents);
}

TEST(smf_rejects_garbage) {
    SmfFile f;
    std::vector<uint8_t> junk = {0xde, 0xad, 0xbe, 0xef};
    CHECK(!parseSmf(junk.data(), junk.size(), f));
    // Truncated track payload.
    std::vector<uint8_t> trk = {0x00, 0x90, 0x3c};
    auto bytes = smf(96, {trk});
    CHECK(!parseSmf(bytes.data(), bytes.size(), f));
    // SMPTE division (high bit).
    std::vector<uint8_t> ok = {0x00, 0xff, 0x2f, 0x00};
    auto b2 = smf(96, {ok});
    b2[12] = 0xe7;   // division high byte
    CHECK(!parseSmf(b2.data(), b2.size(), f));
    // Missing EOT.
    std::vector<uint8_t> noEot = {0x00, 0x90, 0x3c, 0x64};
    auto b3 = smf(96, {noEot});
    CHECK(!parseSmf(b3.data(), b3.size(), f));
}
