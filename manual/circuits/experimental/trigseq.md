---
circuit: trigseq
title: Declarative trigger sequencer
obsolete: false
experimental: true
ram_bytes: 32
manual_pages: []
category: clock-timing
tags: [trigger, sequencer, rhythm, pattern, text, drum, grid, gate, clock, chain, chaintonext, experimental]
see_also: [euklid, sequencer, clockedtrigger]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Text-driven step sequencer over euklid's transport and output contract; the only new mechanics are pattern indexing and sequencer-style chaining.
verification_note: "Headless: clock the circuit and assert output/offbeats per step for pattern placement, every rest character, clock passthrough, outputsignal hold, reset arming and chained/empty instances."
---

# trigseq — Declarative trigger sequencer

> **⚠️ EXPERIMENTAL — vcvoid only.**
> `trigseq` is **not** a DROID circuit. It does not exist in any firmware, the
> Droid Forge does not know it, and a patch using it **will not run on DROID
> hardware**. vcvoid refuses to load such a patch until you enable
> **"Allow experimental circuits"** in the master module's context menu.
> See [the experimental circuits index](index.md).

This circuit plays a rhythm that you *write out* in the patch rather than define
via controllers. Each clock advances one character of a text pattern; characters
that are not rests emit a trigger.

```droid
[trigseq]
    clock   = G1
    pattern = "x...x.x."
    output  = G3
```

That patch fires on the 1st, 5th and 7th of every eight clocks. The rhythm is
visible in the patch file at a glance and is declared statically. It is similar
to, but distinct from, [`euklid`](../euklid.md) (which can only distribute beats
*evenly*) and [`sequencer`](../sequencer.md) (which needs one jack per step).

## Rests and triggers

Four characters are **rests**:

| Character | Name |
|-----------|------|
| `.` | period — the canonical rest |
| ` ` | space |
| `-` | dash |
| `_` | underscore |

**Every other character emits a trigger.** Use whichever notation you find
readable — `x`, `X`, `*`, `o`, `1`, or a drum letter like `k` or `s`. All of
these are the same eight-step rhythm:

```droid
    pattern = "x...x.x."
    pattern = "*---*-*-"
    pattern = "k___k_k_"
    pattern = "1 . . 1 . 1 . ."     # NO — see below
```

The last line is *not* the same rhythm: a space is a rest **and it is a step**,
so that pattern is 15 steps long, not 8. There is no grouping or separator
syntax — every character you type is one step. In particular:

- `0` **emits**. It is not a rest, despite looking like one in binary notation.
- `|` **emits**. It is not a bar line, it is a step.

There is no validation and nothing to get wrong at load time: a pattern is
simply read left to right, and anything that is not one of the four rest
characters plays.

## Length

The pattern's length *is* the string's length. A seven-character pattern is a
seven-step sequence:

```droid
[trigseq]
    clock   = G1
    pattern = "x..x..x"      # 7 steps against a 4/4 clock — a rolling phase
    output  = G3
```

The patch format limits any text to **18 characters** (a master-side limit; the
Forge reports "The maximum allowed text length is 18" beyond it). For longer
sequences, chain instances — see below.

## Passing the clock through

On an emitting step the input clock is sent to `output` **unchanged** — same
voltage, same gate length. On a rest step it is sent to `offbeats` instead. So
the gate length is whatever your clock source produces, and
[`clocktool`](../clocktool.md) or [`gatetool`](../gatetool.md) shape it exactly
as they would any other trigger stream.

`offbeats` is the exact complement of `output` — the hats between the kicks,
without writing a second, inverted pattern:

```droid
[trigseq]
    clock    = G1
    pattern  = "x..x..x."
    output   = G3          # kick
    offbeats = G4          # everything else
```

If you would rather have a **gate for the whole step** than a copy of the clock,
patch `outputsignal`. Its value is then held for the entire active step, until
the next clock:

```droid
[trigseq]
    clock        = G1
    pattern      = "x..x..x."
    outputsignal = 1        # 10 V for the whole step, not just the clock pulse
    output       = O1
```

`outputsignal` is read live, so it does not have to be a constant — patch a CV
there and `trigseq` becomes a rhythmic stencil, letting that CV through only on
the steps you wrote.

## Reset

A trigger at `reset` restarts the pattern. Like [`euklid`](../euklid.md), the
restart is *armed* and consumed by the next clock — a reset never emits a
trigger by itself, and nothing at all is emitted before the first clock after
the patch loads.

If a reset and a clock arrive at the same moment, the reset is applied first, so
that clock plays step 1.

## Making longer sequences

