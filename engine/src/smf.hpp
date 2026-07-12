#pragma once
// Standard MIDI File (SMF) parser for the midifileplayer circuit. Parses the
// MThd header and every MTrk chunk into absolute-tick channel events plus
// set-tempo metas. Deliberately minimal: sysex payloads and all metas other
// than Set Tempo (0x51) and End Of Track (0x2F) are skipped; SMPTE division
// (high bit set) is rejected as unsupported. Pure and allocation-only-at-parse
// (no EngineState/Rack includes) — headless unit-testable.
#include <cstdint>
#include <vector>

namespace droid {

struct SmfEvent {
    uint32_t tick = 0;      // absolute, in file ticks (division = ticks/quarter)
    uint8_t status = 0;     // channel event 0x80..0xEF (never meta/sysex)
    uint8_t data1 = 0;
    uint8_t data2 = 0;
};

struct SmfTempo {
    uint32_t tick = 0;
    uint32_t usPerQuarter = 500000;   // Set Tempo payload
};

struct SmfTrack {
    std::vector<SmfEvent> events;     // channel events, ascending tick
    std::vector<SmfTempo> tempos;     // Set Tempo metas in THIS track
    uint32_t lengthTicks = 0;         // tick of the End Of Track meta
    uint32_t byteLength = 0;          // MTrk chunk payload size (DROID's 6000-byte cap)
    bool hasChannelEvents = false;    // "non-empty" per the manual's track counting
};

struct SmfFile {
    uint16_t division = 96;           // PPQ (ticks per quarter note)
    std::vector<SmfTrack> tracks;
    // Set Tempo metas merged from ALL tracks, ascending tick (format-1 files
    // keep the tempo map in track 1, which is typically meta-only and thus
    // never the played track).
    std::vector<SmfTempo> tempoMap;
};

// Parses `len` bytes of SMF data. Returns false on any structural error
// (bad magic, truncated chunk, SMPTE division, malformed varint/event).
bool parseSmf(const uint8_t* data, size_t len, SmfFile& out);

} // namespace droid
