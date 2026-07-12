# vcvoid — Architecture

Goal: VCV Rack 2 modules that emulate the DROID hardware system — the master,
its controllers, and the X7 — such that a real `droid.ini` patch loaded into
the virtual master behaves exactly as it does on hardware (modulo physical,
hardware-only behavior like motor-fader haptics).

This document describes the code architecture. The process for building it
circuit-by-circuit is in [docs/implementation-system.md](docs/implementation-system.md).
For task-oriented "where do I touch to change X" walkthroughs (fix a circuit,
add a circuit, move a panel control, trace a knob turn or a MIDI event to the
engine), see [docs/code-tour.md](docs/code-tour.md).

## Design principles

1. **The engine is pure.** All DROID behavior lives in a Rack-independent,
   Qt-independent C++17 library (`engine/`). Its only I/O boundary is a
   register table. Everything else — Rack, panels, MIDI — is an adapter.
2. **Deterministic verification first.** Every circuit is verified headlessly
   by golden tests (patch + input timeline → expected output timeline) run by
   a standalone test binary. No test requires launching Rack, except for the
   thin Rack adapter layer itself.
3. **Official resources are the interface spec.** The Forge's
   `droidfirmware.json` (vendored via `tools/droidcheck`) defines the exact
   jack tables, defaults, short aliases, RAM costs, and memory limits. We
   code-generate from it rather than transcribing. The structured manual
   (`manual/`) defines semantics. `droidcheck` (the Forge's own
   parser/validator, headless) is the parity oracle for patch acceptance.
4. **Hardware limits are emulated, not ignored.** RAM budget, patch size
   limit, jack clamping, and the control-rate loop are all faithfully
   reproduced.

## Repository layout

```
vcvoid/
├── engine/                 # Pure C++17 DROID engine — zero Rack/Qt deps
│   ├── src/                #   parser, loader, registers, cables, input math, kernel
│   │                       #   loop, shared circuit cores (seqcore/encodercore/…)
│   ├── circuits/           #   one .cpp per circuit (subclasses Circuit)
│   └── gen/                #   GENERATED jack + scale tables (do not edit)
├── plugin/                 # VCV Rack plugin (Rack SDK Makefile workflow)
│   ├── src/                #   MasterBase.hpp (shared master host), DroidMaster,
│   │                       #   Master18, controller expanders, G8, X7, Layout.hpp
│   └── res/faceplates/     #   panel art (PNG faceplates from the Forge)
├── tests/
│   ├── golden/             #   per-circuit golden test cases (.gold, one dir/circuit)
│   ├── runner/             #   headless golden runner (links engine directly)
│   ├── unit/               #   engine unit tests (parser, loader, chain, midi, …)
│   └── layoutcheck/        #   Layout.hpp ↔ Forge parity + art-alignment harness
├── tools/
│   ├── droidcheck/         #   (existing) Forge-parity patch validator
│   ├── jackgen/            #   codegen: droidfirmware.json → engine/gen/ jack tables
│   ├── scalegen/           #   codegen: scale tables → engine/gen/
│   └── ledger/             #   genledger.py: manual frontmatter → circuits-status.yaml
├── manual/                 # (existing) structured DROID manual, blue-7
├── patches/                # (existing) curated droid.ini patches (UAT/test fixtures)
├── circuits-status.yaml    # implementation ledger (see implementation-system.md)
├── architecture.md         # this file
└── docs/                   # code-tour.md, implementation-system.md, UAT/deferral logs
```

## The engine (`engine/`)

### Values and registers

- All values are 32-bit floats in DROID number convention: **1.0 = 10 V**.
  Internal math is unbounded; clamping to ±10 V (±1.0) happens only where the
  hardware clamps — at physical output-jack registers.
- A single **register table** holds every addressable register: `I1…I8`,
  `O1…O8`, `N1…N8`, `G` gates (master18/G8), per-controller `P`/`B`/`S`/`L`,
  the special registers (`X1`, `R`, …), and all internal cables (`_NAME`).
- The engine's entire external interface is: *write input registers →
  `tick()` → read output registers*. This is what makes headless golden
  testing trivial and the Rack adapter thin.

### The kernel loop (tick model)

Mirrors the hardware master's loop (manual, hardware.md §11.2 — ~180 µs/cycle
≈ 5.5 kHz). Each `tick()`:

1. Input registers hold the values the adapter wrote (hardware: jacks/pots/
   buttons sampled into `I`, `G`, `B`, `P`).
