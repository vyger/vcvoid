---
circuit: crossfader2
title: Morph between 8 inputs, smoothly
obsolete: false
experimental: true
ram_bytes: 56
manual_pages: []
category: mixer-cv
tags: [crossfader, crossfade, morph, mix, fade, blend, eight-inputs, cubic, fourier, band-limited, smooth, interpolation, spline, loop, wavetable, lfo, faders, experimental]
see_also: [crossfader, lfo, faderbank, slew]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: "crossfader's segment mapping plus pure functions: a loop flag that changes the segment count from N-1 to N, a monotone cubic Hermite kernel and a periodic-sinc (Fourier) kernel over the ring."
verification_note: "Headless: curve=0 must reproduce crossfader's goldens exactly; curve=1 must hit every input, stay inside the two neighbours' range, give a flat plateau between equal inputs; curve=2 must hit every input and show the periodic-sinc lobes (N=4 spike: 0.603553 / -0.103553); loop=1 must reach input1 again at fade=1.0 with N equal segments."
---

# crossfader2 — Morph between 8 inputs, smoothly

> **⚠️ EXPERIMENTAL — vcvoid only.**
> `crossfader2` is **not** a DROID circuit. It does not exist in any firmware,
> the Droid Forge does not know it, and a patch using it **will not run on DROID
> hardware**. vcvoid refuses to load such a patch until you enable
> **"Allow experimental circuits"** in the master module's context menu.
> See [the experimental circuits index](index.md).

This circuit is [`crossfader`](../crossfader.md) with two extra inputs. With
both of them left at their defaults it behaves exactly like `crossfader` —
same inputs, same `fade` mapping, same wraparound past 1.0, same output — so
you can rename a `[crossfader]` to `[crossfader2]` and nothing changes until
you use one of the new inputs:

- **`curve`** chooses how the output moves *between* the inputs.
  `crossfader` draws a straight line, which leaves a corner at every input.
  `curve = 1` draws a smooth cubic curve through the inputs that never
  overshoots, and `2` the smoothest possible curve through them
  (band-limited), which overshoots and rings.
- **`loop`** closes the ring. `crossfader` reaches the last input at
  `fade = 1.0`; with `loop = 1`, `fade = 1.0` is back at the first input, and
  the journey from the last input to the first is one more equal segment.

## Drawing an LFO waveform on motor faders

The reason this circuit exists. Put a patched value on each of eight inputs —
here the eight faders of two M4s, via [`faderbank`](../faderbank.md) — and
sweep `fade` with a sawtooth. The faders are then the waveform, one cycle per
sawtooth period:

```droid
[faderbank]
    firstfader = 1
    output1 = _W1
    output2 = _W2
    output3 = _W3
    output4 = _W4
    output5 = _W5
    output6 = _W6
    output7 = _W7
    output8 = _W8

[lfo]
    hz = 0.5
    sawtooth = _PHASE

[crossfader2]
    input1 = _W1
    input2 = _W2
    input3 = _W3
    input4 = _W4
    input5 = _W5
    input6 = _W6
    input7 = _W7
    input8 = _W8
    fade   = _PHASE
    curve  = 1
    loop   = 1
    output = O1
```

With `crossfader` you would need `fade = _PHASE * 1.142857` (8/7) to close
the loop, and the output would have a corner at every fader. Here `loop = 1`
takes care of the first, `curve = 1` of the second. Rate, sync and phase are
the `lfo`'s business. Level, offset and polarity are one line of input math
on whatever reads the output: send it to a cable, `output = _WAVE`, and read
it back as `_WAVE * 2 - 1` for a bipolar swing.

## The three curves

All three pass through every input value exactly; they differ in what happens
in between. The best way to see it is a lone raised fader among low ones:

| `curve` | Name | Between two faders | A lone raised fader |
|---|---|---|---|
| `0` | linear | straight line, corner at each fader | a triangle |
| `1` | cubic | S-curve, no corner, stays inside the two faders' range | a rounded bump, flat beside it |
| `2` | Fourier | one smooth wave through all faders | a bump with ripples that decay across the whole cycle |

`1` is the tame one: what you draw is exactly the range you get. `2` is
allowed to **overshoot** — the output can go above the highest fader or below
the lowest — and that is what makes it read as "curvy" on a scope. With
eight faders the difference between `0` and `1` is small but the smoothness
makes a difference at low rates; `2` is a little more wild and organic.

### The ring

Every mode works on the ring of inputs — the neighbour of the last input is
the first one — regardless of `loop`, in the same way that `fade` beyond 1.0
already wraps in `crossfader`. Whatever `fade` does, the curve is therefore
one continuous periodic shape.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input1 … input8` (`i`) | CV | `0.0` | The input signals that you want to crossfade between. At least `input1` and `input2` need to be patched. Otherwise they are treated like 0 V signals. Exactly as in `crossfader`: the number of inputs N is the highest patched one, and gaps read 0 V. |
| `fade` (`f`) | `0..1` | `0.5` | Position of the morph. At 0.0 the output is 100 % of the first input. At 1.0 it is 100 % of the last patched input — or, with `loop` set, 100 % of the first input again. Values beyond 1.0 wrap around from the last input to the first, exactly as in `crossfader` (period N inputs). |
| `curve` (`cv`) | integer | `0` | Interpolation between the inputs. `0` = linear, bit-identical to `crossfader`. `1` = monotone cubic (never overshoots). `2` = Fourier (overshoots and rings). Values outside 0 … 2 are clamped. |
| `loop` (`lp`) | gate | `0` | When on, the inputs form a ring: `fade` 0.0 … 1.0 travels through all N patched inputs and back to the first one in N equal segments, so a sawtooth at `fade` gives a seamless cyclic morph. When off, `fade` 0.0 … 1.0 travels from the first to the last input in N − 1 segments, exactly as in `crossfader`. |

With one patched input the output is that input; with none it is 0 V.

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | Output of the mix. |

## See also

- [`crossfader`](../crossfader.md) — the real DROID circuit this extends. Use
  it whenever the patch must run on hardware; the hardware-compatible version
  of the example above is `crossfader` with `fade = _PHASE * 1.142857`.
- [`faderbank`](../faderbank.md) — eight M4 faders as eight CVs.
- [`lfo`](../lfo.md) — the sawtooth that sweeps `fade`.
