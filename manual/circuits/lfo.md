---
circuit: lfo
title: Low frequency oscillator (LFO)
obsolete: false
ram_bytes: 216
manual_pages: [267, 268, 269, 270, 271, 272]
category: envelope-lfo
tags: [lfo, oscillator, waveform, sine, triangle, sawtooth, ramp, square, cosine, paraboloid, phase, sync, morph, randomize, taptempo, bpm]
see_also: []
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Standard LFO with deterministic waveform math; sync, morph and randomize add edge cases but the spec is complete.
verification_note: "Headless: sample the outputs over time and compare against computed waveform values at known hz/phase; sync and gate-reset behaviour are deterministic. Partial — the randomize output is non-deterministic and the morph interpolation curve should be spot-checked against a hardware reference."
---

# lfo — Low frequency oscillator (LFO)

A flexible low frequency oscillator with seven different waveforms, phase
modulation, flexible sync mechanisms, randomization, wave form morphing and
other interesting features.

Please note also that this LFO is not intended to be used at audio rate. It can
probably operate until roughly 1000-1500 Hz, but will sound ugly, distorted and
with many digital artefacts – especially the waveforms with steep edges like
square, ramp and sawtooth. If that's exactly what you intend, then maybe you
will have fun anyway.

## Waveforms

Here is the simplest possible patch. In this example the frequency is specified
in Hertz (cycles per seconds) and the triangle output is routed directly to
`O1`:

```droid
[lfo]
    hz       = 4
    triangle = O1
```

The resulting output is a triangle wave swinging between 0 V and 10 V.

This is how the `sawtooth` output looks like:

```droid
[lfo]
    hz       = 4
    sawtooth = O1
```

The `ramp` is similar but falling instead of rising.

The waveforms `sine` and `cosine` are similar but are one quarter cycle (90°)
apart.

`paraboloid` is *very* similar to sine, but is constructed based on quadratic
equations (which is faster).

Maybe the simplest waveform is `square`.

## Bipolar output, Level and Offset

Please note that the LFO outputs just positive voltage ranges until you set
`bipolar = on`. That extends the waveform to negative voltages (while doubling
the peak-to-peak voltage):

```droid
[lfo]
    hz       = 4
    bipolar = on
    triangle = O1
```

The inputs `level` and `offset` can be used to control the voltage range of the
outputs – which is here for your convenience and avoids the need for additional
circuits for doing this. The following example outputs a sine wave at 5 Hz to
`O4` that is gently oscillating between 2 V and 3.5 V:

```droid
[lfo]
    hz                      = 5
    level                   = 1.5V
    offset                  = 2V
    sine                    = O4
```

## Frequency control

The frequency of the LFO can be controlled in various ways. In the upper
examples we used the input `hz`. Here you specify the frequency of the LFO
directly in Hz. This is ideal when you want to set a fixed frequency with a
discrete number, rather than a control voltage. Here is a rectangle LFO running
at 1.5 cycles per second:

```droid
[lfo]
    hz            = 1.5
    rectangle     = O3
```

A more eurorack-like way is using the `rate` input, which works on a 1 V /
octave scheme. One "octave" here means that the frequency doubles. Here is an
example for creating a triangle LFO running at 4 Hz, since 2 V doubles the base
frequency of 1 Hz two times (instead of 2V you could also write 0.2):

```droid
[lfo]
    rate     = 2V
    bipolar = on
    triangle = O1
```

The third way is to use tap tempo by sending a steady clock into `taptempo`. The
LFO then mimics the speed of that input clock. This can even be combined with
`rate`: If you use both, then first `taptempo` is being used to set the speed and
then `rate` is used for altering that speed. So sending -1 V into `rate` will
create an LFO running at half clock speed (since -1 V pitches down the LFO by one
octave).

```droid
[lfo]
    taptempo      = G1 # steady clock here
    rate          = -1V # run at half clock speed
    sawtooth      = O2
```

And even `hz` can be used in combination. Now the speed of the taptempo is
multiplied with the value of `hz`. Otherwise stated: 1 Hz is the reference. The
following sets the LFO's frequency to three times the tap tempo:

```droid
[lfo]
    taptempo = G1
    hz = 3
    sawtooth = O2
```

