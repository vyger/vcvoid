#include "harness.hpp"
#include "src/chain.hpp"
#include "src/registers.hpp"
#include "src/engine.hpp"
#include <cmath>
#include <cstring>
#include <memory>
#include <type_traits>

using namespace droid::chain;

TEST(chain_prepend_shift) {
    UpstreamMessage in;                      // empty chain to my right
    UpstreamBlock me; me.modelId = MP2B8; me.pots[0] = 0.5f; me.buttons = 0b101;
    UpstreamMessage out;
    prependUpstream(me, in, out);
    CHECK(out.count == 1);
    CHECK(out.block[0].modelId == MP2B8);
    CHECK(out.block[0].buttons == 0b101);

    UpstreamBlock g8; g8.modelId = MG8; g8.gates[3] = 1.0f;
    UpstreamMessage out2;
    prependUpstream(g8, out, out2);          // G8 sits between me and the master
    CHECK(out2.count == 2);
    CHECK(out2.block[0].modelId == MG8);     // block[0] = nearest master
    CHECK(out2.block[1].modelId == MP2B8);

    DownstreamMessage d; d.count = 2;
    d.block[0].modelId = MG8;  d.block[0].gates[3] = 1.0f;
    d.block[1].modelId = MP2B8; d.block[1].leds[7] = 0.25f;
    DownstreamBlock mine; DownstreamMessage rest;
    shiftDownstream(d, mine, rest);
    CHECK(mine.modelId == MG8);
    CHECK(rest.count == 1);
    CHECK(rest.block[0].modelId == MP2B8);
    CHECK(std::fabs(rest.block[0].leds[7] - 0.25f) < 1e-6f);
    DownstreamMessage empty; shiftDownstream(empty, mine, rest);
    CHECK(mine.modelId == None);             // starved chain reads a cleared block
    CHECK(rest.count == 0);
}

// The relay's end-of-chain case (issue #32): a module with no right neighbour
// passes out=null, so the 11 KB tail is never built. block[0] must still reach
// `mine` exactly as it does when a tail is requested.
TEST(chain_shift_null_out) {
    DownstreamMessage d; d.count = 2;
    d.block[0].modelId = MP2B8; d.block[0].leds[3] = 0.75f;
    d.block[1].modelId = MB32;
    DownstreamBlock mine;
    shiftDownstream(d, mine, nullptr);
    CHECK(mine.modelId == MP2B8);
    CHECK(std::fabs(mine.leds[3] - 0.75f) < 1e-6f);

    DownstreamMessage empty;                 // starved chain, still no tail wanted
    shiftDownstream(empty, mine, nullptr);
    CHECK(mine.modelId == None);             // cleared block, not frozen at the last state
}

TEST(chain_overflow_clamps) {
    UpstreamMessage m; m.count = kMaxChainModules;   // already full
    for (int i = 0; i < kMaxChainModules; i++) m.block[i].modelId = MB32;
    UpstreamBlock me; me.modelId = MP2B8;
    UpstreamMessage out;
    prependUpstream(me, m, out);
    CHECK(out.count == kMaxChainModules);            // farthest module dropped, no overrun
    CHECK(out.block[0].modelId == MP2B8);
}

TEST(chain_shift_overflow_clamps) {
    DownstreamMessage d; d.count = 255;              // untrusted wire count, over kMaxChainModules
    for (int i = 0; i < kMaxChainModules; i++) d.block[i].modelId = MB32;
    DownstreamBlock mine; DownstreamMessage rest;
    shiftDownstream(d, mine, rest);
    CHECK(rest.count == kMaxChainModules - 1);       // clamped, no over-read/write of block[]
}

