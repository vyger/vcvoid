#include "harness.hpp"
#include "src/midi.hpp"
#include "src/engine.hpp"

TEST(midiqueue_fifo_and_overflow) {
    droid::MidiQueue q;
    droid::MidiEvent e;
    CHECK(!q.pop(e));                       // empty
    for (int i = 0; i < droid::kMidiQueueCap; i++)
        q.push({0x90, (uint8_t)i, 100});
    CHECK(q.dropped == 0);
    q.push({0x90, 200, 100});               // 65th: drops the OLDEST
    CHECK(q.dropped == 1);
    CHECK(q.pop(e) && e.data1 == 1);        // event 0 was dropped
    q.clear();
    CHECK(!q.pop(e));
}

TEST(engine_midi_seams_roundtrip_snapshot) {
    droid::Engine eng;
    eng.load("[lfo]\n    hz = 1\n    square = O1\n");   // any valid patch
    eng.setX7Present(true);
    CHECK(eng.x7Present());
    eng.sendMidiIn(droid::MidiPort::X7Trs, {0x90, 60, 100});
    eng.sendMidiIn(droid::MidiPort::X7Usb, {0xB0, 1, 64});
    eng.tick();
    // tick drained the in-queues into the snapshot (no circuit consumed them;
    // snapshot is discarded next tick — assert via friend-free observable:
    // the in-queues are now empty, so a second tick snapshots zero events).
    droid::MidiEvent e;
    CHECK(!eng.drainMidiOut(droid::MidiPort::X7Trs, e));  // nothing emitted
}

// Direct snapshot assertion via the midiTickCount readback seam: two events
// snapshotted on the first tick, then zero on the next (previous snapshot
// discarded, in-queue already drained).
TEST(engine_midi_tick_snapshot_count) {
    droid::Engine eng;
    eng.load("[lfo]\n    hz = 1\n    square = O1\n");
    eng.sendMidiIn(droid::MidiPort::X7Trs, {0x90, 60, 100});
    eng.sendMidiIn(droid::MidiPort::X7Trs, {0x80, 60, 0});
    CHECK(eng.midiTickCount(droid::MidiPort::X7Trs) == 0);   // no tick yet
    eng.tick();
    CHECK(eng.midiTickCount(droid::MidiPort::X7Trs) == 2);   // both drained
    CHECK(eng.midiTickCount(droid::MidiPort::X7Usb) == 0);
    eng.tick();
    CHECK(eng.midiTickCount(droid::MidiPort::X7Trs) == 0);   // snapshot discarded
}

// Port numbering per manual/circuits/midiin.md "Port selection": the master's
// own ports index first, the X7's after.
TEST(midi_resolve_port) {
    using droid::MidiPort; using droid::PortKind;
    auto rp = droid::resolvePort;
    // MASTER + X7
    CHECK(rp(false, true, PortKind::Usb, 1) == (int)MidiPort::X7Usb);
    CHECK(rp(false, true, PortKind::Trs, 1) == (int)MidiPort::X7Trs);
    CHECK(rp(false, true, PortKind::Trs, 2) == -1);
    // MASTER16 without X7: no ports at all
    CHECK(rp(false, false, PortKind::Usb, 1) == -1);
    // MASTER18 alone
    CHECK(rp(true, false, PortKind::Usb, 1) == (int)MidiPort::M18Usb);
    CHECK(rp(true, false, PortKind::Trs, 1) == (int)MidiPort::M18Trs1);
    CHECK(rp(true, false, PortKind::Trs, 2) == (int)MidiPort::M18Trs2);
    CHECK(rp(true, false, PortKind::Trs, 3) == -1);
    // MASTER18 + X7
    CHECK(rp(true, true, PortKind::Usb, 2) == (int)MidiPort::X7Usb);
    CHECK(rp(true, true, PortKind::Trs, 3) == (int)MidiPort::X7Trs);
    CHECK(rp(true, true, PortKind::Trs, 4) == -1);
    CHECK(rp(true, true, PortKind::Trs, 0) == -1);
    int ports[3];
    CHECK(droid::portsOfKind(true, true, PortKind::Trs, ports) == 3);
    CHECK(ports[0] == (int)MidiPort::M18Trs1 && ports[2] == (int)MidiPort::X7Trs);
}

// x7 presence survives a patch reload (the chain, not the patch, sets it).
TEST(engine_x7_presence_survives_load) {
    droid::Engine eng;
    eng.setX7Present(true);
    eng.load("[lfo]\n    hz = 1\n    square = O1\n");
    CHECK(eng.x7Present());
}
