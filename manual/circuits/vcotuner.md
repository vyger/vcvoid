---
circuit: vcotuner
title: measure frequency and tuning of a VCO
obsolete: false
ram_bytes: 72
manual_pages: [407, 408, 409, 410]
category: conversion
tags: [tuner, vco, frequency, pitch, tuning, cents, master18, db8e, pitch-follower, semitone]
see_also: [encoder]
impl_difficulty: moderate
controller_binding: controller-enhanced
verification: rack-automated
spec_gap: false
difficulty_note: Frequency probe (positive zero-crossing counting, 1 Hz-20 kHz) plus semitone comparator, cents/pitch/tuning outputs and smoothing; MASTER18-specific but the hz/pitch outputs work master-only, with optional P2B8/E4/DB8E display bindings.
verification_note: "Needs an actual oscillator in the rack to measure, so drive it Rack-automated: patch a VCO at a known 1V/oct pitch and read hz/pitch/cents/intune. The DB8E tuner screen is an optional display enhancement that would need a human to verify visually."
---

# vcotuner — measure frequency and tuning of a VCO

The MASTER18 has a builtin probe that can measure the frequency of incoming
signals with high precision. This circuit uses this probe to measure the pitch of
basic waveforms, such as triangle, since, sawtooth and square. With this you can
build a tuner and a pitch follower.

To use the circuit, plug any signal with a basic waveform in the `I1` input of
your MASTER18. This input can measure the frequency of *positive zero crossings*,
which is is the number of times the signal goes from below zero volt to above
zero volts, per second. For basic waveforms this is the same as the frequency of
the signal itself. The frequency range you can measure is in the range 1 Hz to
20 kHz. The low end allows you to measure clocks.

Notes:

- The tuner does not work well for complex waves, such as frequency modulated
  waves or the result of complex wave table synthesis.
- This circuit does not exist on the MASTER.

## Building a tuner

Included in the circuit is a comparator that checks how far the frequency is away
from the nearest semitone or a reference note that you choose. You can display
this deviation in various ways to build a tuning device.

The following patch example uses the three button-LEDs of a P2B8 to show the
current tuning:

```droid
[p2b8]

[vcotuner]
    ledsharp = L1.1
    ledintune = L1.3
    ledflat = L1.5
```

If you plug a signal into `I1`, the circuit shows you how good its frequency
matches that of the nearest semitone.

- When the frequency is close enough, LED 3 is lit.
- When the frequency is too low, LED 1 is lit.
- When the frequency is too high, LED 5 is lit.

Close enough here means that the measured note is within ± 3 cents of the nearest
semitone. 100 cents correlates to the pitch difference of one semitone, so the
largest possible distance to the nearest semitone is 50 cents. You can reduce this
to 2 cents with `precision = 2`, if you like.

There is no display for showing the semitone the tuner locked in. So if you are
not sure you got the right note, you can set a tuning note, like for example C.
This is done with the input `tuningnote`. A value of `0` selects C (C♯ is 1, D is
2 and so on). The following example tunes to the nearest C with a precision of 2
cents:

```droid
[vcotuner]
    ledsharp = L1.1
    ledintune = L1.3
    ledflat = L1.5
    tuningnote = 0 # any C
    precision = 2 # 2 cents max deviation
```

If you are lucky enough to own an E4 controller (see the manual
([hardware](../hardware.md))), you can use the LED ring of an encoder to display
the current tuning. Here is an example:

```droid
[e4]

[vcotuner]
    tuningnote = 0
    cents = _CENTS
    ledintune = L1.1

[encoder]
    encoder = 1
    override = _CENTS / 50
    rangemode = 1 # bipolar
    negativecolor = 0.8 # red
```

The encoder-LEDs can display a value in the range -1 … 1. You might change the 50
in `override = _CENTS / 50` if you want the display to be more "zoomed in" or
"zoomed out".

## Pitch follower

The `vcotuner` can give you the measured frequency as a 1V/Octave value at the
output `pitch`. You can then patch this output to a second VCO, to make it track
the pitch of the first one:

```droid
[vcotuner]
    pitch = O1 # patch to 2nd VCO
```

Hereby it is assumed that the frequency of A1 is 440 Hz and 0 V corresponds to a
C0. You can change both with the settings `concertpitch` and `basepitch` (see
below).

## Display

If you have a DB8E display controller (see the manual
([hardware](../hardware.md))), its display automatically switches to typical
tuner screen, as soon as `vcotuner` detects a valid input signal.

The pointer gets thick, when the input VCO is "in tune". This is the case whenever
the tuning is within the range -3.0 … +3.0 cents (you can change this with the
input `precision`). You also see the reference note (here G♯4).

The amount of detuning in cents is shown at the left or right, depending wether
it's negative or positive.