// ---- relay clock (issue #32) -----------------------------------------
// tickSeq travels rightward with the LED data and dirty travels leftward with
// the controls; together they are what lets a module skip the multi-KB block
// copies on a frame where nothing can have changed.
TEST(chain_relay_clock_carriers) {
    DownstreamMessage d; d.count = 2; d.tickSeq = 77;
    d.block[0].modelId = MP2B8; d.block[1].modelId = MB32;
    DownstreamBlock mine; DownstreamMessage rest;
    shiftDownstream(d, mine, rest);
    CHECK(rest.tickSeq == 77);               // the tick reaches the next hop

    DownstreamMessage starved; starved.tickSeq = 78;
    shiftDownstream(starved, mine, rest);
    CHECK(rest.tickSeq == 78);               // ... even when there is no block to pass on

    UpstreamMessage from; from.count = 1; from.dirty = 1;
    from.block[0].modelId = MB32;
    UpstreamBlock me; me.modelId = MP2B8;
    UpstreamMessage out;
    prependUpstream(me, from, out);
    CHECK(out.dirty == 1);                   // someone to my right changed: keep relaying
    from.dirty = 0;
    prependUpstream(me, from, out);
    CHECK(out.dirty == 0);                   // ... and stop once they go quiet
}

TEST(chain_upstream_gate) {
    UpstreamGate g;
    UpstreamBlock mine; std::memset(&mine, 0, sizeof mine);
    mine.modelId = MP2B8;
    const int64_t dest = 7;                  // one unchanging left neighbour

    // First frame: the baseline is zeroed and my block is not, so publish.
    auto d = g.decide(mine, 0, 0, dest);
    CHECK(d.publish && d.dirty);
    g.notePublished(mine, 0, d.dirty, dest);

    // Trailing edge: one more publish carrying dirty = 0, so the neighbour's
    // buffer does not sit with dirty stuck at 1 forever.
    d = g.decide(mine, 0, 0, dest);
    CHECK(d.publish && !d.dirty);
    g.notePublished(mine, 0, d.dirty, dest);

    // Now genuinely idle — this is the whole point of the gate.
    d = g.decide(mine, 0, 0, dest);
    CHECK(!d.publish && !d.dirty);

    // A control moves.
    mine.pots[0] = 0.5f;
    d = g.decide(mine, 0, 0, dest);
    CHECK(d.publish && d.dirty);
    g.notePublished(mine, 0, d.dirty, dest);

    // Someone to my right flags a change even though my own block is unchanged.
    d = g.decide(mine, 1, 0, dest);
    CHECK(d.publish && d.dirty);
    g.notePublished(mine, 0, d.dirty, dest);
    g.notePublished(mine, 0, false, dest);   // settle the trailing edge
    CHECK(!g.decide(mine, 0, 0, dest).publish);

    // A module UNPLUGGED to my right: nobody is left to set dirty, so only the
    // chain length betrays it. Without this the master keeps the old chain.
    g.notePublished(mine, 3, false, dest);
    d = g.decide(mine, 0, 2, dest);
    CHECK(d.publish && d.dirty);
}

// Issue #59: a left neighbour REPLACED by a different module is invisible to a
// purely content-based gate — same block, same chain length, nothing dirty —
// yet the newcomer has never been written to and goes on serving whatever its
// own buffer last held. The destination's identity has to be part of the
// decision.
TEST(chain_upstream_gate_neighbour_swap) {
    UpstreamGate g;
    UpstreamBlock mine; std::memset(&mine, 0, sizeof mine);
    mine.modelId = MB32;
    const int64_t first = 4, second = 9;     // two distinct left neighbours
    auto d = g.decide(mine, 0, 0, first);
    g.notePublished(mine, 0, d.dirty, first);
    g.notePublished(mine, 0, false, first);        // settle the trailing edge
    CHECK(!g.decide(mine, 0, 0, first).publish);   // idle, same neighbour: silent

    // Same content, same chain length, different neighbour: publish, and stamp
    // dirty so the fresh message keeps rippling on toward the master.
    d = g.decide(mine, 0, 0, second);
    CHECK(d.publish && d.dirty);
    g.notePublished(mine, 0, d.dirty, second);
    g.notePublished(mine, 0, false, second);
    CHECK(!g.decide(mine, 0, 0, second).publish);  // and settles again

    // Losing the neighbour counts as a change too, so the gate never believes
    // the module it comes back to has already had this message.
    CHECK(g.decide(mine, 0, 0, kNoNeighbour).publish);
}