Set `chaintonext` to 1 and the **next** `trigseq` circuit's pattern is appended
to this one, forming a single longer sequence. Only the first circuit needs
`clock`, `reset`, `outputsignal` and the outputs; the followers just supply a
pattern. This is the same mechanism [`sequencer`](../sequencer.md) uses.

```droid
[trigseq]
    clock       = G1
    pattern     = "x...x.x."      # bar 1
    chaintonext = 1
    output      = G3

[trigseq]
    pattern     = "x...x.x.."     # bar 2 — one step longer, so the phrase limps
    chaintonext = 1

[trigseq]
    pattern     = "xxxx"          # a fill
```

Add as many as you like; all but the last need `chaintonext = 1`. The chain also
ends if the next circuit is not a `trigseq`.

Notes:

- `chaintonext` is **dynamic**: drive it from a button or a toggle to make or
  break the chain while the patch runs. The step counter keeps running, so
  breaking a chain mid-phrase folds the position into the shorter sequence
  rather than resetting it.
- An **empty pattern contributes nothing at all** — it is skipped, not played
  as a rest. Blanking one instance's pattern shortens the sequence cleanly:

  ```droid
  [trigseq]
      pattern = ""      # this phrase is "muted": zero steps, not a silent bar
  ```

## Memory

A typical instance costs about **70 bytes**: 32 for the circuit, 20 for `clock`,
8 for the `pattern` jack, 4 for `output`, and 6 for the text itself. The text is
charged as a pointer and a length, so a two-character pattern and an
eighteen-character pattern cost exactly the same.

## Notes

- The pattern **cannot be changed while the patch is running**. A quoted text is
  a constant baked into the patch when it loads, so `pattern` cannot be driven
  from a button, a cable or any other circuit — the same is true of every text
  value in DROID, such as the ones you send to a
  [`display`](../display.md).
- `trigseq` saves no state. Reloading a patch always starts at step 1.
- **Possible future addition:** an `offset` input to rotate the pattern by ±N
  steps, as [`euklid`](../euklid.md) has. It was deliberately left out of the
  first version because its meaning under `chaintonext` — rotate this instance,
  or the whole chained sequence? — deserves its own decision rather than a
  guess.
- **Possible future addition:** multiple outputs based on a class of `trigger`
  characters. Example:
  ```droid
  [trigseq]
    clock       = G1
    pattern     = "A...AB..A.B.ABBB"
    output1     = G3
    output2     = G4
    offbeats    = G5
  ```
  This patch would produce 3 distinct rhythms on the different outputs. `A`
  triggers would go to `output1`, `B` triggers to `output2`, and rests would go
  to `offbeats`. The mapping of symbol to output would be defined by the order
  in which they appear in the string — or, as an alternative worth considering,
  declared explicitly:

  ```droid
  [trigseq]
    pattern     = "A...AB..A.B.ABBB"
    symbol1     = "A"
    symbol2     = "B"
  ```

  Positional mapping keeps patterns terse, but it is *silently* order-dependent:
  inserting a `B` ahead of the first `A` swaps which output each symbol drives,
  so editing a rhythm can repatch the drums. Explicit symbols are wordier and
  immune to that.

  WARNING: this would break the behavior of any patches using the old behavior
  so tread carefully — today every non-rest character is documented as
  equivalent, so an existing `"k___k_k_"` would stop being one rhythm the moment
  symbols carry meaning. Gating the split on a second output being patched would
  keep old patches intact.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `clock` (`c`) | gate | | Patch a clock signal here. Each trigger advances the pattern by one character. It does not need to be steady. Note: this input is classified as a gate input, since the length of the gate is preserved when forwarded to `output` and `offbeats`. |
| `reset` (`r`) | trigger | | A trigger here restarts the pattern. The restart is armed and consumed by the next clock, so a reset never emits a trigger of its own. |
| `pattern` (`p`) | text | | The rhythm, written as a quoted text. `.`, space, `-` and `_` are rests; every other character emits a trigger. The length of the text is the length of the sequence (max 18 characters — the patch format's text limit). An empty pattern contributes no steps at all. Cannot be changed while the patch is running. |
| `outputsignal` (`os`) | CV | | Usually `trigseq` just lets the input clock through to the output on emitting steps. If this parameter is used, its value is sent to the output for the whole active step instead. The easiest application is setting it to `1`, which turns the output into a gate that lasts the entire step. |
| `chaintonext` (`cn`) | gate | | If you set this input to 1, the next `trigseq` circuit's pattern is appended to this one's, forming one longer sequence. See "Making longer sequences". This input is dynamic — you may make or break the chain while the patch runs. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | gate | Emits on every step whose character is not a rest. The gate length and voltage are taken directly from the input clock, unless `outputsignal` is used. |
| `offbeats` (`ob`) | gate | Emits on every rest step — the exact complement of `output`. |
