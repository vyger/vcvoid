---
circuit: calibrator
title: VCO Calibrator
obsolete: false
ram_bytes: 232
manual_pages: [164, 165, 166, 167]
category: quantizer-pitch
tags: [vco, calibration, tuning, tracking, pitch, correction, octave, nudge, semitone, v-oct]
see_also: [encoder, minifonion, vcotuner, outputcalibrator]
impl_difficulty: hard
controller_binding: controller-enhanced
verification: rack-automated
spec_gap: true
difficulty_note: "Per-octave pitch correction runs master-only from tune0..tune8, but manual nudging, tail extrapolation and inter-octave interpolation add stateful complexity."
verification_note: "Rack-automated: patch a Rack VCO and measure tracking, or drive fixed tune inputs and read output. Spec-gap — the interpolation between per-octave correction points and the low/high tail behaviour are not precisely documented and need hardware reference."
---

# calibrator — VCO Calibrator

This circuit allows you to precisely compensate for decalibrated or otherwise
imperfectly tracking VCOs – which is probably a property of all existing analog
VCOs to some degree. It does this by applying one specific adaptation value per
individual octave. This way you can make even those VCO track well over 10
octaves, that would normally only do 2 or 3.

The calibration of the error compensation is done manually – by you. At first
this may seem like a disadvantage. In practice, however, this is much easier and
more accurate than the way some "autotune" modules do it. Those modules have an
additional input for "listening" to a waveform output of the oscillator and
measure and adjust the tracking at a button press.

The advantages of manual tuning are:

- You don't need an extra waveform output of your VCO.
- You can calibrate sound sources with complex wave forms, whose pitch is hard
  to grab by autotune devices.
- You can change the correction at any time during a live performance without
  your audience noticing.
- It's possible to make one VCO follow the (imperfect) tracking of a second one,
  in order to create perfect FM sounds while just one VCO needs to be adapted.
- It's also possible to fix the tracking of unprecise pitch CV *generators*, such
  as sequencers, quantizers or MIDI interfaces.

The calibrator circuit happily profits from the DROID's highly precise, linear
and low-jitter ADCs and DACs. And using eight such circuits one DROID could fix
the tuning of up to eight VCOs.

## How to use

Here is a typical patch for the use of the calibrator:

```droid
[calibrator]
    input     = I1
    output    = O1
    nudgeup   = B1.1
    nudgedown = B1.3
    ledup     = L1.1
    leddown   = L1.3
```

The original pitch information from the sequencer, quantizer, MIDI converter or
whatever comes into `I1`. The adapted pitch goes to `O1` and from there to the
V/Oct input of your VCO. Of course the pitch information could also come from
some internal circuit like the [`minifonion`](minifonion.md). In that case
`input` is connected to an internal patch cable coming from that circuit.

Now with the two buttons `B1.1` and `B1.3` you can adjust the tuning up and down
at any time while playing. Each button press just very slightly shifts the pitch
up or down. The adjustment is only done for the octave that's currently playing.
`calibrator` saves one calibration value for each octave from 0 to 8 and also one
for the pitches below 0 V and those above 8 V. Your tuning profile is
automatically saved to the memory card.

Pressing both buttons at the same time resets the calibration of the current
octave.

For a good result I suggest either using a precise tuner or playing the voice at
the same time as a reference voice and try to minimize the audible beatings.

A second way of using the VCO calibrator is specifying a tuning adjustment for
each octave by a fixed number (or a potentiometer if you can afford). This is
done with the inputs `tune0 … tune8` and `tunelowtail` and `tunehightail`. A
value of 1.0 means an upwards tuning of one semitone (100 cents) per octave, and
−1.0 likewise downwards.

## Persistence

As always, the internal state of the `calibrator` circuit is automatically saved
to your SD card and loaded when your DROID starts.

But what if you are using several calibrators, each for a different (and
differently tracking) VCO? How do you know which of the saved calibration states
is applied to which VCO?

The answer to this is: all calibrators in your patch are enumerated starting from
1. For each of them there is one configuration saved to the SD card, based on
that number. So when you modify the calibration of the third `calibrator` circuit
in your patch, the modified configuration will be saved as belonging to
calibrator number 3.

So if you make sure that each VCO is always handled by the same `calibrator`
circuit you will always get the right configuration.

If you for example remove the first calibrator from your patch, the second one
will become the new first one and load its calibration state when you load the
new patch. If you don't want that to happen, simply keep the calibrator in the
patch, even if you don't need it anymore. It is sufficient to keep just the line
`[calibrator]` without any further jack specifications.

## Using an encoder instead of buttons

If you own an E4 controller, you can use one of its encoders for the tuning
correction, instead of buttons. This is not only faster and easier to operate
but also gives you visual feedback about the current correction in the LED ring
of the E4.

To do that add an [`encoder`](encoder.md) circuit. The trick is to use the
encoder's `movedup` and `moveddown` triggers and feed them into the `nudgeup` and
`nudgedown` inputs. The calibrator's `correction` output informs you about the
current correction and can be used as an input for the `override` parameter of
the encoder. If you use just tiny corrections, you can amplify the display (zoom
in) by multiplying the value say by 2.

The following example shows you how to set up this. Here in addition the
encoder's button is used for resetting the correction of the current pitch (not
the total one):

```droid
[calibrator]
    input = I1
    output = O4
    nudgeup = _UP
    nudgedown = _DOWN
    correction = _CORRECTION
    clearhere = _CLEARHERE
    nudgeamount = 0.01

[encoder]
    encoder = 4
    movedup = _UP
    moveddown = _DOWN
    override = _CORRECTION * 2
    button = _CLEARHERE
    mode = 2 # make it bipolar
    color = 0.4 # green
    negativecolor = 0.8 # red
```