// A block differing ONLY in a field sitting next to padding must still be seen.
// The gate compares with memcmp, which reads padding bytes too, so both sides
// have to be memset — NSDMI value-init does not zero padding.
TEST(chain_upstream_gate_padding) {
    UpstreamGate g;
    UpstreamBlock a; std::memset(&a, 0, sizeof a);
    a.modelId = MB32;                        // uint8_t followed by padding, then buttons
    const int64_t dest = 3;
    g.notePublished(a, 0, false, dest);
    CHECK(!g.decide(a, 0, 0, dest).publish); // identical block: no publish
    UpstreamBlock b = a;
    b.buttons = 1u << 31;                    // the field right after that padding
    CHECK(g.decide(b, 0, 0, dest).publish);
}

// ---- hot-plugging a whole chain (issue #59) ---------------------------
// A headless stand-in for Rack's expander plumbing, just enough of it to
// rearrange a rack and read what the master ends up seeing. Each module owns
// the producer/consumer pair Rack allocates on its RIGHT face: the right
// neighbour writes the producer and asks for a flip, the owner reads the
// consumer, and every requested flip happens once per frame after all modules
// have stepped — so data moves exactly one hop per frame, as in Rack.
namespace {
struct SimModule {
    int64_t id;                             // rack::Module::id: unique, never reused
    UpstreamBlock block;
    UpstreamMessage bufA, bufB;
    UpstreamMessage* producer = &bufA;      // written by my right neighbour
    UpstreamMessage* consumer = &bufB;      // what I read this frame
    bool flipRequested = false;
    UpstreamRelay relay;                    // controllers only
    NeighbourId source;                     // master only (MasterBase's chainSource_)
    int publishes = 0;                      // gate activity, for the idle check

    SimModule(int64_t moduleId, ModelId m) : id(moduleId) {
        std::memset(&block, 0, sizeof block);
        block.modelId = m;
    }
};

struct SimRack {
    // mods[0] is the master; the rest run rightward, the way a DROID system is
    // physically laid out.
    std::vector<std::unique_ptr<SimModule>> mods;
    int64_t nextId = 1;

    SimRack(std::initializer_list<ModelId> models) {
        for (ModelId m : models) insert(int(mods.size()), m);
    }
    SimModule* insert(int at, ModelId m) {
        mods.insert(mods.begin() + at, std::unique_ptr<SimModule>(new SimModule(nextId++, m)));
        return mods[at].get();
    }
    void remove(int at) { mods.erase(mods.begin() + at); }

    static int64_t idOf(SimModule* m) { return m ? m->id : kNoNeighbour; }

    void step() {
        for (size_t i = 0; i < mods.size(); i++) {
            SimModule* me = mods[i].get();
            SimModule* right = (i + 1 < mods.size()) ? mods[i + 1].get() : nullptr;
            if (i == 0) {   // the master publishes nothing; it only reads
                if (me->source.changed(idOf(right))) {
                    me->consumer->count = 0;
                    me->consumer->dirty = 0;
                }
                continue;
            }
            SimModule* left = mods[i - 1].get();
            if (me->relay.step(me->block, idOf(right), *me->consumer,
                               idOf(left), left->producer)) {
                left->flipRequested = true;
                me->publishes++;
            }
        }
        for (auto& m : mods)
            if (m->flipRequested) { std::swap(m->producer, m->consumer); m->flipRequested = false; }
    }
    void settle(int frames = 12) { for (int i = 0; i < frames; i++) step(); }
    std::vector<std::string> chain() const { return controllerModels(*mods[0]->consumer); }
    int publishes() const {
        int n = 0;
        for (auto& m : mods) n += m->publishes;
        return n;
    }
    void resetPublishes() { for (auto& m : mods) m->publishes = 0; }
};
}  // namespace

