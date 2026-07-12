# vcvoid

## Project goal

Build a **VCV Rack 2 module** that emulates the DROID hardware: load and run
DROID patch files (`droid.ini`) and reproduce the behavior of the physical
modules — the master, its circuits, and the controllers/expanders — inside VCV
Rack. The `manual/` reference below is the spec for that behavior; each
`circuits/<name>.md` documents the exact inputs, outputs, and semantics a
circuit must implement.

This repo holds the full working plugin plus its reference material: the pure
C++17 DROID engine under `engine/` and the VCV Rack plugin under `plugin/`, the
machine-navigable DROID reference (firmware blue-7) under `manual/` (the
structured manual; the source PDF `droid-manual-blue-7.pdf` is git-ignored),
`patches/` (curated `droid.ini` patches — UAT/test fixtures plus a worked
example), and
`tools/droidcheck/` (a patch validator, below).

Build the Rack plugin with the Rack SDK + Makefile workflow (`RACK_DIR`, `make`,
`make dist`, `plugin.json`, panel SVGs, the `rack::` C++ API) — see the
`vcv-rack-plugin-dev` skill.

## Contribution workflow

This is a public repo with a tracked remote. Every substantive change goes
through a **branch + PR cycle tied to a GitHub issue**: open (or reference) an
issue describing the change, work on a feature branch, and merge via a pull
request that references the issue — no direct commits to `main`. Trivial
changes (typo/doc fixes, comment tweaks, similar quick edits) may skip the
issue and commit directly, but when in doubt use the full cycle.

## What DROID is

DROID is a **CV processing system for Eurorack** built around a *master* module
that runs a user-written *patch*. It can sequence, generate melodies, quantize,
switch, mix, make clocks/triggers/envelopes/LFOs/randoms, and bridge MIDI —
often several of these at once.

- **Masters** (base of every system; "master" lower-case = either):
  - **MASTER** — 8 CV inputs + 8 CV outputs (16-bit, −10 V…+10 V), 4×4 LED matrix.
  - **MASTER18** — 8 CV outputs, 2 gate/trigger inputs + 4 gate/trigger outputs,
    built-in VCO tuner, USB-C, 2× MIDI in + 2× MIDI out (TRS).
- **Expanders**: `X7` (USB/MIDI, always first in the controller chain),
  up to four `G8` (8 gate in/out each; outputs are only 0 V or 5 V).
- **Controllers** (up to 16): `P2B8`, `P4B2`, `P10`, `S10`, `P8S8`, `B32`, `E4`,
  `M4` (motor faders), `DB8E` — pots, buttons, encoders, switches, faders.

### Patches and circuits

A patch is a text file **`droid.ini`** on the master's micro-SD card (also keeps
running state — always leave the card in). Build patches in the **Droid Forge**
GUI (recommended) or any text editor. The Forge blocks loading while a patch has
*problems* (e.g. a parameter with no value). Max patch size is 64 000 bytes;
enable compression for large generated patches.

A patch is a list of **circuits** — DROID's internal "modules" (e.g. `lfo`,
`contour`, `mixer`, `algoquencer`). Text syntax: `[circuitname]` header, then
`param = value` lines. Example:

```droid
[lfo]
    hz = 5
    square = O1
```

### Validating patches (`tools/droidcheck`)

The DROID Forge refuses to load a patch that has *problems*, and it's the
authoritative check. The Forge is a Qt GUI app with no CLI, **but its validator
lives in GUI-free model classes**, so we ship a headless runner that reuses the
Forge's own parser + `Patch::updateProblems()`:

```sh
cd tools/droidcheck
./build.sh                              # first run clones Zarkuun/droidforge + builds
./build/droidcheck ../../patches/*.ini  # exit code = number of patches with problems
```

- **Requires** Qt 6 (`brew install qt`), clang (C++17), git. Build takes ~30 s;
  `build/` and `vendor/` are git-ignored.
- **Firmware:** validates against **blue-7** (the `droidfirmware.json` bundled in
  the Forge source), matching this repo's manual.
- **What it catches** — the same problems the GUI panel shows: unknown circuit,
  **deprecated** circuit, `Duplicate usage of O3 as output`, `Output register O5
  is just used as an input`, registers not present on the chosen master, and
  out-of-memory patches. Note: an output jack assigned twice in one circuit is
  *not* a syntax error — the last line silently wins — so route each `O`/`N` jack
  once and read the register back downstream (`clock = O1`) when a signal is
  needed in two places.
- **How the headless build works** (see `tools/droidcheck/README.md`): it links
  only `parser/` + `patch/` + firmware, and replaces the GUI `Module`/rackview
  stack and `Clipboard` with small stubs (`modulebuilder_stub.cpp`,
  `clipboard_stub.cpp`) — the checker never calls into them. Regenerate/rebuild
  after pulling a newer Forge (delete `vendor/` to re-clone).
- **Caveat:** "deprecated circuit" counts as a problem, so older community
  patches brought in from elsewhere often report problems on blue-7 even though
  they ran fine on the firmware they were written for. (The curated patches in
  `patches/` are kept problem-free.)

