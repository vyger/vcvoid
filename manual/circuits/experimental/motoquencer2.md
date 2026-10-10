---
circuit: motoquencer2
title: Motor fader sequencer with a narrowed probability lane
obsolete: false
experimental: true
ram_bytes: 1188
manual_pages: []
category: sequencer
tags: [sequencer, motor faders, m4, motoquencer, gate probability, probability, chance, trig condition, conditional, every other turn, notches, fadermode, performance, experimental]
see_also: [motoquencer, encoquencer, motorfader, algoquencer]
impl_difficulty: easy
controller_binding: controller-required
verification: headless
spec_gap: false
difficulty_note: "motoquencer verbatim (the shared sequencer core plus the M4 editing surface) with one extra input that selects a subset of the gate-probability lane's eight notches. Stored values, presets, saved state, the LED codes and the playing rule are untouched; only the fader's notch grid, its snap/nudge targets and the position a stored value is displayed at change."
verification_note: "Headless: notch count and snap targets per mode, the stored index read back on the eight-notch grid, a value outside the active subset surviving a mode switch and still playing on its own turns, a chain member's own probabilitymode ignored, luckygateprob drawing only subset members, and — at the default mode 0 — motoquencer's own goldens reproduced circuit-for-circuit."
---

# motoquencer2 — Motor fader sequencer with a narrowed probability lane

> **⚠️ EXPERIMENTAL — voidbot only.**
> `motoquencer2` is **not** a DROID circuit. It does not exist in any firmware,
> the Droid Forge does not know it, and a patch using it **will not run on DROID
> hardware**. voidbot refuses to load such a patch until you enable
> **"Allow experimental circuits"** in the master module's context menu.
> See [the experimental circuits index](index.md).

This circuit is [`motoquencer`](../motoquencer.md) with one extra input,
`probabilitymode`. Everything else — all 103 other inputs, all nine outputs,
every fader and button mode, the forms, patterns, presets, linking and "I Feel
Lucky" — is `motoquencer`, and the full reference for it is
[that page](../motoquencer.md). **Read it there; only the delta is described
here.** With `probabilitymode` left at its default `0` the two circuits are
identical, so you can rename a `[motoquencer]` to `[motoquencer2]` and nothing
changes until you use the new input.

## What it changes

The gate-probability fader lane (`fadermode = 2`) has eight notches, and they
mix two unrelated families of setting — plain random chances and
Elektron-style trig conditions:

| Pos. | Meaning | Family |
|------|---------|--------|
| 8 (top) | played always | both |
| 7 | random chance of 50 % | random |
| 6 | played every *even* turn | trig condition |
| 5 | played every *odd* turn | trig condition |
| 4 | random chance of 25 % | random |
| 3 | played every 4th turn | trig condition |
| 2 | random chance of 12 % | random |
| 1 | played if previous random was positive | random |

When a track only wants one family, the other four notches are in the way, and
finding "every odd turn" by feel between "25 %" and "every 4th" is fiddly.
`probabilitymode` removes the notches you are not using, so the fader has fewer
and wider dents and every one of them is a setting you want:

| `probabilitymode` | Notches offered (top to bottom) | Count |
|---|---|---|
| `0` (default) | all eight, exactly as `motoquencer` | 8 |
| `1` — random | always, 50 %, 25 %, 12 %, conditional | 5 |
| `2` — trig conditions | always, every even turn, every odd turn, every 4th turn | 4 |

Nothing else about gate probability changes. The value a step stores is the same
one `motoquencer` stores, so presets, saved state, the LED colours and blink
codes and `luckygateprob`'s range all behave exactly as documented on the
`motoquencer` page.

## It never rewrites a step

Narrowing the lane is purely a change to the **editing surface**. A step already
holding a setting the active mode does not offer — dialled in another mode,
restored from a saved patch, or recalled from a preset — **keeps it and keeps
playing it**. Its fader shows the nearest offered notch (on a tie, the lower
one), and the stored setting changes only when you actually move that fader.

So in mode `2` you can dial a step to "every odd turn", switch to mode `1`, and
the step goes on playing on odd turns; its fader sits at the "25 %" dent, which
is the closest mode `1` has, and the moment you move it the step becomes
whatever you moved it to. Switch back to `2` before touching it and the fader
returns to "every odd turn", none the worse.

`luckygateprob` also draws only from the offered notches, so a reroll cannot put
back a setting the lane is hiding.

## Fixed, and read from the chain main

`probabilitymode` is a **setting**, not a modulation source: wire it to a
constant, a switch or a `buttongroup`, the way you would `fadermode`. A change
takes effect at once (the faders travel to their values' new positions), so a
switch works as a live A/B, but there is no musical meaning to modulating it
from an LFO.

In a [`linktonext`](../motoquencer.md) chain it is read **from the chain's main
circuit only**, like `fadermode` and `buttonmode` — the probability lane is one
menu the whole chain shares. A linked member's own `probabilitymode` is ignored.
Unlike `fadermode` there is no `+ 10` addressing: the notch set is not a
per-member choice, so the main's value applies to every lane in the chain.

## Example

A four-step track on one M4, with a pot selecting the fader mode and a two-state
button switching the probability lane between the random chances and the trig
conditions:

```droid
[p2b8]
[m4]

[button]
    button = B1.1
    led    = L1.1
    output = _PROBMODE      # 0 = random chances, 1 = trig conditions

[motoquencer2]
    clock           = I1
    fadermode       = P1.1 * 7
    probabilitymode = _PROBMODE + 1
    cv              = O1
    gate            = O2
```

With the button up, `probabilitymode` is `1` and the probability fader has five
dents: always / 50 % / 25 % / 12 % / conditional. With it down it is `2` and the
fader has four: always / even / odd / every 4th.

## Inputs

Every input of [`motoquencer`](../motoquencer.md), plus:

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `probabilitymode` (`pm`) | integer | `0` | Which notches the gate-probability lane (`fadermode = 2`) offers. `0` = all eight, exactly as in `motoquencer`. `1` = the five random chances (always, 50 %, 25 %, 12 %, conditional). `2` = the four trig conditions (always, every even turn, every odd turn, every 4th turn). Values outside 0 … 2 are rounded and clamped. Narrows the editing surface only: a step's stored setting is never rewritten, and a setting the mode does not offer keeps playing until that step's fader is moved. Read from the chain main in a `linktonext` chain. |

## Outputs

Exactly those of [`motoquencer`](../motoquencer.md); this circuit adds none.

## See also

- [`motoquencer`](../motoquencer.md) — the real DROID circuit this extends, and
  the reference for every other input, output and feature. Use it whenever the
  patch has to run on hardware.
- [`encoquencer`](../encoquencer.md) — the same sequencer on E4 encoders (no
  `probabilitymode`).
- [`motorfader`](../motorfader.md) — a single motor fader, where `notches`
  shapes the dents directly.
