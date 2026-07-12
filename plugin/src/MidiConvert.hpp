#pragma once
#include "src/midi.hpp"
#include <midi.hpp>

// Rack <-> DROID MIDI wire-format conversion, shared by the X7 (chain-frame
// MIDI, X7.cpp) and the MASTER18 (native ports, Master18.cpp). The rules here
// are MIDI-protocol facts, not per-module policy, so they live in one place:
//   * inbound: only 1..3-byte non-sysex messages fit a droid::MidiEvent
//     (a fixed 3-byte POD — sysex is excluded by construction, midithrough.md);
//   * outbound: message size derives from the status byte — realtime (>= 0xF8)
//     is 1 byte, program change / channel aftertouch (0xCn / 0xDn) are 2,
//     everything else 3.
namespace dmidi {

// Fill `e` from a Rack message; false if it can't be represented (sysex or
// oversized) and must be skipped.
inline bool fromRack(const rack::midi::Message& msg, droid::MidiEvent& e) {
    size_t sz = msg.bytes.size();
    if (sz < 1 || sz > 3) return false;   // non-sysex 1..3-byte only
    e.status = msg.bytes[0];
    // Mask data bytes to 0..0x7f. A malformed/hostile MIDI source (virtual
    // port, buggy driver) can deliver a data byte with the high bit set;
    // consumers such as midiin use data1 as an array index (heldVel_[note]),
    // so an unmasked value would be an out-of-bounds access.
    e.data1  = sz > 1 ? (msg.bytes[1] & 0x7f) : 0;
    e.data2  = sz > 2 ? (msg.bytes[2] & 0x7f) : 0;
    return true;
}

inline rack::midi::Message toRack(const droid::MidiEvent& e) {
    uint8_t hi = e.status & 0xf0;
    int size = 3;
    if (e.status >= 0xf8) size = 1;               // realtime
    else if (hi == 0xc0 || hi == 0xd0) size = 2;  // pgm / chan aftertouch
    rack::midi::Message m;
    m.setSize(size);
    m.bytes[0] = e.status;
    if (size > 1) m.bytes[1] = e.data1;
    if (size > 2) m.bytes[2] = e.data2;
    return m;
}

} // namespace dmidi