TEST(chain_hotplug_insert_and_remove) {
    SimRack r{None /*master*/, MP2B8, MB32};
    r.settle();
    CHECK(r.chain() == std::vector<std::string>({"p2b8", "b32"}));

    // Once settled, the gate is the whole point: nothing moves, nobody writes.
    r.resetPublishes();
    r.settle(20);
    CHECK(r.publishes() == 0);
    CHECK(r.chain() == std::vector<std::string>({"p2b8", "b32"}));

    // Insert an M4 between them. The B32's content does not change one bit —
    // only the module it now publishes into — so before #59 the B32 fell out of
    // the chain and the M4 took its place.
    r.insert(2, MM4);
    r.settle();
    CHECK(r.chain() == std::vector<std::string>({"p2b8", "m4", "b32"}));

    // Pull the M4 back out and close the gap. The P2B8's consumer buffer still
    // holds the M4's last message: without ageing it out, that deleted module
    // haunts the chain — the phantom in the bug report.
    r.remove(2);
    r.settle();
    CHECK(r.chain() == std::vector<std::string>({"p2b8", "b32"}));

    // ... and the restored chain is quiet again.
    r.resetPublishes();
    r.settle(20);
    CHECK(r.publishes() == 0);
    CHECK(r.chain() == std::vector<std::string>({"p2b8", "b32"}));
}

// Deleting the module at the END of the chain, and then the one in the middle:
// each time the master must be left with exactly what is physically there.
TEST(chain_hotplug_remove_drops_module) {
    SimRack r{None /*master*/, MP2B8, MM4, MB32};
    r.settle();
    CHECK(r.chain() == std::vector<std::string>({"p2b8", "m4", "b32"}));

    r.remove(3);                             // the B32 at the far end
    r.settle();
    CHECK(r.chain() == std::vector<std::string>({"p2b8", "m4"}));

    r.remove(1);                             // the P2B8, nearest the master
    r.settle();
    CHECK(r.chain() == std::vector<std::string>({"m4"}));

    r.remove(1);                             // ... and the last one standing
    r.settle();
    CHECK(r.chain().empty());
}

// A swap that changes NOTHING the wire can see: same model, same controls, same
// chain length — only a different object at that spot. The neighbour on each
// side must still be told, or one of them keeps talking to a module that is no
// longer there.
TEST(chain_hotplug_swap_identical_module) {
    SimRack r{None /*master*/, MP2B8, MB32};
    r.settle();
    CHECK(r.chain() == std::vector<std::string>({"p2b8", "b32"}));

    r.remove(1);
    r.insert(1, MP2B8);                      // a different P2B8, byte-identical
    r.settle();
    CHECK(r.chain() == std::vector<std::string>({"p2b8", "b32"}));
    // The replacement really is carrying the chain: move a control on the B32
    // and it has to reach the master through it.
    r.mods[2]->block.buttons = 1u << 3;
    r.settle();
    CHECK(r.mods[0]->consumer->block[1].buttons == (1u << 3));
}

TEST(chain_controller_models_skip_g8) {
    UpstreamMessage m; m.count = 3;
    m.block[0].modelId = MG8;
    m.block[1].modelId = MP2B8;
    m.block[2].modelId = MS10;
    auto v = controllerModels(m);
    CHECK(v.size() == 2);
    CHECK(v[0] == "p2b8");                   // controller #1 despite the G8 before it
    CHECK(v[1] == "s10");
}

TEST(chain_validate_chain) {
    CHECK(validateChain({}, {}) == "");
    CHECK(validateChain({}, {"p2b8"}) == "");                    // surplus physical: idle, legal
    CHECK(validateChain({"p2b8"}, {"p2b8"}) == "");
    CHECK(validateChain({"p2b8"}, {"p2b8", "b32"}) == "");       // prefix rule
    CHECK(validateChain({"p2b8"}, {}) != "");                    // declared but missing
    CHECK(validateChain({"p2b8", "b32"}, {"p2b8"}) != "");
    CHECK(validateChain({"b32"}, {"p2b8"}) != "");               // wrong model at position
    // message mentions position and both models — pin the format:
    CHECK(validateChain({"b32"}, {"p2b8"}) ==
          "controller 1: patch declares b32, chain has p2b8");
    CHECK(validateChain({"p2b8", "b32"}, {"p2b8"}) ==
          "controller 2: patch declares b32, chain has nothing");
}

