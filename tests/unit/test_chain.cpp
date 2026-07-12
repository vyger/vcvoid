#include "harness.hpp"
#include "src/chain.hpp"
#include "src/registers.hpp"
#include "src/engine.hpp"
#include <cmath>
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

// Guards the assumption the master G8 mapping relies on: it always feeds the
// explicit-controller G form ('G', g8, j) with a 1-based g8 counter, so
// canonicalize must leave that form UNCHANGED (identity). Plain G1..G8 (ctrl 0)
// is the only form that gets rewritten to the first G8 (ctrl 1).
TEST(chain_g8_register_canonicalization) {
    using namespace droid;
    // non-identity: plain G5 aliases the first G8's gate 5 on the MASTER
    RegId plain = canonicalize(RegId{'G', 0, 5}, MasterType::Master16);
    CHECK(plain.ctrl == 1 && plain.num == 5);
    // identity: explicit-controller G forms (what the master code feeds) are untouched
    for (uint8_t g = 1; g <= 4; g++)
        for (uint8_t j = 1; j <= 8; j++) {
            RegId r = canonicalize(RegId{'G', g, j}, MasterType::Master16);
            CHECK(r.type == 'G' && r.ctrl == g && r.num == j);
        }
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