## Display

If you have a DB8E controller (see the manual, [hardware](../hardware.md))
attached and configured, its display will automatically show the current
correction curve of any `calibrator` circuit as soon as you trigger `nudgeup` or
`nudgedown`. Here is an example screen:

The small numbers at the bottom indicate the octaves of the input pitch, where
for example a 2 stands for 2 Volts. The little cross in the middle shows the
current input pitch (in the example exactly 2 V) and the position of a correction
of zero. In the example the curve is way below this, indicating a large downwards
pitch correction.

The line left of 1 V is dotted. This means that in this range there is no
correction – it's exactly zero.

If you start from scratch or have cleared all corrections, the graph looks flat
(all corrections zero).

Usually this plot is only shown when you nudge the correction up or down. By
setting `forcedisplay = 1`, the circuit will always bring up the display. You can
use this to monitor how the input pitch is corrected while some melody is being
played or so.

As always, if you use `select`, the circuit must be selected in order for the
display to be used.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | pitch | `0V` | Patch your V/Oct pitch input here. |
| `nudgeup` (`nu`) | trigger | | A trigger here (most likely a button press) will modify the tuning of the currently played note (as read by `input`) upwards by one cent (or by `nudgeamount` if that is used). |
| `nudgedown` (`nd`) | trigger | | A trigger here will modify the tuning of the currently played note down. |
| `clearhere` (`ch`) | trigger | | A trigger here sets the correction of the currently played note to zero. This might affect a range of up to two octaves. |
| `nudgeamount` (`na`) | CV | `0.01` | Changes the amount each button press detunes. A value of one would mean one semitone, so the default value of 0.01 corresponds to one cent (1⁄100) of a semitone. |
| `forcedisplay` (`fd`) | gate | | If you set this to `1`, the display of a DB8E will *always* show the current tuning correction, even if you don't nudge up or down (still the circuit needs to be selected for the display to get active). |
| `tune0 … tune8` (`t`) | CV | `0.0` | Explicit tuning of the octaves 0 through 8 – if you do not want to nudge manually. `tune0` sets the tuning for the input pitch of 0 V, `tune1` for 1 V and so on. A value of 1 means a tune adjustment of one semitone – which is 100 cent. The maximum detuning is ± 1 Octave (at a value of ±12). |
| `tunelowtail` (`tl`) | CV | `0.0` | Tuning adaption for the negative voltage range. A value of 1 means an upwards tuning of one semitone *per octave*, −1 likewise downwards. |
| `tunehightail` (`th`) | CV | `0.0` | Tuning adaption for voltages > 8 V. A value of 1 means an upwards tuning of one semitone *per octave*, −1 likewise downwards. |
| `display` (`dy`) | integer | `1` | This parameter selects the number of the DB8E display controller, where any change of this circuit's state will show up. Per default, the first DB8E available is used. Set this to `2` if you have a second DB8E and want to use that. Note: any other controller is not being counted here, it's really just the number of the DB8E as counted from the master module. Set this to `0` to suppress any display output from this circuit. |
| `header` (`hr`) | text | `☞ smart` | This text is displayed at the top of the DB8E display whenever this circuit shows up in the display. If you omit this, an automatic title is used. For example if the output of this circuit is fed into an internal patch cable, the name of this cable is displayed. Setting `header = ""` or `header = 0` removes the header and just displays the actual value. |
| `select` (`s`) | integer | `☞ smart` | The select input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to `1`). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | `☞ smart` | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is `0`, the circuit will be active if `select` is exactly `0` instead of a positive gate signal. In some cases this is more convenient. |
| `preset` (`pr`) | integer | `☞ smart` | This is the preset number to save or to load. Note: the first preset has the number `0`, not `1`! For the whole story on presets please refer to the manual ([basics](../basics.md)). This circuit has 4 presets, so this number ranges from `0` to `3`. |
| `loadpreset` (`lp`) | trigger | | A trigger here loads a preset. As a speciality you can use the trigger for selecting a preset at the same time. |
| `savepreset` (`sp`) | trigger | | A trigger here saves a preset. |
| `clear` (`cl`) | trigger | | A trigger here loads the default start state into the circuit. The presets are not affected, unless you use direct preset switching with the `preset` input and without triggers. And in that case the current preset is also cleared. |
| `clearall` (`ca`) | trigger | | A trigger here loads the default start state into the circuit and into all of its presets. |
| `dontsave` (`dos`) | gate | `0` | If you set this to `1`, the state of the circuit will not be saved to the SD card and not loaded from the SD card when the Droid starts. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | pitch | The calibrated pitch goes out here. |
| `ledup` (`lu`) | 0..1 | When `nudgeup` is mapped to a button (which is most likely), map this output to the according LED and it will indicate whenever it's currently adjusting the output pitch upwards. |
| `leddown` (`ld`) | 0..1 | This is the LED for `nudgedown`, which indicates downwards adjustment. |
| `correction` (`c`) | CV | This output gives you information about the current amount of pitch correction. It is positive if the pitch is corrected upwards, else negative. It is scaled in semitones, so a value of `0.2` means 20% of a semitone, which is the same as 20 cents. |

## See also

- [`encoder`](encoder.md) — use an E4 encoder for tuning correction with LED-ring feedback.
- [`minifonion`](minifonion.md) — a common internal pitch source feeding the calibrator's `input`.