2. Circuits execute **strictly in patch-file order**. Register and cable
   writes are **immediately visible** to circuits later in the file within
   the same tick; circuits earlier in the file see them on the *next* tick
   (one-tick delay). This ordering subtlety is observable on hardware and is
   locked in by a dedicated golden test (the manual's `bernoulli`/`contour`
   example, both orderings).
3. Output registers are published (hardware: DAC conversion, LED update).

**Tick rate** is a parameter of the engine, not a constant:

- Golden tests declare the tick rate per test — results are independent of
  any host sample rate.
- In Rack, the tick rate is a **master context-menu setting** with four
  discrete choices — 2 / 4 / 6 / 8 kHz, default 6 kHz (`kTickRates` in
  `plugin/src/MasterBase.hpp`) — implemented as an integer divider of Rack's
  sample rate (`divider = round(sampleRate / targetHz)`; e.g. 48 000 / 8) so
  timing stays exactly deterministic — no fractional resampling. The engine
  takes its tick rate only in its constructor, so a tick-rate or sample-rate
  change rebuilds the engine at the new *effective* rate (`applyTiming`).
  Lower rates let users match a heavily-loaded hardware master, whose real
  cycle time stretches (180 → ~500 µs).
- *Deferred (opt-in, later):* "authentic timing" mode that derives cycle time
  from patch size/circuit count to simulate load-dependent slowdown
  automatically.

### Input math and circuit authoring surface

- Every circuit input implements the manual's three-column evaluation
  **`A × B + C`** (value, factor, offset), each operand bound to a register,
  cable, or constant. A shared `Input` class owns this; `Output` owns the
  single-column write.
- Circuit authors subclass `Circuit`, declare nothing by hand (jack tables
  are generated — below), and implement `tick()` reading typed inputs /
  writing outputs. Jacks are addressed by name at run time —
  `in("input")`, `out("output", index)` (`engine/src/circuit.hpp`) — never by
  raw slot. Stateful circuits also implement `save()`/`load()` for state
  persistence (the SD-card role; Rack stores it in patch JSON) and respect
  `dontsave`.
- **Jack lookup is memoized per instance.** `in()`/`out()` are called with the
  same short literal names every tick, and resolving each through
  `gen::findJack` (linear prefix search + a `std::string` temp) dominated the
  audio thread in profiles. `Circuit::memoSlot` (`engine/src/circuit.cpp`)
  caches name→slot resolutions keyed by name *content* (a few circuits —
  `fadermatrix`, `matrixmixer` — `snprintf` names into a reused stack buffer,
  so pointer identity is unsafe as a key) and scans from a rotating cursor so
  the steady-state cost is one probe per call. This is a pure hot-path
  optimization: behavior is identical to resolving fresh each time.
- **Randomness is seeded.** The engine takes an RNG seed; all random circuits
  draw from engine-owned RNG. Golden tests pin the seed, so even "random"
  circuits are deterministically testable. (Streams won't match hardware —
  inherently impossible — but distributions and behavior are verified.)

### Generated jack tables (`engine/gen/`, `tools/jackgen`)

`tools/jackgen` reads the Forge's `droidfirmware.json`
(`tools/droidcheck/vendor/droidforge/droidforge/droidfirmware.json`) and
emits, per circuit: input/output declarations with exact names, **short
aliases** (`tt` ≡ `taptempo`), types, defaults, and RAM cost data. This
guarantees the engine accepts exactly the parameter surface the Forge
accepts, and is regenerable when the Forge updates. The manual's per-circuit
docs (`manual/circuits/*.md`) supply the *semantics* the hand-written
`tick()` must implement.

### Patch parsing

- The engine ships its own small `droid.ini` parser (syntax fully specified
  in `manual/basics.md`: `[circuit]` headers, `param = value`, comments,
  short aliases, voltage suffixes like `2V`, cables, controller declarations).
  The Forge's parser is Qt-entangled, so we don't link it at runtime.
- **Parity is verified, not assumed:** CI runs `droidcheck` against every
  test patch. Any patch the Forge flags as a problem, our loader must reject
  with an equivalent error; anything the Forge accepts, we must load. One
  deliberate exception: **deprecated circuits** — the Forge counts them as
  problems, but real hardware runs them, so we load them with a warning
  (matching hardware, not the Forge). This deprecated-circuit exception is
  currently the *only* deliberate loader divergence. (An earlier lenient
  acceptance of unary-negated register atoms — e.g. `input = -P1.1` — was
  **aligned to the Forge in the M4 parity pass**: such a token matches none of
  the Forge's A*B+C regex forms, so it is rejected as an "Invalid (garbled)
  value" in every position; a leading minus survives only on a numeric literal
  like `-2`. Verified against droidcheck.)