// Guards the register mapping the master's chain feed/readback relies on: it
// walks the physical chain with a 1-based G8 counter and resolves each jack via
// g8Register(), which is where the MASTER18's one-device offset lives (#24).
// Feeding {'G', g8, j} directly would put the MASTER18's first G8 on top of the
// master's own gate outputs.
TEST(chain_g8_register_mapping) {
    using namespace droid;
    for (uint8_t g = 1; g <= 4; g++)
        for (uint8_t j = 1; j <= 8; j++) {
            RegId r16 = g8Register(g, j, MasterType::Master16);
            CHECK(r16.type == 'G' && r16.ctrl == g && r16.num == j);
            RegId r18 = g8Register(g, j, MasterType::Master18);
            CHECK(r18.type == 'G' && r18.ctrl == g + 1 && r18.num == j);
        }
    // The MASTER18's four native gate outputs are device 1 and belong to no G8.
    for (uint8_t j = 1; j <= 4; j++) {
        RegId native = canonicalize(RegId{'G', 0, j}, MasterType::Master18);
        CHECK(native.ctrl == 1 && native.num == j);
        for (uint8_t g = 1; g <= 4; g++)
            CHECK(pack(g8Register(g, j, MasterType::Master18)) != pack(native));
    }
    // On the MASTER, by contrast, bare G1..G8 IS the first G8.
    CHECK(pack(canonicalize(RegId{'G', 0, 5}, MasterType::Master16)) ==
          pack(g8Register(1, 5, MasterType::Master16)));
}

TEST(chain_detent_delta) {
    using namespace droid::chain;
    CHECK(detentDelta(5, 0) == 5);
    CHECK(detentDelta(0, 5) == -5);
    CHECK(detentDelta(0x00000002u, 0xFFFFFFFEu) == 4);    // wrap forward
    CHECK(detentDelta(0xFFFFFFFEu, 0x00000002u) == -4);   // wrap backward
    CHECK(detentDelta(7, 7) == 0);
}

TEST(chain_extended_blocks_are_pod) {
    using namespace droid::chain;
    // Protocol contract: blocks cross void* expander buffers by assignment.
    static_assert(std::is_trivially_copyable<UpstreamBlock>::value, "upstream POD");
    static_assert(std::is_trivially_copyable<DownstreamBlock>::value, "downstream POD");
    UpstreamBlock u;
    CHECK(u.detentCount[0] == 0u);
    CHECK(u.faderTouch == 0);
    DownstreamBlock d;
    CHECK(d.dispIsText == 0);
    CHECK(d.dispHeader[0] == '\0');
}

TEST(chain_fader_led_fields_pod) {
    using namespace droid::chain;
    static_assert(std::is_trivially_copyable<DownstreamBlock>::value, "downstream POD");
    DownstreamBlock d;
    CHECK(d.faderLed[0] == 0.f);
    CHECK(d.faderLedColor[0] == 0.f);
}

// M5: MidiFrame rides both blocks. Zero-init leaves no residual events, and the
// blocks must stay trivially copyable so they still cross the void* buffers.
TEST(chain_midiframe_pod_and_zero_init) {
    using namespace droid::chain;
    static_assert(std::is_trivially_copyable<MidiFrame>::value, "MidiFrame POD");
    static_assert(std::is_trivially_copyable<UpstreamBlock>::value, "upstream POD");
    static_assert(std::is_trivially_copyable<DownstreamBlock>::value, "downstream POD");
    UpstreamBlock u;
    CHECK(u.midi.seq == 0u);
    for (int p = 0; p < droid::chain::kChainMidiPorts; p++) {
        CHECK(u.midi.count[p] == 0);
        CHECK(u.midi.dropped[p] == 0);
        CHECK(u.midi.ev[p][0].status == 0);
    }
    DownstreamBlock d;
    CHECK(d.midi.seq == 0u);
    for (int p = 0; p < droid::chain::kChainMidiPorts; p++) {
        CHECK(d.midi.count[p] == 0);
        CHECK(d.midi.dropped[p] == 0);
        CHECK(d.midi.ev[p][kMidiEventsPerFrame - 1].status == 0);
    }
}

