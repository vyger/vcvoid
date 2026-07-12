---
circuit: superjust
title: Perfect intonation of up to eight voices
obsolete: false
ram_bytes: 64
manual_pages: [392, 393]
category: quantizer-pitch
tags: [just-intonation, pure-intonation, tuning, chord, beatings, intervals, vco, calibration, pitch]
see_also: [calibrator]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: true
difficulty_note: Bypass/tuningmode/transpose plumbing is simple, but the core dynamic just-intonation retuning is the hard part.
verification_note: "Output is deterministic given inputs, but the intonation algorithm is described only conceptually and marked 'patent pending' (no ratio-selection rule, no averaging formula). Faithful reproduction and headless verification both need captured hardware input/output pitch pairs."
---

# superjust — Perfect intonation of up to eight voices

This circuit automatically creates a perfect pure intonation for up to eight
input pitches.

## Introduction

This means that all pitches are in just intervals, which correspond to small
whole number ratios such as 3⁄2 or 5⁄4. Assuming that you have perfectly tuned and
calibrated VCOs, if these pitches are used to play a chord, there will be no or
just minimal audible beatings and the chord will sound very pure.

In normal equal temperament intonation all intervals are a multiple of ¹²√2 and
thus there is no pure interval at all, with the exception of the octave. So all
chords will sound impure.

The problem about pure or just intonation is, that you need to decide for just
one scale, e.g. C major, and then tune all 12 notes in a way that chords from
that scale sound good. But as soon as you change the scale, the intervals will
sound ugly.

What makes the `superjust` unique is that fact, that it automatically creates a
pure intonation in a *dynamic* way. It constantly "listens" to the notes that are
*currently* being played and creates a perfect intonation just for those, not for
a scale or so. As soon as at least one note changes, all notes are retuned in
order to find a new perfect tuning. This is a bit like a well-trained string
ensemble or choir, where each musician listens and adjusts his or her pitch in
relation to all others.

## Usage

The nice thing is: you don't need any configuration. You need not specify any
information about the root note, the scale or anything else. Neither need the
inputs be quantized so some scale or tuned to 440 Hz. The circuit will simply
analyse all input pitches, apply its algorithm (patent pending) and then just
slightly raises or lowers each note so that at the end each pair of frequencies
have a rational oscillation ratio with small numerator and denominator. This is
done in a way that the average pitch does not change. Just pipe your pitches
through that circuit and you are done. And if you want to use a quantizer, use
`superjust` *after* quantization.

Here an example for three voices:

```droid
[superjust]
    input1 = I1
    input2 = I2
    input3 = I3
    output1 = O1
    output2 = O2
    output3 = O3
```

## Tuning

Of course, an exact tuning of your VCOs is crucial, since the pitch differences
between a normal tempered intonation and a perfect intonation are quite small.
The circuit helps you in the process of tuning with the inputs `tuningmode`,
which you can map to a toggle button:

```droid
[button]
    button = B1.1
    led = L1.1

[superjust]
    input1 = I1
    input2 = I2
    input3 = I3
    output1 = O1
    output2 = O2
    output3 = O3
    tuningmode = L1.1
```

Now when the button `B1.1` is active, all outputs will output zero volts. Tuning
with 0 V is not optimal in some cases. You should tune your VCOs always roughly
in the average pitch you play them. So you can set the tuning voltage with the
parameter `tuningpitch`. Here it is set to 2 V (2 octaves higher then 0 V):

```droid
[button]
    button = B1.1
    led = L1.1

[superjust]
    input1 = I1
    input2 = I2
    input3 = I3
    output1 = O1
    output2 = O2
    output3 = O3
    tuningmode = L1.1
    tuningpitch = 2V
```

Sometimes it is desirable to change the tuning pitch to other octaves on the fly.
This example uses pot `P1.1` for going through several octaves, and uses a
quantizer for creating steps of 1 V each:

```droid
[button]
    button = B1.1
    led = L1.1

[quantizer]
    input = P1.1
    steps = 1 # 1 step per octave
    output = _TUNINGPITCH

[superjust]
    input1 = I1
    input2 = I2
    input3 = I3
    output1 = O1
    output2 = O2
    output3 = O3
    tuningmode = L1.1
    tuningpitch = _TUNINGPITCH
```

## Perfect VCO calibration

If you *really* want to eliminate all beatings in your chords while using analog
VCOs, you probably need something to correct tracking deviations. Here I strongly
recommend using the circuit [`calibrator`](calibrator.md). Here is an example
with three voices, where buttons of a P2B8 are used for fine tuning the VCO
tracking in each octave:

```droid
[superjust]
    input1 = I1
    input2 = I2
    input3 = I3
    output1 = _O1
    output2 = _O2
    output3 = _O3

[calibrator]
    input = _O1
    output = O1
    nudgeup = B1.1
    nudgedown = B1.3

[calibrator]
    input = _O2
    output = O2
    nudgeup = B1.2
    nudgedown = B1.4

[calibrator]
    input = _O3
    output = O3
    nudgeup = B1.5
    nudgedown = B1.7
```

The number of pitch inputs and pitch outputs you patch should be identical.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input1 … input8` (`i`) | pitch | ☞ smart | 1st … 8th pitch input. Patch as many as you have voices; unpatched inputs are not used in the intonation. |
| `tuningmode` (`tm`) | gate | `0` | While this is 1, all outputs output the value set by `tuningpitch`. This is for tuning all outputs. Since perfect tuning is crucial for perfect intonation, this is quite useful. |
| `tuningpitch` (`tp`) | pitch | `0V` | This pitch CV will be output while the tuning mode is active. |
| `bypass` (`b`) | gate | `0` | While this is 1, all inputs are passed through to the outputs without changes. |
| `transpose` (`tr`) | pitch | `0V` | This value is being added to all outputs, but not in tuning or bypass mode. It can e.g. be used for making a vibrato on a chord. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output1 … output8` (`o`) | pitch | 1st … 8th pitch output. |

## See also

- [`calibrator`](calibrator.md) — correct VCO tracking deviations for even purer chords.
