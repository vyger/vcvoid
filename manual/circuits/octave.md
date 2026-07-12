---
circuit: octave
title: Multi-VCO octave animator
obsolete: false
ram_bytes: 32
manual_pages: [355, 356]
category: quantizer-pitch
tags: [octave, vco, oscillator, detune, spread, fifths, fat, animation, pitch, 1v-oct]
see_also: [random]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Small circuit, but the stepped octave/fifths spread mapping and the pitch-compensated linear (constant-Hz) detune need care to reproduce faithfully.
verification_note: "Both the spread quantization (with/without fifths) and the linear-detune formula (4 semitones at 0V, halving per octave) are documented and deterministic, so all three outputs can be checked headless against a computed reference."
---

# octave — Multi-VCO octave animator

This circuit is used to control the pitches of three oscillators by octave or
even fifths. It also allows a linear detune in order to make the common sound of
the VCOs sound fatter.

Here is an example for a setup where the octave spreading and the detune is
controlled with two pots:

```droid
[octave]
    input        = I1
    output1      = O1
    output2      = O2
    output3      = O3
    spread       = P1.1
    detune       = P1.2
```

Patch the 1 V / octave inputs of three VCOs at `O1`, `O2` and `O3`. Tune all
VCOs at exactly the same pitch. Patch the pitch output from your sequencer,
quantizer or whatever to `I1`.

Now with the pot `P1.1` turned fully left nothing changes. All VCOs will get
exactly the same pitch. As you turn up the pot the pitches of the VCOs 2 and 3
will start to get octaved up more and more until VCO 2 is two octaves above VCO 1
and VCO 3 is four octaves above VCO 1.

If you add `fifths = on` then intermediate steps shift the pitch by perfect
fifths.

Note: The output `output1` was implemented just for sake of completeness. It
passes through the input to `output1`, since the pitch of VCO 1 is never detuned
nor pitched up. If you are running low in outputs then some use a passive
multiple or stacked cable and connect VCO 1 externally the pitch and thus save
one output.

## Detune

In the example, if you turn `P1.2`, VCO 2 will be detuned up and VCO 3 down. A
very slight turn will get you the nice fat classical detune sound. The speciality
here is: the detune is *linear*. This means that the detune is always done by the
same number of *Hertz* – regardless of the current pitch. This is done by
automatically adapting the detune voltage to be less in higher pitches and
greater in lower pitches. The result is a beating independent of pitch.

## Animation

Since everything in DROID is CV'able so is `spread`. A nice application is to use
a sequencer or clocked random generator for *animating* the octaving. Here is an
example:

```droid
[random]
    trigger     = I1
    output      = _RANDOM

[octave]
    input       = I1
    output1     = O1
    output2     = O2
    output3     = O3
    spread      = _RANDOM * P1.1
```

Now `P1.1` controls the depth of random octave animation.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | pitch | `0V` | The general pitch information on a 1 V / octave base to be used for the three VCOs. |
| `spread` (`s`) | stepped | `0` | The amount of octave spread between `output1` and `output3`. At a value of `1.0` the spread is four octaves. |
| `detune` (`d`) | 0..1 | `0.0` | The amount of linear detune of VCO 2 and 3. This is not on a 1 V / octave base but corresponds to an absolute frequency difference in Hertz. The exact frequency difference cannot be set here, since that depends on how you have tuned your VCOs. But the rule is the following: If `input` is at 0 V and `detune` is `1.0`, the detune is by four semitones. And for an input of 1 V (one octave higher) it is just two semitones, because that results in the same frequency difference. For 2 V (two octaves up) it is just one semitone and for 3 V half a semitone (and so on). Best thing is to simply try out and listen! |
| `fifths` (`f`) | gate | `off` | Set this to 1 or `on` if you want to include perfect fifths as intermediate steps. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output1 … output3` (`o`) | pitch | Outputs for the 1 V / octave of the three VCOs. `output1` is an exact copy of `input` so you could omit that and rather patch VCO 1 to the original pitch CV. |