// X7 never consumes a controller number, so numbering skips it just like a G8.
TEST(chain_x7_not_controller_and_skipped) {
    using namespace droid::chain;
    CHECK(isControllerModel(MX7) == false);
    CHECK(std::string(modelName(MX7)) == "x7");
    UpstreamMessage m; m.count = 3;
    m.block[0].modelId = MX7;
    m.block[1].modelId = MP2B8;
    m.block[2].modelId = MS10;
    auto v = controllerModels(m);
    CHECK(v.size() == 2);
    CHECK(v[0] == "p2b8");                   // controller #1 despite the X7 before it
    CHECK(v[1] == "s10");
}

// ISSUE-2: upstream MIDI sliding-window contract. The writer keeps a
// persistent window + monotonic totals; the reader diffs the totals and
// consumes only the unseen tail — no event is lost to the tick divider or the
// double-buffer flips, and none is processed twice.
TEST(chain_midi_upstream_window) {
    using namespace droid::chain;
    static_assert(std::is_trivially_copyable<MidiUpstreamWindow>::value, "window POD");
    auto ev = [](uint8_t d1) { droid::MidiEvent e; e.status = 0x90; e.data1 = d1; return e; };

    MidiUpstreamWindow w;
    MidiFrame f;
    uint32_t last = 0;
    int first, n;

    // Nothing captured yet: reader consumes nothing.
    w.publish(f);
    CHECK(consumeUpstreamMidi(f, 0, last, first, n) == 0);
    CHECK(n == 0);

    // One event; reader sees exactly it.
    w.append(0, ev(60));
    w.publish(f);
    CHECK(f.count[0] == 1 && f.total[0] == 1u);
    CHECK(consumeUpstreamMidi(f, 0, last, first, n) == 0);
    CHECK(n == 1 && first == 0 && f.ev[0][0].data1 == 60);
    last = f.total[0];

    // The ISSUE-2 shape: events land on non-sampled frames, later empty
    // frames republish — the window persists, so a late sample still sees them.
    w.append(0, ev(61));
    w.append(0, ev(62));
    w.publish(f);              // "event frame"
    w.publish(f);              // subsequent "empty frames" overwrite the block
    w.publish(f);
    CHECK(consumeUpstreamMidi(f, 0, last, first, n) == 0);
    CHECK(n == 2);
    CHECK(f.ev[0][first].data1 == 61 && f.ev[0][first + 1].data1 == 62);
    last = f.total[0];

    // Re-sample with nothing new: no replay.
    w.publish(f);
    CHECK(consumeUpstreamMidi(f, 0, last, first, n) == 0);
    CHECK(n == 0);

    // Ports are independent.
    w.append(1, ev(70));
    w.publish(f);
    CHECK(consumeUpstreamMidi(f, 0, last, first, n) == 0 && n == 0);
    CHECK(consumeUpstreamMidi(f, 1, 0, first, n) == 0);
    CHECK(n == 1 && f.ev[1][first].data1 == 70);
}

TEST(chain_midi_upstream_window_overflow) {
    using namespace droid::chain;
    auto ev = [](uint8_t d1) { droid::MidiEvent e; e.status = 0x90; e.data1 = d1; return e; };

    // More than kMidiEventsPerFrame events between samples: the window slides,
    // the reader consumes the newest kMidiEventsPerFrame in order and reports
    // the displaced remainder as lost.
    MidiUpstreamWindow w;
    for (int i = 0; i < kMidiEventsPerFrame + 3; i++) w.append(0, ev(uint8_t(i)));
    MidiFrame f;
    w.publish(f);
    CHECK(f.count[0] == kMidiEventsPerFrame);
    CHECK(f.total[0] == uint32_t(kMidiEventsPerFrame + 3));
    int first, n;
    CHECK(consumeUpstreamMidi(f, 0, 0, first, n) == 3);   // 3 oldest displaced
    CHECK(n == kMidiEventsPerFrame && first == 0);
    CHECK(f.ev[0][0].data1 == 3);                          // oldest survivor
    CHECK(f.ev[0][kMidiEventsPerFrame - 1].data1 == kMidiEventsPerFrame + 2);
}

