#pragma once
// Symbolic MIDI event table for the golden-file harness. Translates the
// human-readable event words in `midisend`/`expectmidi` directives to/from the
// engine's 3-byte droid::MidiEvent. Channels are patch-facing 1..16 here and
// map to the status low nibble 0..15 at this edge. Sysex and any variable-length
// payload are intentionally unrepresentable (MidiEvent is a fixed 3-byte POD).
#include "src/midi.hpp"
#include <string>
#include <vector>

namespace droid::gold {

// Parse `words` (event name followed by its numeric args) into `out`.
// Returns true on success; on failure returns false and writes a human-readable
// reason to `err`. `words` must not be empty.
bool midiEventFromWords(const std::vector<std::string>& words,
                        droid::MidiEvent& out, std::string& err);

// Render a MidiEvent back to its symbolic form (e.g. "noteon 1 60 100",
// "bend 1 8192", "clock") for FAIL diagnostics. Unrecognised status bytes fall
// back to a raw "midi <status> <data1> <data2>" hex form.
std::string midiEventToString(droid::MidiEvent ev);

// Symbolic name of a physical MIDI port for FAIL diagnostics ("m18trs1",
// "x7usb", ...), matching the golden-file port tokens.
const char* midiPortName(uint8_t p);

} // namespace droid::gold
