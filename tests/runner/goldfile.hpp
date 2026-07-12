#pragma once
#include "src/midi.hpp"
#include <map>
#include <string>
#include <vector>

namespace droid::gold {

struct Event {
    enum class Kind { Set, Unpatch, Expect, Turn, Push, Move, Touch, Hold, ExpectDisplay,
                      MidiSend, ExpectMidi, Probe };
    Kind kind;
    long tick;
    std::string target;    // register/cable; E<ctrl>.<num> (Turn/Push); F<n> (Move/Touch/Hold);
                           // D<n> (ExpectDisplay, 1-based DB8E in chain order)
    float value = 0.0f;    // Set: value; Expect: expected; Turn: ±detents; Push: 0|1;
                           // Move: fader position 0..1; Touch/Hold: 0|1
                           // (Touch = finger on the plate below the fader: plate
                           // + hold; Hold = grabbing the fader itself, as a Rack
                           // drag does — no plate press);
                           // ExpectDisplay value/mode/font: the numeric operand
    float tol = 1e-6f;
    int line = 0;
    // ExpectDisplay only: `field` is one of header|text|value|mode|font|off;
    // `strValue` is the unquoted expected string for header/text (empty otherwise).
    std::string field;
    std::string strValue;
    // MidiSend/ExpectMidi only: `midi` is the 3-byte event, `midiPort` is the
    // droid::MidiPort as a uint8_t, and `midiNone` marks `expectmidi <port> none`
    // (assert the out-queue is empty; `midi` unused then).
    droid::MidiEvent midi;
    bool midiNone = false;
    uint8_t midiPort = 0;
    // Probe only: `probe none` marks signal loss (`value` unused then);
    // otherwise `value` carries the measured hz.
    bool probeNone = false;
};

struct GoldFile {
    int master = 16;
    bool x7 = true;              // X7 expander present unless header says `x7 0`
    float tickrate = 6000.0f;
    unsigned seed = 1;
    bool hasExpectRam = false;
    unsigned expectRam = 0;
    std::string expectLoadError;   // empty = expect successful load
    std::string expectWarning;
    // SD-card fixtures: `midifile <n> <hexbytes>` header lines (repeatable —
    // same n appends, so long files can be split across lines for comments).
    std::map<int, std::vector<uint8_t>> midiFiles;
    std::string patch;
    std::vector<Event> events;     // sorted by tick (stable)
    std::string parseError;        // non-empty = malformed gold file
};

GoldFile parse(const std::string& path);

} // namespace droid::gold