TEST(chain_midi_upstream_total_wrap_and_resync) {
    using namespace droid::chain;
    // Wrap-safe totals: reader diff crosses the uint32 boundary correctly.
    MidiFrame f;
    f.total[0] = 2;            // writer wrapped past 0xFFFFFFFF
    f.count[0] = 4;
    int first, n;
    CHECK(consumeUpstreamMidi(f, 0, 0xFFFFFFFEu, first, n) == 0);
    CHECK(n == 4 && first == 0);
    // Hot-plug resync: baseline set to the CURRENT total consumes nothing;
    // a stale-ahead baseline (fresh X7, totals restarted) consumes nothing
    // rather than replaying.
    CHECK(consumeUpstreamMidi(f, 0, f.total[0], first, n) == 0);
    CHECK(n == 0);
    CHECK(consumeUpstreamMidi(f, 0, f.total[0] + 100, first, n) == 0);
    CHECK(n == 0);
}

// X7 must be first in the chain and unique.
TEST(chain_x7_position_validation) {
    using namespace droid::chain;
    UpstreamMessage ok; ok.count = 2;        // MX7 at index 0 (nearest master) — valid
    ok.block[0].modelId = MX7;
    ok.block[1].modelId = MP2B8;
    CHECK(x7ChainError(ok) == "");

    UpstreamMessage none; none.count = 2;    // no X7 at all — valid
    none.block[0].modelId = MP2B8;
    none.block[1].modelId = MS10;
    CHECK(x7ChainError(none) == "");

    UpstreamMessage late; late.count = 2;    // MX7 not first — invalid
    late.block[0].modelId = MP2B8;
    late.block[1].modelId = MX7;
    CHECK(x7ChainError(late) == "x7 must be first in the chain");

    UpstreamMessage dup; dup.count = 2;      // two MX7s — invalid
    dup.block[0].modelId = MX7;
    dup.block[1].modelId = MX7;
    CHECK(x7ChainError(dup) == "only one x7 can be attached");
}

// ISSUE-1 seam: DroidMaster::loadPatchFile swaps in a FRESH Engine on every
// (re)load. A fresh engine's s.midi.x7 defaults false, and the engine's own
// keepX7 preserve (engine.cpp) only covers an in-place load() on the SAME
// engine — it cannot carry presence across the swap. So the master must
// re-assert its retained, chain-derived x7Present into the new engine; else
// midiin/out/through gate off (midiAvailable() == false) until the chain next
// changes, even with an X7 attached. Guards that re-assertion contract.
TEST(chain_x7_reasserted_across_engine_swap) {
    using namespace droid;
    bool x7Present = true;   // the master's retained physical-chain state

    // Mirror loadPatchFile: build a fresh engine, load a MIDI patch, then apply
    // the ISSUE-1 re-assert. Without setX7Present the patch would run silently.
    auto swapIn = [&](bool present) {
        auto e = std::make_unique<Engine>(MasterType::Master16, 6000.f);
        e->load("[midiout]\n    usb = 1\n    gate1 = 1\n    pitch1 = 0\n");
        CHECK(e->patchUsesMidi() == true);
        CHECK(e->x7Present() == false);       // presence lost across the swap
        CHECK(e->midiAvailable() == false);   // MIDI would gate off here
        e->setX7Present(present);             // the fix
        return e;
    };

    auto engine = swapIn(x7Present);
    CHECK(engine->x7Present() == true);
    CHECK(engine->midiAvailable() == true);   // MIDI reachable after re-assert

    // Contrast: no re-assert (fix absent) leaves MIDI unreachable despite the X7.
    auto broken = std::make_unique<Engine>(MasterType::Master16, 6000.f);
    broken->load("[midiout]\n    usb = 1\n    gate1 = 1\n    pitch1 = 0\n");
    CHECK(broken->patchUsesMidi() == true);
    CHECK(broken->midiAvailable() == false);
}