### Hardware limits (faithfully enforced at patch load)

| Limit | Value | Source |
|-------|-------|--------|
| RAM budget | 112 867 bytes (MASTER), 109 015 (MASTER18) | `droidfirmware.json` `available_memory` |
| RAM accounting | base jacktable 168 B + per-circuit `ramsize` + per-jack `ramhint` costs | `droidfirmware.json` (same math as the Forge) |
| Patch file size | 64 000 bytes | manual |
| Jack voltage | outputs clamp to ±10 V | manual |
| Registers | only those present on the configured master/controllers | Forge validator |

Over-budget or invalid patches **refuse to load** with a Forge-style error
(the hardware blinks LEDs; we show the message + line number).

## The Rack plugin (`plugin/`)

Built with the Rack SDK + Makefile workflow (`RACK_DIR`, `make`, `make
dist`, `plugin.json`) — see the `vcv-rack-plugin-dev` skill. The plugin
Makefile also compiles `engine/` sources; the headless test runner builds the
same sources standalone, so there is exactly one engine. The plugin ships the
two masters, all nine controllers, the `G8` and `X7` expanders, and a
decorative `Bling` LED module (`plugin/src/plugin.cpp` `init()`).

All engine-hosting logic — patch loading, the controller-chain + X7/MIDI feed,
`process()`, hot-reload, chain revalidation, and the context menu — lives once
in `DroidMasterBase` / `DroidMasterBaseWidget` (`plugin/src/MasterBase.hpp`).
`DroidMaster` and `DroidMaster18` are thin subclasses that pin the master type
and I/O geometry and add their own panel art, ports, and LEDs; MASTER18's gate
I/O rides the `processExtraIO()` hook.

### DroidMaster / Master18 modules

- MASTER: 8 CV inputs, 8 CV outputs (16-bit, ±10 V), 4×4 LED matrix rendered
  on the panel. MASTER18: 2 gate/trigger inputs (`I1`/`I2`), 8 CV outputs,
  4 gate/trigger outputs (`G1`–`G4`, driven-only → 5 V), `MasterType::Master18`
  register validation and RAM budget. Both are implemented.
- **Patch loading:** context menu → load a DROID patch file. Any filename is
  accepted (the hardware requires `droid.ini`, but the Forge saves patches
  under arbitrary `.ini` names — we load those directly, as saved). The
  patch is consumed **verbatim as Forge-format text** — never reformatted or
  converted. The module watches the file's mtime and hot-reloads on change,
  so editing in the Forge or a text editor updates Rack live. Load errors
  display Forge-style (message + line) and the engine stops, like hardware.