### Building & testing the engine

- `make test` — build + run all unit tests and golden files (headless, no Rack).
- `make gen` — regenerate `engine/gen/` from the Forge's droidfirmware.json
  (needs `tools/droidcheck/vendor/`, i.e. run `tools/droidcheck/build.sh` once).
- `make crosscheck` — validate golden patches against droidcheck (Forge parity).

### Plugin identity: vcvoid

The plugin's Rack identity is **vcvoid** (brand, plugin slug, module names —
always lowercase; module slugs are bare hardware names like `master`, `m4`).
The faceplate PNGs in `plugin/res/faceplates/` are one-time derivatives of the
Forge renders with all DROID/DMMDM branding removed (creator's permission;
wordmark in Share Tech Mono, OFL) — there is no import/regen script anymore.
DROID stays as the factual name of the emulated system; the plugin is not
affiliated with or supported by Der Mann mit der Maschine. This repo is
**vcvoid** (`vyger/vcvoid`); the older private dev repo it was rebased from
remains as the archive.

### Panel layout & visual tests

`plugin/src/Layout.hpp` is the single source of truth for control positions (HP
units); `make layoutcheck` parity-checks it against the Forge's own layout
data. `make artcheck` (needs `opencv-python`) validates those positions against
the faceplate art via Hough-circle/line detection. `tools/panelshots.sh`
renders per-module screenshots through Rack for regression diffing —
`tools/check_panelshots.py` compares them against `tests/panel-baseline/`.

### Key concepts

- **Registers / jacks**: `I1`…`I8` inputs, `O1`…`O8` outputs, `N1`…`N8` input
  normalizations (act as outputs feeding an input's default). MASTER18/G8 gates
  are `I1`/`I2`, `G1`…`G4`, `Gn.m`.
- **Controls**: pots `P1.2` (controller 1, pot 2), buttons `B`, button LEDs `L`,
  switches `S`. A pot reads 0.0…1.0; a pressed button = 1.
- **Voltages ↔ numbers**: −10 V…+10 V maps to −1…+1 (2.5 V = `0.25`; write
  either form). Jacks clamp to ±10 V; internal math is unbounded 32-bit float.
- **Input math**: every input has 3 columns — value **A**, factor **B**,
  offset **C** — computed as `A × B + C` (a per-input VCA + offset), e.g.
  `hz = P1.2 * 9 + 1`. Outputs have one column.
- **Internal cables** `_NAME`: wire circuits together — exactly one output,
  at least one input per cable.
- **Overlaying** (one knob, many functions): duplicate a `pot`/`button` circuit
  and gate each with `select`/`selectat`, driven by a `button`/`buttongroup`/
  switch. Unselected circuits' `led`/output jacks go inactive (use
  `buttonoutput` for always-live values).
- **Presets**: stateful circuits (`algoquencer`, `motoquencer`, `pot`, …) expose
  `preset`/`loadpreset`/`savepreset`; state persists to SD.
- **taptempo**: feed a button or clock to set an interval/frequency.
- **Patch generators**: Forge scripts (need Python 3) that emit ready patches,
  e.g. the Motor Fader Performance Sequencer (MFPS).

## Navigating `manual/`

Start at **`manual/README.md`**. Layout:

| Path | Manual ch. | Contents |
|------|-----------|----------|
| `manual/basics.md` | 1–5 | Install, patching fundamentals, the Forge, advanced patching (overlays, presets, tap tempo), patch generators, and the `droid.ini` text syntax + error-code table. |
| `manual/hardware.md` | 6–14 | All controllers, G8/X7 expanders, MASTER18, R2M/R2C bridge, internals, firmware upgrade, calibration/maintenance, specs. |
| `manual/scales.md` | 15 | All 108 scales (0–107) with notes and scale-degree fills. |
| `manual/circuits/` | 16 | **Core reference** — one file per circuit (76). Entry point: `manual/circuits/index.md`. |
| `manual/images/` | — | `page-NNN.png` full-page renders (`NNN` = PDF page); local-only, git-ignored, optional (derived from the source PDF). |

### Finding a circuit

`manual/circuits/index.md` has: a **jack-type legend** (`CV`, `pitch`, `0..1`,
`gate`, `trigger`, `integer`, `stepped`, `text`; `☞ smart` = smart default), an
**alphabetical master table** (function, category, RAM bytes, page range,
searchable keywords — grep the tags), a **by-function grouping**, and an
**obsolete list** (⚠️). Each `circuits/<name>.md` has YAML frontmatter
(`circuit`, `title`, `obsolete`, `ram_bytes`, `manual_pages`, `category`,
`tags`, `see_also`) then prose, `droid` examples, and full **Inputs/Outputs**
tables.

### Conventions

- DROID code is fenced ```` ```droid ````; register/cable names in backticks
  (`I1`, `O3`, `B1.1`, `L1.1`, `_MYCABLE`).
- Cross-references are relative Markdown links between docs.
- RAM budget: the master has ~110 000 free bytes; each circuit's cost is listed.
