---
circuit: crossfader
title: Morph between 8 inputs
obsolete: false
ram_bytes: 40
manual_pages: [189, 189]
category: mixer-cv
tags: [crossfader, crossfade, morph, mix, fade, blend, eight-inputs, utility]
see_also: [lfo]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Linear interpolation across up to 8 inputs with a well-defined fade-to-segment mapping and wraparound beyond 1.0.
verification_note: "Headless: sweep fade across 0.0-1.3333 with known inputs and assert the output matches the computed two-neighbor linear mix, including the wrap from last input back to first."
---

# crossfader — Morph between 8 inputs

This utility circuit creates a CV controlled mix of two out of up to eight
inputs.

With two inputs this acts like a classical cross fader. The following example
lets you fade between the signals at `I1` and `I2` by turning the pot `P1.1`:

```droid
[crossfader]
    input1 = I1
    input2 = I2
    fade   = P1.1
    output = O1
```

At fully CCW (0.0) only the signal of the first input is being output, at fully
CW (1.0) only that of the second one. In the center position (0.5) you get the
average of both inputs, namely 0.5×I1 + 0.5×I2.

Using more than two inputs is possible. The `fade` input then maps the range
0.0 … 1.0 to a journey from the first to the last input. Let's see the following
example:

```droid
[lfo]
    hz                = 0.1
    sawtooth          = _FADE

[crossfader]
    input1            = I1
    input2            = I2
    input3            = I3
    input4            = I4
    fade              = _FADE
    output            = O1
```

Now during one LFO cycle of 10 seconds the output `O1` begins with the signal at
`I1` and then morphs to that of `I2`. It reaches 100% of `I2` at a fade value of
⅓. Then it continues to `I3`, which it reaches at ⅔ and finally – after 10
seconds – it ends at `I4`. After that it immediately jumps back to `I1`, in order
to begin the next cycle.

Values beyond 1.0 for `fade` are allowed and allow you to morph from the last
input to the first one. In the previous example that would be the range from 1.0
to 1.3333. So if you scale up the sawtooth to a total range of 0.0 … 1.3333 you
will get a smooth cyclic morph between all four inputs:

```droid
[lfo]
    hz               = 0.1
    sawtooth         = _FADE

[crossfader]
    input1           = I1
    input2           = I2
    input3           = I3
    input4           = I4
    fade             = _FADE * 1.3333
    output           = O1
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input1 … input8` (`i`) | CV | `0.0` | The input signals that you want to crossfade between. At least `input1` and `input2` need to be patched. Otherwise they are treated like 0 V signals. |
| `fade` (`f`) | `0..1` | `0.5` | This value decides which of the two inputs should be mixed and to which degree each one should go into the mix. At 0.0 the mix consists of 100% of the first inputs, at 1.0 of 100% of the last patched input. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | Output of the mix. |
