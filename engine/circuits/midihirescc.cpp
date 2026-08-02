// midihirescc — 14-bit (hi-res) MIDI CC output. EXPERIMENTAL: vcvoid only, not
// a DROID firmware circuit (see docs/adr/0001-experimental-circuits.md). Spec:
// manual/circuits/experimental/midihirescc.md.
//
// Firmware `midiout` sends cc1..8 through frac127() — 7 bits. Its only hi-res
// paths are hardwired to fixed controllers (`volume` = CC#7 + CC#39,
// `pitchbend`). Driving anything else at 14 bits therefore means splitting the
// value in the patch with `math` circuits and hand-ordering the slots so the
// two bytes land adjacent on the wire (see patches/lfo-cc-midi.ini, which needs
// 24 `math` circuits and 2 `midiout` circuits for eight destinations). This
// circuit does that split at the emitter.
//
// The jack number IS the controller number: `hirescc22` sends CC#22 (MSB) and
// CC#54 (LSB). MIDI defines the hi-res pairing as n / n+32 for n = 0..31 only —
// 32..63 ARE the LSB half, and everything above is single-byte — so numbering
// the jacks this way makes an unpairable controller unrepresentable rather than
// merely documented.
//
// The range is 1..31, and it starts at 1 because the FORGE cannot express
// anything else: DroidFirmware::findJack (Forge main/droidfirmware.cpp:592-616)
// resolves an array jack by generating prefix+i for i = 1..count and never
// reads `start_at`. A block declared start_at=8 would validate as hirescc1..24
// in droidcheck while the engine accepted hirescc8..31 — the exact drift ADR
// 0001 exists to prevent (the firmware's own start_at=0 jack, calibrator's
// tune0, is mis-validated the same way; this is a Forge limitation, not ours).
// CC#0/Bank Select therefore has no jack, which is the right answer anyway.
// CC#6 (Data Entry) and CC#7 (Volume, which midiout already pairs) remain
// reachable and are flagged on the manual page rather than removed: holes are
// not expressible either, since one {prefix, count, start_at} block is
// contiguous and two blocks sharing a name would be unaddressable from C++ —
// Circuit::in() resolves a bare base name by exact match.
//
// CHOSEN BEHAVIOUR (this circuit has no hardware to be faithful to; the manual
// page states all of it):
//   * Atomic pairs. Both bytes are pushed inside one tick(), so they are
//     adjacent in the port's out-queue and cannot be interleaved with another
//     controller. This is the property the patch-level workaround cannot
//     guarantee structurally.
//   * Always BOTH bytes, even when only the LSB moved. Emitting the LSB alone
//     would halve the traffic for small movements, but a receiver that commits
//     on the MSB would then hold the update until something else moved it.
//   * Byte order per circuit (`lsbfirst`, default LSB-first): the order that
//     lets a receiver with a single pending-LSB latch apply the whole value at
//     once. `lsbfirst = 0` gives the MIDI spec's MSB-then-LSB order.
//   * Change detection on the RESOLVED 14-bit value, rate-limited by
//     `updaterate` (default 500/s — see USB-only below). A still input emits
//     nothing at all.
//   * NO initial send: the baseline is seeded silently on the first tick, so a
//     value that never moves is never transmitted. `midiout`'s initial send
//     exists together with `delayinitialccs`, a jack whose reason (waiting for
//     real hardware to finish booting) vcvoid does not reproduce.
//   * USB only. A classical MIDI cable carries 3125 bytes/s (midiout.md line
//     386); one pair is 6 bytes, so DIN could not sustain hi-res updates for
//     more than a couple of controllers. Offering a `trs` jack would be
//     offering a rate the cable cannot keep.
//   * No `select`/`selectat`: in `midiout` they gate only button/LED
//     processing, and this circuit has neither.
//   * No MIDI hardware in the chain (no MASTER18, no X7) -> silent no-op, the
//     patch still loads. Same as midiout/midiin/midithrough.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>
#include <cstdint>

