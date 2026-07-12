#pragma once
// Engine-side MIDI seam for M5. Sysex is deliberately excluded everywhere: a
// MidiEvent is a fixed 3-byte status/data1/data2 POD (system-exclusive and
// other variable-length payloads are dropped at the adapter, never modelled
// here). Five physical ports: USB + 2x TRS on the MASTER18, USB + TRS on the
// X7; which exist depends on the hardware configuration (master type + x7).
#include <cstdint>

namespace droid {

// A single 3-byte MIDI message (channel-voice / system-common / realtime).
// POD, all-zero default so `MidiEvent{}` is a valid empty slot.
struct MidiEvent {
    uint8_t status = 0;
    uint8_t data1 = 0;
    uint8_t data2 = 0;
};

// Physical MIDI ports across every hardware configuration. Values are
// load-bearing (array indices + chain protocol); do not renumber. The
// MASTER18 carries ports 0-2 natively; the X7 expander carries 3-4.
enum class MidiPort : uint8_t { M18Usb = 0, M18Trs1 = 1, M18Trs2 = 2, X7Usb = 3, X7Trs = 4 };
constexpr int kNumMidiPorts = 5;

enum class PortKind : uint8_t { Usb, Trs };

// Valid physical ports of one kind in USER-NUMBER order (manual
// midiin.md "Port selection": the master's own ports index first, the X7's
// after). Writes them to out[]; returns the count (0..3).
inline int portsOfKind(bool master18, bool x7, PortKind k, int out[3]) {
    int n = 0;
    if (k == PortKind::Usb) {
        if (master18) out[n++] = (int)MidiPort::M18Usb;
        if (x7)       out[n++] = (int)MidiPort::X7Usb;
    } else {
        if (master18) { out[n++] = (int)MidiPort::M18Trs1; out[n++] = (int)MidiPort::M18Trs2; }
        if (x7)       out[n++] = (int)MidiPort::X7Trs;
    }
    return n;
}

// User port number (1-based) -> physical port, or -1 if no such port.
inline int resolvePort(bool master18, bool x7, PortKind k, int userNum) {
    int ports[3];
    int n = portsOfKind(master18, x7, k, ports);
    return (userNum >= 1 && userNum <= n) ? ports[userNum - 1] : -1;
}

constexpr int kMidiQueueCap = 64;

// Fixed-capacity FIFO ring of MidiEvents. On overflow the OLDEST event is
// dropped (head advances) and `dropped` is incremented, so the most recent
// kMidiQueueCap events are always retained.
struct MidiQueue {
    MidiEvent buf[kMidiQueueCap];
    int head = 0;                 // index of oldest element
    int size = 0;                 // number of buffered elements
    uint32_t dropped = 0;         // total events discarded on overflow

    void push(MidiEvent e) {
        if (size == kMidiQueueCap) {
            head = (head + 1) % kMidiQueueCap;   // drop oldest
            size--;
            dropped++;
        }
        int tail = (head + size) % kMidiQueueCap;
        buf[tail] = e;
        size++;
    }

    bool pop(MidiEvent& out) {
        if (size == 0) return false;
        out = buf[head];
        head = (head + 1) % kMidiQueueCap;
        size--;
        return true;
    }

    void clear() { head = 0; size = 0; }
};

// All MIDI state carried in EngineState. `x7` records whether an X7 expander is
// present in the controller chain (survives patch reloads — set by the chain,
// not the patch). Per port: an inbound queue (adapter -> engine), an outbound
// queue (engine -> adapter), and a per-tick snapshot of inbound events that
// circuits read during tick().
struct MidiState {
    bool x7 = false;
    bool master18 = false;          // set once by the Engine ctor
    // MIDI circuits run iff any MIDI hardware exists in the configuration.
    bool available() const { return x7 || master18; }
    MidiQueue in[kNumMidiPorts];
    MidiQueue out[kNumMidiPorts];
    MidiEvent tickEv[kNumMidiPorts][kMidiQueueCap];
    uint16_t tickCount[kNumMidiPorts] = {};
};

} // namespace droid
