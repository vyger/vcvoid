// midithrough — Forward MIDI events from input ports to one or more outputs.
// Spec: manual/circuits/midithrough.md. Port-to-port forwarding with merge,
// auto-detect and broadcast; Sysex is excluded (a MidiEvent is a fixed 3-byte
// POD, so nothing to drop). Requires MIDI hardware — an X7 in the chain or a
// MASTER18 (native ports): with neither this is a runtime no-op (the patch
// still loads).
//
// Port model (midithrough.md port tables): from*/to* take master-aware port
// NUMBERS of their kind — 0/omitted = disabled, 1..n = specific port,
// from* = 10 -> auto-detect, to* = 10 -> broadcast to all ports of the kind.
// Auto-detect locks to the first valid port of the kind that carries data and
// stays locked (the manual's "until the data stops for a second" re-detect is
// NOT modelled — chosen reading, logged in circuits-status.yaml).
// Merge order when both from* feed one target: the per-tick snapshot order,
// TRS then USB (the manual leaves the interleave unspecified).
#include "../src/registry.hpp"
#include <cmath>

namespace droid {

class MidiThrough : public Circuit {
    int lockedFromTrs_ = -1, lockedFromUsb_ = -1;   // auto-detect locks per kind

    // Source port of one kind: -1 = none.
    int fromPort(EngineState& s, PortKind k, const char* jack, int& locked) {
        int v = (int)lroundf(in(jack).value(s));    // omitted -> 0 -> off
        if (v == 0) return -1;
        if (v != 10) return resolvePort(s.midi.master18, s.midi.x7, k, v);
        if (locked < 0) {
            int ports[3];
            int n = portsOfKind(s.midi.master18, s.midi.x7, k, ports);
            for (int i = 0; i < n; i++)
                if (s.midi.tickCount[ports[i]] > 0) { locked = ports[i]; break; }
        }
        return locked;
    }

    // Target-port bitmask of one kind (bit p = physical port p).
    uint8_t toMask(EngineState& s, PortKind k, const char* jack) {
        int v = (int)lroundf(in(jack).value(s));
        int ports[3];
        int n = portsOfKind(s.midi.master18, s.midi.x7, k, ports);
        uint8_t m = 0;
        if (v == 10) { for (int i = 0; i < n; i++) m |= uint8_t(1u << ports[i]); }
        else if (v >= 1 && v <= n) m = uint8_t(1u << ports[v - 1]);
        return m;
    }

public:
    void tick(EngineState& s) override {
        if (!s.midi.available()) return;   // no MIDI hardware: forward nothing
        int srcTrs = fromPort(s, PortKind::Trs, "fromtrs", lockedFromTrs_);
        int srcUsb = fromPort(s, PortKind::Usb, "fromusb", lockedFromUsb_);
        uint8_t targets = toMask(s, PortKind::Trs, "totrs") |
                          toMask(s, PortKind::Usb, "tousb");
        // TRS-then-USB snapshot order, so a merged stream on one target keeps
        // TRS events ahead of USB events for the same tick.
        for (int src : {srcTrs, srcUsb}) {
            if (src < 0) continue;
            for (int dst = 0; dst < kNumMidiPorts; dst++)
                if (targets & (1u << dst))
                    for (uint16_t k = 0; k < s.midi.tickCount[src]; k++)
                        s.midi.out[dst].push(s.midi.tickEv[src][k]);
        }
    }
};

DROID_REGISTER_CIRCUIT(midithrough, MidiThrough)

} // namespace droid