// ISSUE-3: patchUsesMidi() flags only MIDI patches, and midiAvailable() gates on
// reachable MIDI hardware — the two conditions the master ANDs to warn.
TEST(chain_patch_uses_midi_detection) {
    using namespace droid;
    Engine e(MasterType::Master16, 6000.f);
    e.load("[lfo]\n    hz = 1\n    square = O1\n");
    CHECK(e.patchUsesMidi() == false);
    e.load("[midithrough]\n    fromusb = 1\n    tousb = 1\n");
    CHECK(e.patchUsesMidi() == true);
    // MASTER18 has built-in MIDI: available even with no X7 (no warning there).
    Engine m18(MasterType::Master18, 6000.f);
    m18.load("[midiout]\n    usb = 1\n    gate1 = 1\n    pitch1 = 0\n");
    CHECK(m18.patchUsesMidi() == true);
    CHECK(m18.midiAvailable() == true);
    // Experimental MIDI circuits count too — midihirescc emits nothing without
    // a port, which is exactly what the warning exists to tell the user. Found
    // by UAT (2026-08-02): the detection is a hardcoded name list, so a new
    // MIDI circuit is silently omitted until it is added here.
    LoadOptions exp;
    exp.allowExperimental = true;
    Engine hires(MasterType::Master16, 6000.f);
    hires.load("[midihirescc]\n    usb = 1\n    hirescc22 = 0.5\n", exp);
    CHECK(hires.patchUsesMidi() == true);
    CHECK(hires.midiAvailable() == false);      // MASTER16, no X7 -> warn
}

// ISSUE-5: chainOk demotion is debounced against Rack's transient expander
// re-enumeration on hot-plug. A brief invalid window (< kMaxInvalidFrames) must
// never demote a previously-valid chainOk; sustained invalidity must; and a
// forced (patch-load) revalidation must demote immediately.
TEST(chain_ok_debounce) {
    using namespace droid::chain;

    // Transient shrink: invalid for a few frames, then valid — never demotes.
    {
        ChainOkDebounce d;
        bool ok = true;
        for (int i = 0; i < ChainOkDebounce::kMaxInvalidFrames - 1; i++) {
            auto r = d.update(ok, /*valid=*/false, /*force=*/false);
            ok = r.ok;
            CHECK(ok == true);        // held through the transient
            CHECK(r.pending == true); // caller keeps polling
        }
        auto r = d.update(ok, /*valid=*/true, false);   // chain self-heals
        ok = r.ok;
        CHECK(ok == true);
        CHECK(r.pending == false);
        CHECK(d.invalidFrames == 0);  // counter reset for the next episode
    }

    // Sustained invalidity: demotes exactly at the tolerance bound.
    {
        ChainOkDebounce d;
        bool ok = true;
        for (int i = 1; i < ChainOkDebounce::kMaxInvalidFrames; i++) {
            ok = d.update(ok, false, false).ok;
            CHECK(ok == true);        // still within window
        }
        auto r = d.update(ok, false, false);   // the kMaxInvalidFrames-th check
        CHECK(r.ok == false);         // now demoted
        CHECK(r.pending == false);
    }

    // Already-invalid (prevOk false): demote/stay-false at once, no polling.
    {
        ChainOkDebounce d;
        auto r = d.update(/*prevOk=*/false, /*valid=*/false, /*force=*/false);
        CHECK(r.ok == false);
        CHECK(r.pending == false);
    }

    // Forced revalidation (fresh patch load) with a wrong chain: immediate error,
    // no debounce — a bad chain at load must not wait out the hot-plug window.
    {
        ChainOkDebounce d;
        auto r = d.update(/*prevOk=*/true, /*valid=*/false, /*force=*/true);
        CHECK(r.ok == false);
        CHECK(r.pending == false);
    }
}