- **Tick rate** context-menu setting (see kernel loop above).
- Per-audio-block adapter: write jack voltages → tick the engine at the
  configured divider → read output registers → set output voltages. Output
  values hold between ticks (matching the hardware's control-rate DAC).
- **Rack patch JSON stores no DROID patch data** — only the loaded file's
  *path*, module settings (tick rate), and the runtime state of stateful
  circuits (the micro-SD card's persistence role: sequencer state, presets,
  …). The DROID patch itself always lives in its own `.ini` file on disk.

### Controller expanders

- One VCV module per controller — `P2B8`, `P4B2`, `P10`, `S10`, `P8S8`,
  `B32`, `E4`, `M4`, `DB8E` — plus the `G8` gate expander. Placed to the
  **right** of the master and chained via Rack's expander messaging;
  **chain position = controller number**, exactly like the hardware ribbon
  chain (`P1.2` = pot 2 on the first controller).
- The chain protocol is a Rack-agnostic contract in `engine/src/chain.hpp`:
  fixed-size POD `UpstreamBlock`/`DownstreamBlock` structs memcpy'd through
  Rack's `void*` expander buffers. **Controls flow toward the master
  (upstream) by prepend** — each expander inserts its own block at `[0]` and
  relays the rest, so block order equals chain order with no position
  handshake — and **LED/display state flows away from the master (downstream)
  by shift** — each expander consumes `block[0]` and relays the remainder.
  Every expander is a `ChainModule` (`plugin/src/ChainModule.hpp`) that relays
  **one hop per direction per audio frame** (`relay()`); the master's
  `process()` samples the assembled upstream message on its own tick frames.
  The one-frame-per-hop latency is well inside the tick divider (≥6 frames).
- The patch's controller declarations (`[p2b8]`, …) are validated against the
  physical chain by `chain::validateChain` (prefix match; surplus physical
  modules idle). A mismatch sets `chainOk = false` and pauses the engine until
  it clears. G8 exposes real gate jacks (input level in, driven register → 5 V
  out). See the code tour for the full knob-to-engine trace.

### X7 module

- Bridges Rack's MIDI ports to the engine's MIDI circuits (`midiin`,
  `midiout`, `midithrough`, `midifileplayer`). Sits **first** in the expander
  chain like hardware (`block[0]`) and is the only block that carries MIDI.
- The two directions use **different** contracts because their pacing differs
  (`chain.hpp` `MidiFrame`; `plugin/src/X7.cpp`). **Downstream** (master → X7,
  master-paced) is a per-tick snapshot deduped by a monotonic `seq`.
  **Upstream** (X7 → master) cannot use a snapshot: the X7 relays every audio
  frame but the master samples only every `divider` frames, so an event
  written on a non-sampled frame would be overwritten by later empty flips
  before the master reads it. Instead the X7 publishes a **persistent sliding
  window** of the most recent `kMidiEventsPerFrame` events per port plus a
  wrap-safe monotonic `total`; the master diffs the total each tick frame and
  consumes exactly the unseen window tail. This is the ISSUE-2 fix (the master
  must never miss upstream MIDI even when several events land in one tick
  frame); the same detent-count pattern carries encoder turns and fader
  touches upstream.

## Verification harness (`tests/`)

`make test` runs four gates, all headless, exit code = failures (see the root
`Makefile`): **unittests** + **goldens** + **layoutcheck** + **artcheck**.

- **Golden test case** = one `.gold` file (line-oriented, zero-dependency
  format — see `tests/runner/goldfile.hpp`): inline patch text (`patch <<< …
  >>>`), tick rate, RNG seed, input timeline (`@N set I1 x`), expected outputs
  at ticks with per-value tolerance (`@N expect O1 y tol 1e-4`); also
  `expect_load_error` files for patches that must be rejected. One directory
  per circuit under `tests/golden/`. The `goldens` target builds
  `tests/runner/` (a standalone binary linking the engine directly) and runs
  every `.gold`.
- **Unit tests** (`tests/unit/`, `make unittests`) cover the engine internals
  the goldens don't exercise directly — parser, loader, registers, RAM, chain
  protocol (window persistence, overflow, hot-plug resync), MIDI, note math,
  and the golden file format itself.
- **layoutcheck** parity-checks `plugin/src/Layout.hpp` against the Forge's own
  module layout source; **artcheck** validates those positions against the
  faceplate art via OpenCV Hough detection. Both **skip gracefully** if their
  inputs are absent (`tools/droidcheck/vendor/` checkout, `opencv-python`).
- **`make crosscheck`** validates the golden corpus against
  `droidcheck` (the Forge's own validator) — our loader and the Forge must
  agree on what loads. Not part of `make test` (needs the vendored Forge
  build); run it in the validation-parity pass.
- Expected values are derived at authoring time from the manual's math and
  worked examples (waveform equations, timing tables, truth tables).
- Circuits whose manual frontmatter says `spec_gap: true` get goldens for the
  well-specified parts plus a `# SPEC-GAP:` annotation recording exactly what
  hardware reference data would close the gap.
- Rack-adapter behavior (expander chain, MIDI bridging, panel) is the only
  layer needing `rack-automated` or human verification, and is kept thin for
  that reason. The chain/MIDI *contract* structs are POD and Rack-free, so the
  ISSUE-1/2 seam bugs are now covered by `tests/unit/test_chain.cpp` even
  though the Rack glue around them is not.

## Note on circuit implementation

Not emulated (inherent hardware scope): `firefacecontrol`, `outputcalibrator`,
`sinfonionlink`, motor-fader haptics, VCO calibration hardware specifics —
per-circuit calls are recorded in the ledger as `not-feasible` (from the
manual frontmatter).

All other circuits (as of blue-7 firmware) are implemented — see
[`circuits-status.yaml`](circuits-status.yaml) for per-circuit status,
conventions, and spec-gap notes.