The display also shows the frequency in Hz.

If the tuner looses its signal – for example if you unplug the cable from the VCO
– this display shows "no signal". This means that there is no signal that can be
measured by the frequency probe of your MASTER18.

Notes on using the display:

- The tuning display automatically pops up when the MASTER18 module detects a
  valid input signal on `I1` and there is `vcotuner` circuit in your patch. If you
  don't want this, you can set `display = 0`. This will disable the display. Also
  the display does not react if you use the `select` input and the circuit is not
  selected.
- The tuning display does not display a header, because we consider this useless
  and rather wanted to use the screen space for the tuning information. The input
  parameter `header` is ignored. Even if the list of inputs below says otherwise.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `tuningnote` (`tn`) | integer | `-1` | You can either tune to a specific note, like C or A or whatever you like. In this case set `tuningnote` to a number from 0 to 11, where 0 is C, 1 is C#, 2 is D and so on. Or you set it to `-1` (which is the default) to tune to the nearest semitone. |
| `concertpitch` (`cp`) | CV | `440.0` | Set the frequency of the *concert pitch* here, which is the frequency of A1. Most commonly this is 440 Hz, so `440` is the default value of this parameter. |
| `basepitch` (`b`) | pitch | `0V` | This output sets the reference for the `pitch` output. It is the voltage for the note C0. With analog synths it is common – but not neccessary – that the C notes correspond to integer voltage numbers. |
| `smooth` (`sm`) | 0..1 | `0.5` | When measuring the input frequency, a slight smoothing is applied (like a low pass filter). It evens out some possible jitter that you might have. You can adjust this filter here. Per default smoothing is just subtle. Beware: `smooth = 1` smoothes strong, so the frequency measurement needs some time to get accurate. |
| `precision` (`pc`) | CV | `3.0` | This parameter defines the precision for the input frequency to be in tune. It is set in cents, where 100 cent correspond to one semitone. If you set the precision too low, it is very hard to tune correctly. If you set it too high, your tuning is not very precise. |
| `display` (`dy`) | integer | `1` | This parameter selects the number of the DB8E display controller, where any change of this circuit's state will show up. Per default, the first DB8E available is used. Set this to `2` if you have a second DB8E and want to use that. Note: any other controller is not being counted here, it's really just the number of the DB8E as counted from the master module. Set this to `0` to suppress any display output from this circuit. |
| `header` (`hr`) | text | ☞ smart | This text is displayed at the top of the DB8E display whenever this circuit shows up in the display. If you omit this, an automatic title is used. For example if the output of this circuit is fed into an internal patch cable, the name of this cable is displayed. Setting `header = ""` or `header = 0` removes the header and just displays the actual value. |
| `select` (`s`) | integer | ☞ smart | The `select` input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to `1`). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | ☞ smart | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is `0`, the circuit will be active if `select` is *exactly* `0` instead of a positive gate signal. In some cases this is more conventient. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `hz` () | CV | Outputs the current input frequency. If you no signal is found, the output is `0`. |
| `ledflat` (`lf`) | 0..1 | Patch this to a LED to show if and how much the input frequency is flat (too low). This value ranges from `0.2` to `1.0` if the input frequency is flat and goes straight to `0` otherwise. If you use `select`, this output only works while the circuit is selected. |
| `ledsharp` (`ls`) | 0..1 | Patch this to a LED to show if and how much the input frequency is sharp (too high). This value ranges from `0.2` to `1.0` if the input frequency is sharp and goes straight to `0` otherwise. If you use `select`, this output only works while the circuit is selected. |
| `ledintune` (`lt`) | gate | Outputs `1` if the input frequency is "in tune". Here the `precision` input is applied. If you use `select`, this output only works while the circuit is selected. |
| `intune` (`it`) | gate | Outputs `1` if the input frequency is "in tune". Here the `precision` input is applied. This output also while while the circuit is not selected. You can use it in situations where you want to process this information by some other circuit rather than just displaying it. |
| `tuning` (`t`) | 0..1 (mid 0.5) | Outputs the current tuning deviation of the input measured in 1V/octave. |
| `cents` (`c`) | 0..1 (mid 0.5) | Outputs the current tuning deviation in cents, where 100 cents correspond to one semitone. |
| `pitch` (`p`) | pitch | Outputs the current pitch of the input frequency in terms of 1V/octave. Here the `basepitch` is applied. You can patch this output to the pitch input of a second VCO to have that follow the pitch of the measured VCO. |
| `referencepitch` (`rp`) | pitch | Here you get the pitch of the reference tone the tuner currently "locked in" into. It is either the nearest semitone or the note selected by `tuningnote` in the nearest octave. |
| `vcofound` (`vf`) | gate | This output is `1` whenever a valid input signal is found on the tuning input `I1`. |
