# Code tour — where to touch to change X

Task-oriented walkthroughs for a developer new to the codebase. Read
[architecture.md](../architecture.md) first for the *why*; this file is the
*where*. Anchors are `file` + symbol names (line numbers drift, so grep the
symbol). Two rules hold everywhere:

- **The engine is pure.** Nothing under `engine/` includes a Rack or Qt header.
  All Rack glue is under `plugin/`; the same `engine/` sources compile into both
  the plugin and the headless test binaries (one engine, two builds).
- **Verify headlessly.** `make test` (unit tests + goldens + layout/art checks)
  is the gate and needs no Rack. Only the thin Rack adapter needs Rack.

---

## (a) Fix a bug in a circuit (e.g. `lfo`)

**Where it lives.** One `.cpp` per circuit in `engine/circuits/` —
`engine/circuits/lfo.cpp`. It subclasses `Circuit` and implements
`tick(EngineState&)`, reading typed inputs and writing outputs by *name*:

```cpp
out("square").set(s, level);              // write an output jack
float hz = in("hz").value(s);             // read an input (applies A*B+C)
```

Jack names, defaults, aliases, and RAM come from the generated tables
(`engine/gen/`); the circuit only supplies semantics from
`manual/circuits/lfo.md`. `in()`/`out()` resolve names through a per-instance
memoized cache (`Circuit::memoSlot`, `engine/src/circuit.cpp`) — never index
`inputs[]`/`outputs[]` directly.

**How to test.** Golden tests are the reproduction and the regression guard:
`tests/golden/lfo/*.gold` (one directory per circuit). Each `.gold` is a
line-oriented file — inline patch, tick rate, RNG seed, an input timeline, and
expected outputs with tolerance:

```
patch <<<
[lfo]
    hz = 5
    square = O1
>>>
tickrate 6000
@0 expect O1 1.0 tol 1e-4
```

Format reference: `tests/runner/goldfile.hpp`. Reproduce the bug by writing a
failing `.gold` derived from the manual's math, fix `lfo.cpp`, then:

```sh
make goldens      # builds tests/runner/ and runs every .gold
make test         # full suite — confirm no regressions
```

If the fix changes what the manual guarantees vs. what only hardware pins down,
record the gap in `circuits-status.yaml` (its `notes`) and a `# SPEC-GAP:`
comment in the golden. Never commit red.

---

## (b) Add a brand-new circuit

This is automated end-to-end — see [implementation-system.md](implementation-system.md)
and the `implement-circuit` skill. The manual steps, if doing it by hand:

1. **Jack table.** Confirm `engine/gen/jacktables.gen.*` has the circuit's
   entry (it is generated from the Forge's `droidfirmware.json`; run
   `make gen` if missing — needs `tools/droidcheck/vendor/`).
2. **Goldens first.** Write `tests/golden/<name>/*.gold` from every worked
   example and I/O edge case in `manual/circuits/<name>.md`.
3. **Implement.** `engine/circuits/<name>.cpp`: subclass `Circuit`, implement
   `tick()`, and **register it** with the factory at file scope:
   ```cpp
   DROID_REGISTER_CIRCUIT(myname, MyCircuit)   // registry.hpp
   ```
   The Makefile globs `engine/circuits/*.cpp`, so no build-file edit is needed.
   `Engine::load` (`engine.cpp`) calls `makeCircuit(name)`; an unimplemented
   circuit name becomes a load error, so registration is what "turns it on".
4. **Stateful circuits** implement `save()`/`load()` (SD-card persistence; Rack
   stores it in patch JSON) and respect `dontsave`. Randomness draws from the
   engine-owned seeded RNG (`engine/src/rng.hpp`) so goldens stay deterministic.
5. **Verify + record.** `make test` green, update `circuits-status.yaml`,
   commit atomically.

---

## (c) Change a panel layout

`plugin/src/Layout.hpp` is the **single source of truth** for control positions,
transcribed 1:1 from the Forge's `modules/module<name>.cpp`. Each module is a
`ModuleLayout` with `num`/`pos`/`size` lambdas keyed by the Forge register-type
char (`I N O G B L P E S R X`); positions are control centres in HP.

- The module widget (e.g. `DroidP2B8Widget` in `plugin/src/P2B8.cpp`) reads
  positions via `droid::layout::find("p2b8")` and `L->pos('P', n)` — it never
  hard-codes coordinates. So a layout change is edited **once** in `Layout.hpp`.
- **Verify without Rack:** `make layoutcheck` parity-checks `Layout.hpp` against
  the Forge's own layout source (edit `tests/layoutcheck/` too if the Forge
  changed); `make artcheck` validates positions against the faceplate PNGs via
  OpenCV Hough-circle/line detection. Both skip gracefully if their inputs
  (`tools/droidcheck/vendor/`, `opencv-python`) are absent.
- Faceplate art is `plugin/res/faceplates/*.png`, loaded by
  `dw::ImagePanel::create` (`plugin/src/DroidWidgets.hpp`).

`Layout.hpp` is deliberately Rack-free (`#include <cstring>` only) so the
headless `layoutcheck`/`artcheck` binaries can include it.

---

## (d) How a knob turn on a controller reaches the engine

The controller chain is a POD message protocol (`engine/src/chain.hpp`) relayed
one hop per audio frame; the master samples it on its tick frames. Trace, using
a P2B8 pot:

1. **Widget → param.** The Rack knob drives a param on the expander module
   (`DroidP2B8`, `plugin/src/P2B8.cpp`).
2. **Fill upstream.** Every frame, `ChainModule::relay()`
   (`plugin/src/ChainModule.hpp`) calls the module's `fillUpstream()`, which
   writes its live control state into an `UpstreamBlock`
   (`b.pots[i] = params[...].getValue()`, buttons into a bitmask, etc.).
3. **Prepend + relay.** `relay()` **prepends** this block at `[0]` ahead of
   whatever came from the right, and writes the left neighbour's producer
   buffer, requesting a Rack message flip. Block order therefore equals chain
   order — no position handshake. One hop per frame; latency ≪ tick divider.
4. **Master samples.** `DroidMasterBase::process()` (`plugin/src/MasterBase.hpp`)
   runs only every `divider` frames. It reads the assembled `UpstreamMessage`
   from its `rightExpander.consumerMessage`, validates the physical chain
   against the patch's declared controllers (`chainOk` gate), then walks the
   blocks in chain order and pushes each control into the engine register table:
   `engine->setRegister({'P', ctrl, k}, up.block[i].pots[k-1])` — `ctrl` is the
   running chain position, so this is exactly `P<ctrl>.<k>`. Buttons → `B`,
   switches → `S`, G8 gate inputs → `G`. Encoders and motor faders use a
   monotonic **detent/total-count** diff (survives the tick divider) via
   `turnEncoder`/`moveFader`.
5. **Tick.** `engine->tick()` runs circuits in patch order; a `pot`/`button`
   circuit reads its `P`/`B` register and does its thing.
6. **Downstream (LEDs).** Same `process()` writes a `DownstreamMessage` (LED
   brightness from driven `L` registers, encoder rings, fader targets, DB8E
   screen text); `relay()` **shifts** it back out — each expander consumes
   `block[0]` in `applyDownstream()` and relays the rest.

The pacing asymmetry (X7-paced fill vs. master-paced sample) is why controls use
monotonic counters rather than per-frame snapshots — see `MasterBase.hpp`
comments (the ISSUE-2 fix: the master must never miss upstream events even when
several land in a single tick frame).

---

## (e) How MIDI gets from Rack to a `midiin` circuit

The X7 (`plugin/src/X7.cpp`) is the only chain block that carries MIDI and must
sit first (`block[0]`). Upstream (into the engine) uses a **sliding-window**
contract, not a snapshot, because the X7 relays every frame but the master
samples only every `divider` frames (the ISSUE-2 sliding-window fix):

1. **Rack MIDI → X7.** `DroidX7::fillUpstream()` drains its `rack::midi::InputQueue`s
   (TRS, USB) for the current frame, converts each 1–3 byte message to a
   `droid::MidiEvent`, and `midiUp.append(port, e)` into a **persistent** window
   of the most recent `kMidiEventsPerFrame` events per port, plus a wrap-safe
   monotonic `total`. `midiUp.publish(b.midi)` copies the window into the block
   **every frame** — the window survives empty frames.
2. **Master consumes the tail.** On its tick frame, `DroidMasterBase::process()`
   calls `consumeUpstreamMidi(mf, port, lastMidiTotalUp[port], first, n)`
   (`chain.cpp`), which diffs the published `total` against the master's
   last-seen baseline and yields exactly the not-yet-seen window tail. Each of
   those events → `engine->sendMidiIn(port, ev)`, which pushes onto the engine's
   inbound `MidiQueue`.
3. **Engine snapshots.** At the top of `engine->tick()` (`engine.cpp`), each
   port's inbound queue is drained into a per-tick buffer
   (`state_.midi.tickEv[port][...]`, count `tickCount`); the previous snapshot
   is discarded.
4. **Circuit reads.** `MidiIn::tick()` (`engine/circuits/midiin.cpp`) first
   gates on `s.midi.x7` (no X7 → writes nothing, patch still loads), then loops
   `s.midi.tickEv[port][0..tickCount)` and converts each event to
   pitch/gate/velocity/CC/clock CV on its output jacks.

Outbound (`midiout`) is the mirror: circuits push to `state_.midi.out`;
`process()` drains it into the downstream `MidiFrame` deduped by a monotonic
`seq`; `DroidX7::applyDownstream()` sends new-`seq` frames out the Rack MIDI
output ports. The X7's own presence is chain-derived and re-asserted into a
freshly loaded engine (`engine->setX7Present`) — ISSUE-1.

---

## Cross-references

| Topic | File(s) |
|-------|---------|
| Engine public interface | `engine/src/engine.hpp` (`Engine::load/tick/setRegister/...`) |
| Circuit base + jack memoization | `engine/src/circuit.hpp`, `circuit.cpp` |
| Circuit factory registration | `engine/src/registry.hpp` (`DROID_REGISTER_CIRCUIT`) |
| Patch parse → validated compile | `engine/src/parser.cpp`, `loader.cpp` |
| Chain protocol (POD contract) | `engine/src/chain.hpp`, `chain.cpp` |
| Master Rack host (all masters) | `plugin/src/MasterBase.hpp` |
| Expander relay base | `plugin/src/ChainModule.hpp` |
| MIDI bridge | `plugin/src/X7.cpp`, `engine/src/midi.hpp` |
| Panel positions | `plugin/src/Layout.hpp` |
| Golden format | `tests/runner/goldfile.hpp` |
| Chain/MIDI seam tests | `tests/unit/test_chain.cpp` |