## Hz vs BPM

Sometimes people ask for help converting BPM into Hz or vice versa. And some
even express their unhappyness about the fact that the Droid uses Hz rather than
BPM. Well – that decision was made because in general I see the LFO rather as an
oscillator than as a clock. And for oscillators Hz is the usual way to measure
the speed or frequency.

So when you use an LFO as your master clock, how can you convert specify BPM as
Hz? **Simply divide your BPM by 15** to get the correct value for `hz`. So 120
BPM would be `hz = 8`.

That sounds surprising, since Hz means *oscillations per second* and BPM *beats
per minute*. The point is: BPM means *beats* per minute, not clock ticks per
minute. In a modular environment it is most common to run your clock at 16th
notes. And the "beat" in BPM refers to quarter notes. For playing one quarter
note we need to play four 16th notes, so after dividing by 60 to convert minutes
into seconds, we need to multiply by 4, to convert quarters into 16th s.

That – of course – assumes that your master clock is running at 16th notes,
sometimes written as 4 PPQN (4 pulses per quarter note).

## Randomization

Randomization is an experimental new feature that combines random voltages with
an LFO. If you turn this parameter up, then for each "hill" of the output
waveform has a different height. The parameter `randomize` controls how strong
that effect is. With 0.0 randomization is turned off. At 1.0 it is at its
strongest and the random level of each hill is in the range 0.0 … 1.0.

Here is an example of a randomized sine wave:

```droid
[lfo]
    hz        = 0.5
    randomize = 0.8
    sine      = O1
```

The original wave is printed light and the randomized wave at output `O1` is
magenta.

Please note: If you turn `bipolar` on, then a "hill" is considered to be
something *above* or *below* the zero line. That means that now the sine wave has
twice as much hills and the randomization works different. Here is an example
patch:

```droid
[lfo]
    hz        = 0.5
    randomize = 0.8
    sine      = O1
    bipolar   = 1
```

Note: Since not all waveform have there "hills" at the same place and the start
and end of a hill might even be affected by `skew` or `pulsewidth`, each waveform
output has its own independent randomization. Therefore `cosine` is *not* the
phase shifted output of `sine` anymore, if you use randomization.

## Wave form selection and morphing

As an alternative to the seven indiviual waveform outputs there is a common
output simply called `output`. The waveform can be selected with the input
`waveform` and defaults to 0, which means *square wave*. So for a simple clock
you can write:

```droid
[lfo]
    hz = 2
    output = G1
```

A triangle wave is selected with the code 2:

```droid
[lfo]
    hz = 2
    output = G1
    waveform = 2
```

Here is the complete list of available waveforms:

| Code | Waveform |
|------|----------|
| 0 | square |
| 1 | sawtooth |
| 2 | triangle |
| 3 | ramp |
| 4 | paraboloid |
| 5 | sine |
| 6 | cosine |

It is allowed to use non-integer values, like 0.5. This will create a mixture
between two adjacent waveforms – while respecting the ratio. For example 2.1 will
select 90% triangle and 10% ramp. That way you can smoothly morph through the
available waveforms. Here is an example. Let's start with `waveform = 0.0`, which
gives a plain square wave:

```droid
[lfo]
    hz = 4
    output = O1
    waveform = 0.0
```

At 1.0 we get a saw tooth:

```droid
[lfo]
    hz = 4
    output = O1
    waveform = 1.0
```

And in between – at 0.5 – we get some mixture:

```droid
[lfo]
    hz = 4
    output = O1
    waveform = 0.5
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `rate` (`ra`) | CV | `0.0` | Frequency control: The default frequency of the LFO is 1 Hz (one cycle per second). Each volt doubles the frequency. So an input of 1 V (a number of 0.1) speeds up the LFO to 2 Hz, 2 V (0.2) create 4 Hz and so on. On the other hand negative voltages reduce the speed, so -1 V (-0.1) will give 0.5 Hz and so on. |
| `taptempo` (`tt`) | gate | | Feed a reference clock here and the LFO will run at the speed of that clock – albeit optionally modified by `rate` and `hz`. Please see the manual ([basics](../basics.md)) for details on using `taptempo` inputs. |
| `hz` () | CV | `1.0` | Set the frequency in Hz directly by setting a number here. Note: you cannot use `hz` at that same time as `taptempo`. But both can be combined with `rate`. |
| `level` (`l`) | CV | `1.0` | The maximum positive output level of the LFO. The default of 1.0 means a swing between 0 V and 10 V – unless you enable `bipolar`, in which case it moves from -10 V to 10 V. |
| `randomize` (`r`) | `0..1` | `0.0` | Randomization is an experimental new feature that combines random voltages with an LFO. If you turn this parameter up, then for each hill of the LFO's waveform output a new random attenuation is being chosen and multiplied with the current level. The result is an output, where each cycle of the waveform has a different level. |
| `offset` (`of`) | CV | `0.0` | The output of the LFO is shifted by that voltage right before the output. This is the same as adding or mixing a fixed voltage to the output. Not very fancy, but practical if you want to output a modulation voltage within a certain range. |
| `bipolar` (`b`) | gate | `0` | If this switch is set to on, then the LFO will output a full swing from `-level` to `+level`. When set to off it will swing between 0 V and `+level`. |
| `phase` (`p`) | `0..1` | `0.0` | Shift the LFOs phase by this value. A value of 0.0 leaves the LFO run in its normal phase. 0.5 will shift bei 180°. And 1.0 will shift by a complete phase of 360°, which is the same as 0.0. |
| `pulsewidth` (`pw`) | `0..1 (mid 0.5)` | `0.5` | This sets the pulse width of the square LFO and only affects the output `square`. It ranges from 0.0 to 1.0. Please note that a pulse width of exactly 0.0 or 1.0 will make the output stick to the respective lower or upper level. |
| `skew` (`sk`) | `0..1 (mid 0.5)` | `0.5` | Modifies the symmetry of the triangle output by shifting the "peak" of the triangle left and right. The default of 0.5 creates a symmetric waveform. Smaller values speed up the rising part of the triangle and create more and more a ramp like waveform until a skew of 0.0 creates an exact ramp – just the same as the `ramp` output. A skew of 1.0 create a sawtooth waveform. |
| `sync` (`sy`) | trigger | | A positive trigger edge at this input will reset the LFO. It will force to restart the waveform at its "beginning". By using the input `syncphase` you can change that behaviour. |
| `syncphase` (`sp`) | `0..1` | `0.0` | This input changes the behaviour of the `sync` input. I changes the phase the waveform restarts at when it receives a sync trigger. E.g. by setting this to 0.5 a sync trigger will restart the waveform right at its middle. This is an interesting feature that cannot be found in analog LFOs since it would be very hard to build in actual circuits. |
| `waveform` (`w`) | CV | `0.0` | If you use `output` – rather than the individual waveform outputs like square, saw and so on – this input selects the Wave form. An integer number from 0 to 6 selects one of the seven available waveforms. Any number in between selects a mixture of the two neighboring waveforms. That way you can smoothly morph through all the available waveforms. The codes for the waveforms are: 0 square, 1 sawtooth, 2 triangle, 3 ramp, 4 paraboloid, 5 sine, 6 cosine. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | Main output of the LFO. |
| `square` (`q`) | CV | A square waveform – modified by `pulsewidth`. |
| `sawtooth` (`st`) | CV | Outputs a sawtooth waveform – i.e. a rising ramp. |
| `triangle` (`t`) | CV | Outputs a triangle waveform – modified by `skew`. |
| `ramp` (`rp`) | CV | Outputs a falling ramp – like a sawtooth that is mirrored. Note: if the LFO is set to bipolar then this is the negation of `sawtooth`. If it is set to unipolar then this is not the case. The waveform will be positive then! |
| `paraboloid` (`pb`) | CV | An experimental waveform that looks very similar to a sine wave but is derived from a triangle by computing the square of each waypoint's distance to `level`. |
| `sine` (`si`) | CV | A sine waveform. |
| `cosine` (`cs`) | CV | A sine waveform shifted by 90°. This output is for your convenience and avoids needing two LFO circuits in cases where you want to make quadrature applications. Please note that 180° and 270° can easily be achieved by negating the outputs `sine` and `cosine` at a later stage. |
