---
name: droid-patch
description: Write or edit a DROID patch file (droid.ini or any other .ini name) and validate it. USER-INVOCABLE ONLY — trigger exclusively when the user explicitly runs /droid-patch or directly asks in this turn to "write me a droid patch" / "edit this droid.ini" / equivalent. Never trigger proactively or infer this intent from surrounding work (e.g. touching the vcvoid engine/plugin code does not imply the user wants a patch written).
---

# droid-patch

Write and validate DROID patch files (the hardware CV system emulated by this
repo's plugin). This skill is a reference card, not a tutorial — consult the
manual for anything not covered here.

**Only act on explicit invocation.** If the user hasn't just asked for a
patch to be written/edited/validated in this turn, don't use this skill.

## Naming the patch file

`droid.ini` is just the **default** name — prefer a short descriptive name
derived from what the user asked for (e.g. `swing-clock.ini`,
`three-voice-arp.ini`), like the examples in `patches/`. Any `*.ini` name
works everywhere in this repo (droidcheck, patchsmoke, the plugin's
load-patch menu). The one place the name is fixed is a **real master's
micro-SD card**, where the file must be called exactly `droid.ini` — mention
that when the user intends to run the patch on hardware. Only default to
`droid.ini` when no better name suggests itself or the user asked for it.

## Source of truth

Before writing anything non-trivial, check:

- `manual/circuits/index.md` — the master table of all 76 circuits (name,
  function, category, RAM cost, tags) and the **jack-type legend** (`CV`,
  `pitch`, `0..1`, `gate`, `trigger`, `integer`, `stepped`, `text`,
  `☞ smart`). Don't guess a circuit's jacks — read `manual/circuits/<name>.md`
  for its Inputs/Outputs tables and worked examples.
- `manual/basics.md` §5 — the full text-syntax reference this skill
  summarizes (register table, error codes, number formats, abbreviations).

## Core syntax

```droid
[lfo]
    hz = 5
    square = O1
```

A patch is a list of `[circuit]` blocks, each with `param = value` lines.
Comments start with `#`. One patch = one file, ≤ **64 000 bytes** (spaces and
comments are stripped before the limit is checked, so they're free).

## Registers

- `I1`…`I8` — CV inputs (MASTER only; MASTER18 has `I1`/`I2` as gate inputs).
- `O1`…`O8` — CV outputs.
- `N1`…`N8` — normalizations: write a signal here and it becomes `I<n>`'s
  default when nothing is physically patched into that input.
- `G1`…`G8` (MASTER) / `G2.1`… (MASTER18, G8 expanders) — gate in/out, 0 V or
  5 V when driven as output, ~0.75 V threshold as input.
- `P<c>.<n>` pot, `B<c>.<n>` button (1 while held, else 0), `L<c>.<n>` button
  LED (output, 0.0 dark…1.0 full brightness), `S<c>.<n>` switch,
  `E<c>.<n>` encoder (only valid inside `encoder`/`encoderbank`/`encoquencer`),
  `R1`…`R56` raw LED-matrix override — `<c>` is controller position in the
  patch's declared chain order, not physical slot.
- `_NAME` — internal cable (any name starting with `_`).

## Voltages ↔ numbers

Jacks are −10 V…+10 V; internally everything is the number range −1…+1
(2.5 V ≡ `0.25`). Write either `2.5V` or `0.25` — a trailing `V` suffix
divides by 10, `%` divides by 100. `on`/`off` are shorthand for `1.0`/`0.0`.
Values clamp to ±10 V at the jacks; internal 32-bit float math is unbounded.
Plain numbers must be written in full decimal (`0.5`, not `.5`; no scientific
notation).

## Input math: A × B + C

Every **input** (not output) has three columns — value **A**, factor **B**,
offset **C** — computed as `A × B + C`:

```droid
    hz = P1.2 * 9 + 1        # A=P1.2, B=9, C=1 → range 1..10 Hz
    input = I1 + I2          # two-signal mix (no factor)
    input = P1.1 - 0.5       # shorthand for + -0.5
    level = -P1.1            # shorthand for P1.1 * -1
    pitch = I1 / 12          # division by a FIXED number only, folds into B
```

Rules: exactly one multiplication and one addition, multiplication first;
`A / P1.1` (dividing by a register) is **not allowed** — use the `math`
circuit instead. `-P1.1 * I1` is also not allowed (that's two
multiplications). If a line can't be reduced to `A × B + C`, it's rejected.

## One output per jack — read it back downstream

An `O`/`N`/`G`/`L`/`_cable` output register **is not an error if assigned
twice** in different circuits — the last one loaded silently wins, no
"problems" flag. So: **route each output register exactly once**, and if a
value is needed in more than one place, read the register back as an input
downstream instead of writing it twice:

```droid
[lfo]
    square = O1        # the only writer of O1

[contour]
    trigger = O1        # reads O1 back as a clock, doesn't rewrite it
    output = O2
```

Internal cables (`_NAME`) are stricter and DO get flagged: a cable must have
**exactly one output** and **at least one input**, or droidcheck reports it
as a problem (unused-as-input / unused-as-output / double-output).

## Overlaying (one control, many functions)

Duplicate a `pot`/`button` circuit per function, and gate each copy with
`select` (+ optional `selectat`) driven by a `button`, `buttongroup`, or
switch register:

```droid
[buttongroup]
    button1 = B1.1
    button2 = B1.2
    output = _MODE

[pot]
    pot = P1.1
    select = _MODE
    selectat = 0
    output = _ATTACK

[pot]
    pot = P1.1
    select = _MODE
    selectat = 1
    output = _RELEASE
```

An unselected circuit's `led`/output jacks go inactive (freeze/dark) — use
`buttonoutput<n>` instead of `led<n>`/`output` when a value must stay live
regardless of selection (e.g. nested/multi-level menus). Exactly one circuit
in an overlay group must be selected at any time.

## Presets

Stateful circuits (`algoquencer`, `motoquencer`, `pot`, `button`,
`buttongroup`, `encoder(bank)`, `matrixmixer`, `fader(bank|matrix)`,
`motorfader`, `notebuttons`, `nudge`, `calibrator`, …) expose `preset`,
`loadpreset`, `savepreset`:

- **Triggered**: `preset` selects the slot number (0-based), a trigger to
  `loadpreset`/`savepreset` fires the load/save.
- **Immediate**: omit `loadpreset`/`savepreset` — changing `preset` switches
  state instantly and autosaves the outgoing state.
- **Trigger-encodes-number**: omit `preset` — a nonzero value sent to
  `loadpreset`/`savepreset` itself is rounded to the preset number (can't
  reach preset `0` this way except by sending e.g. `0.3`, which is `>0.1`
  true-threshold but rounds to `0`).
- Guard load/save buttons with a `button` circuit's `longpress` output
  (≥1.5 s hold) to avoid accidental overwrites.

## Tap tempo

Circuits with a `taptempo` input (`burst`, `contour`, `gatetool`, `lfo`, …)
derive an interval from the last 2–3 triggers sent to it — either a manually
tapped button, or a steady clock patched in directly. A new tap sequence
starts if the gap since the last tap exceeds 4 s; default at boot is 0.5 s
(2 Hz). `taptempo` does not sync phase — patch `sync` too if that matters.

## Texts

Circuits needing text params (e.g. `display`'s `header`) take a double-quoted
ASCII string (code 32…126, no embedded `"`); the parser interns each unique
string to an integer, so text values can flow through ordinary numeric
registers/cables and even take the input-math treatment (`text = "X" * B1.1`
shows "X" only while the button is held, since `0` renders as empty).

## Common mistakes (rejected by the loader)

- Assigning an internal cable (`_NAME`) as output in two places, or never
  reading it as an input anywhere.
- Writing to an `I`/`B`/`P`/`S` register (inputs) as if it were an output.
- Line over 63 characters (local "line too long" error) — irrelevant to the
  64 000-byte total limit, but still rejected per-line.
- Unknown circuit name, unknown parameter name, unknown register name,
  invalid parameter value, invalid `[circuit]` header syntax — see the full
  error-code/color table in `manual/basics.md` §5.4.
- Using a deprecated (⚠️ `obsolete: true`) circuit — not a hard error, but
  droidcheck flags it; avoid in new patches.

## Abbreviated parameter names

Every parameter has a short form (e.g. `clock` → `c`, `pattern` → `pt`,
`button1` → `b1`) to shrink patch size; both forms are accepted when
loading. Don't rely on this table from memory for a specific circuit — check
that circuit's manual page, or just use full names (clearer, and the byte
budget is rarely the binding constraint for hand-written patches).

## Validation loop

Two independent checkers, both fully headless (neither touches a running VCV
Rack instance) — run both when the plugin build/tools are available; if this
repo checkout doesn't have `tools/droidcheck/vendor/` built yet, note that
instead of skipping validation silently.

1. **Forge parity (`droidcheck`)** — the same "problems" check the Forge GUI
   applies, reused headlessly:
   ```sh
   cd tools/droidcheck && ./build.sh          # once: clones+builds the Forge's own validator
   ./build/droidcheck path/to/patch.ini        # exit code = number of patches with problems
   ```
   Catches: unknown/deprecated circuit, unknown parameter/register, cable
   misuse, registers not present on the chosen master, out-of-memory. Note:
   many older community patches report "deprecated circuit" problems on
   blue-7 even though they ran fine on the firmware they were written for —
   that's expected noise, not a real bug in the patch.

2. **Engine load+run smoke (`patchsmoke`)** — a standalone headless binary
   that exercises this repo's own engine (parser, RAM accounting, first tick
   of simulated execution), not the Forge's:
   ```sh
   make patchsmoke
   build/patchsmoke path/to/patch.ini          # exit code = number of failing patches; prints PASS/FAIL + RAM used
   ```

Prefer running both before handing a patch back: `droidcheck` for
Forge/hardware-loadability parity, `patchsmoke` for confirming this repo's
engine actually runs it for a simulated second without erroring. **Never
launch or drive VCV Rack as part of writing a patch** — live-Rack
verification belongs to the `uat-bridge` skill and only ever runs on an
explicit user request (see that skill for why).