namespace droid {

class MidiHiresCC : public Circuit {
    // hirescc1 … hirescc31: jack index (1-based within the block) k+1 carries
    // controller kFirstCC + k; its LSB partner is that plus kLsbOffset.
    static constexpr int kFirstCC = 1;
    static constexpr int kNumSlots = 31;
    static constexpr int kLsbOffset = 32;

    static int clampi(long v, long lo, long hi) { return (int)(v < lo ? lo : v > hi ? hi : v); }
    // 0..1 CV -> 0..16383. Same construction as midiout's `volume`
    // (midiout.cpp:361): round, then clamp, so 1.0 lands exactly on 16383 and
    // an A*B+C overshoot saturates instead of aliasing.
    static int frac14(float v) { return clampi(std::lround(v * 16383.0f), 0, 16383); }

    // Per-controller change-detection state. `inited` false means the baseline
    // has not been observed yet; the first observation seeds it WITHOUT
    // emitting (no initial send).
    struct Slot { bool inited = false; int lastVal = -1; uint64_t lastTick = 0; };
    Slot slot_[kNumSlots];

    uint8_t outMask_ = 0;                              // bit p = physical port p
    uint8_t ch_ = 0;                                   // channel low nibble

    void emitCC(EngineState& s, uint8_t num, uint8_t val) {
        MidiEvent e{uint8_t(0xB0 | ch_), num, val};
        for (int p = 0; p < kNumMidiPorts; p++)
            if (outMask_ & (1u << p)) s.midi.out[p].push(e);
    }

    // USB ports only, using midiout's port NUMBERING (midiout.md lines 98-133):
    // 1..n = the nth USB port in user order (the master's own first, the X7's
    // after), 10 = all of them, anything else = none.
    void computePorts(EngineState& s) {
        int ports[3];
        int n = portsOfKind(s.midi.master18, s.midi.x7, PortKind::Usb, ports);
        int v = (int)std::lround(in("usb").value(s));
        outMask_ = 0;
        if (v == 10) { for (int i = 0; i < n; i++) outMask_ |= uint8_t(1u << ports[i]); }
        else if (v >= 1 && v <= n) outMask_ = uint8_t(1u << ports[v - 1]);
    }

public:
    void tick(EngineState& s) override {
        if (!s.midi.available()) return;               // no MIDI hardware: emit nothing

        computePorts(s);
        ch_ = (uint8_t)(clampi(std::lround(in("channel").value(s)), 1, 16) - 1);
        bool lsbFirst = in("lsbfirst").value(s) >= kGateHighThreshold;

        float ur = in("updaterate").value(s);
        bool updatesOn = ur > 0.0f;                    // 0 or negative: stop updating
        long period = updatesOn ? std::max(1L, std::lround(s.tickRateHz / (double)ur)) : 1;

        for (int k = 0; k < kNumSlots; k++) {
            if (!in("hirescc", k + 1).connected()) continue;
            int v14 = frac14(in("hirescc", k + 1).value(s));
            Slot& c = slot_[k];
            if (!c.inited) {                           // baseline only — never emitted
                c.inited = true; c.lastVal = v14; c.lastTick = s.tick;
                continue;
            }
            if (!updatesOn) continue;
            if (v14 == c.lastVal) continue;
            if ((int64_t)(s.tick - c.lastTick) < period) continue;

            uint8_t msbNum = (uint8_t)(kFirstCC + k);
            uint8_t lsbNum = (uint8_t)(msbNum + kLsbOffset);
            uint8_t msb = (uint8_t)(v14 >> 7), lsb = (uint8_t)(v14 & 0x7F);
            if (lsbFirst) { emitCC(s, lsbNum, lsb); emitCC(s, msbNum, msb); }
            else          { emitCC(s, msbNum, msb); emitCC(s, lsbNum, lsb); }
            c.lastVal = v14; c.lastTick = s.tick;
        }
    }
};

DROID_REGISTER_CIRCUIT(midihirescc, MidiHiresCC)

} // namespace droid
