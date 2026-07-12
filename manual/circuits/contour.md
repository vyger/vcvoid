---
circuit: contour
title: Contour generator
obsolete: false
ram_bytes: 112
manual_pages: [183, 184, 185, 186, 187]
category: envelope-lfo
tags: [envelope, adsr, contour, attack, decay, sustain, release, predelay, hold, swell, shape, exponential, logarithmic, velocity, loop, taptempo, zerocrossing]
see_also: [lfo]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: true
difficulty_note: Deterministic six-phase envelope with a lot of state (phase machine, retrigger/startfromzero/abortattack, loop, swell, zerocrossing) but no controller dependency.
verification_note: "Headless: drive gate/trigger sequences and sample the output over time against computed reference envelopes. Partial — exact bent shape curves (attackshape/decayshape/releaseshape 0.0-1.0) and the swell curve are described qualitatively only and should be spot-checked against a hardware reference."
---

# contour — Contour generator

An enhanced version of the classic ADSR-envelope generator with the six phases
predelay, attack, hold, decay, sustain and release.

For triggering there are two alternative inputs: `gate` and `trigger`. Use
`trigger` if you are not interested in the length of the gate signal. There will
be no decay / sustain phase in that case.

The minimal patch just connects `gate` or `trigger` and the output. It creates
an envelope with standard timings, triggered at `I1` and output to `O1`:

```droid
[contour]
    gate   = I1
    output = O1
```

Assigning pots to the classic four inputs lets you use the DROID just as a normal
ADSR envelope:

```droid
[p2b8]
[p2b8]

[contour]
    gate    = I1
    attack  = P1.1
    decay   = P1.2
    sustain = P2.1
    release = P2.2
    output  = O1
```

When you try this out, you will notice that the time range of the `attack`
parameter is much shorter than that of `decay` and `release`. In fact it is just
1⁄20 of these. This has been chosen in this way because I believe that this makes
sense from a musical point of view. Very long attack times are quite unusual and
I wanted to be able to directly map the four values to pots. But if you don't
like that you can – of course – make all three timing parameters have the same
range simply by multiplying `attack` by 20:

```droid
[p2b8]
[p2b8]

[contour]
    gate    = I1
    attack  = P1.1 * 20
    decay   = P1.2
    sustain = P2.1
    release = P2.2
    output  = O1
```

If you do not change the `shape` parameter, the duration of the attack phase is
0.1 sec at a value of 1. The phases decay and release have a duration of 2.0 sec
at a value of 1.

## The Phases

In addition to the traditional ADSR phases this circuit also has an optional
predelay (P) phase – which acts like a delay before the envelope starts – and an
optional hold (H) phase which keeps the envelope at maximum level for a short
time right after attack and before decay.

The following diagram shows an example envelope with all six phases. The gate
starts at 0 ms and ends at 200 ms.

## Attack, Decay and Release

The phases attack, decay, release are phases where the level of the envelope
starts at one level and then approaches another level within a certain time. In
the upper example all these phases had a *linear* characteristic. That means that
the output voltage changes by a constant amount per time.

DROID's `contour` allows you to control the shape of these phases in order to get
them *bent* in either direction. For that purpose there are the inputs
`attackshape`, `decayshape` and `releaseshape`.

Let's take decay as an example. During the decay phase the envelopes voltage
falls from the maximum level of 10 V (you can change this with the input `level`)
to the sustain level defined by the input `sustain`. For simplicity let's assume
that you have not used these inputs, so the maximum level is 10 V (1.0) and the
sustain level is 5 V (0.5). Also we assume attack, predelay and hold to be 0.0.

When `decayshape` is not patched or otherwise set to its default of 0.5, the
shape of the decay curve is *linear*. This means that it goes down by the same
voltage each second until it reaches 0.5.

Now, if you set `decayshape` to 1.0, the curve is completely *exponential*.

Such an envelope sounds completely different – of course also depending on
whether you feed this into a linear VCA, exponential VCA or a VCF. For fine
control you can use any number between 1.0 and 0.5 of course. In that case you
will get a curve that is bent to a certain degree. Assigning `decayshape` to a
pot helps you *listening* to the different sounds:

```droid
[contour]
    gate               = I1
    decayshape         = P1.1
    output             = O1
```

If the shape gets a value less than 0.5, the curve is bent into the opposite
direction (some call this *logarithmic* but mathematically this is not true).
Here is an example where `decayshape` is set to 0.0.

The effect of `shape` or `attackshape` on the attack phase is similar. A shape of
1.0 means that at the beginning the movement is very fast and at the end of the
phase it becomes slower and slower.

Here is what the attack looks like with an exponential shape. And
`attackshape = 0.0` looks similar too. So in the graphical representation the
curve of the attack phase is bent "in the other direction", but still the rule
holds that an exponential shape creates a movement that is fast at the beginning
and gets slower the more the level reaches its target level.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `gate` (`g`) | gate | ☞ smart | Patch a gate signal here that triggers the envelope. Gate means that the length of the signal is relevant. While the gate is high the sustain phase holds on. As soon as gate is going low the release phase is being entered. |
| `trigger` (`t`) | trigger | | This is an alternative method of starting the envelope. If you use `trigger` instead of `gate`, there are the following differences: • The duration of the trigger signal is being ignored. • There is no decay / sustain phase. Attack and hold are immediately followed by release. The inputs `sustain` and `decay` have no impact anymore. • The predelay and attack phases are continued until their end even when the trigger signal ends (When using `gate` and the gate signal ends during predelay, the envelope does not start. When it ends during attack, decay / sustain are being skipped and release starts at the current level of the envelope. That way short gates can result in "quieter" envelopes). |
| `retrigger` (`rt`) | gate | `1` | If you patch 0 or off here, a gate or trigger impulse will **not** immediately restart the envelope unless it already has reached its release phase. The default `on`, which means that a trigger will immediately restart the envelope in any case. |
| `startfromzero` (`sz`) | gate | `0` | If you set this to 1 or on, a trigger or gate will reset the envelope's current level immediately to zero. This is sometimes called "digital mode". In the normal analog mode the envelope resumes from where it is. This means that when a trigger occurs right in the release phase where the level is still high, will start its attack not from zero but from this high value. |
| `abortattack` (`aa`) | gate | `0` | This is an on / off setting that decides what happens if the input `gate` goes off while the predelay or attack phase is still not finished. Per default that phase will be finalized regardless of the gate state. If `abortattack` is on, the end of the gate will immediately stop the attack phase and move on to hold. Note: In this case the value of the envelope will not reach the maximum level. If the gate ends during the predelay phase, no envelope will be started at all. Note: This setting is only functional when the `gate` input is being used for triggering the envelope. If you use `trigger`, the attack phase is always completely executed and this setting has no influence. |
| `loop` (`lo`) | gate | `0` | This is an on / off input that switches loop on or off. When loop is on, the envelope will immediately start again once it has finished. It also starts without triggering. This converts contour into a kind of fancy LFO. `gate` / `trigger` and `loop` can be combined. Any gate or trigger will restart the envelope just as usual – even in loop mode. |
| `predelay` (`pd`) | CV | `0.0` | The predelay phase inserts a delay between the incoming gate and the begin of the envelope. The length of the predelay is 0.1 seconds per volt, so a value of `1.0` means 1 second. |
| `attack` (`a`) | CV | `0.0` | Length of the attack phase, i.e. the time from the beginning of the gate until the maximum `level` is reached. See the general description for information about the scaling of this input. |
| `hold` (`h`) | CV | `0.0` | If this is none-zero, the envelope lingers a certain amount of time at its maximum level after the attack and before the decay phase. The input value specifies a number of seconds. A value of `0.5` (this is 5 V) will create a hold time of 0.5 seconds. |
| `decay` (`d`) | CV | `0.2` | Time of the decay phase. |
| `sustain` (`s`) | `0..1` | `0.5` | Sustain level. |
| `swell` (`sw`) | `0..1` | `0.0` | If this jack is set to a value greater than 0.0, the level of the envelope will go up or down again during the sustain phase until it reaches `swelllevel`. |
| `swelltime` (`st`) | CV | `5.0` | Time of the swell phase. |
| `swelllevel` (`sl`) | CV | `1.0` | Level the swell phase is approaching. Setting this to the same as `sustain` effectively disables swell. |
| `release` (`r`) | CV | `0.2` | Timing of the release phase. |
| `level` (`l`) | CV | `1.0` | Maximum level and scaling of the envelope. It is basically an output attenuator of the envelope. Sudden changes in the level will immediately have an (audible) impact on the envelope. |
| `velocity` (`v`) | `0..1` | `1.0` | *energy* of the attack: The velocity is similar to the `level`, but is effective just during the attack phase. During that phase that maximum voltage that is read from the `velocity` jack and will be used as the velocity of the envelope. Further changes during the other phases will be ignored. This makes it ideal of using with a sequencer. For example you can patch an `accent` output here and add some offset. Sudden changes in this input will not affect the shape of the envelope. |
| `pitch` (`p`) | pitch | `0V` | This is a one volt per octave input affecting all timings of the envelope except the attack phase. When you set this to `0` (the default), it is neutral. A value of `0.1` (1 Volt) will exactly double the speed of all phases - just as one octave up doubles the frequency of an oscillator. This jack can be used to easily implement envelopes where the length very naturally follows this pitch - just like on a piano, glockenspiel or marimba where lower notes last longer than higher ones. |
| `taptempo` (`tt`) | trigger | | Tap tempo is an alternative method of specifying a pitch information. When you patch a clock to tap tempo, all time parameters in the envelope are relative to that clock. If the clock speeds up, the envelope gets faster and vice versa. The reference speed is 120 BPM. This means that if you patch a 120 BPM clock here, nothing changes. Clocks faster than 120 BPM will speed up the envelope. Clocks slower than 120 BPM will slow it down. Please see the manual ([basics](../basics.md)) for details on using taptempo inputs. |
| `shape` (`sh`) | `0..1 (mid 0.5)` | `0.5` | If you use this jack, it sets the shape for all of the relevant phases, which are attack, decay, swell and release. Note: this input is only effective for those phases where the dedicated input (like `attackshape`, etc.) is not being used. This value ranges from 0.0 (logarithmic), via 0.5 (linear) to 1.0 (exponential). |
| `attackshape` (`as`) | `0..1 (mid 0.5)` | ☞ smart | Shape of the attack curve. If nothing is patched here, the value of `shape` will be used. See the general description for how curve shapes work. This value ranges from 0.0 (logarithmic), via 0.5 (linear) to 1.0 (exponential). |
| `decayshape` (`ds`) | `0..1 (mid 0.5)` | ☞ smart | Shape of the curve in the decay phase. If nothing is patched here, the value of `shape` will be used. This value ranges from 0.0 (logarithmic), via 0.5 (linear) to 1.0 (exponential). |
| `swellshape` (`ss`) | `0..1 (mid 0.5)` | ☞ smart | Shape of curve during the swell phase. If nothing is patched here, the value of `shape` will be used. |
| `releaseshape` (`rs`) | `0..1 (mid 0.5)` | ☞ smart | Shape of the curve in the release phase. If nothing is patched here, the value of `shape` will be used. This value ranges from 0.0 (logarithmic), via 0.5 (linear) to 1.0 (exponential). |
| `zerocrossing` (`z`) | CV | ☞ smart | This is an experimental feature: If you patch the output of an oscillator here, an incoming gate or trigger signal will be delayed until the next zero crossing of that signal. That allows you to start the envelope exactly when the audio signal is at 0 and avoid nasty klicks, even if the attack is set to 0. It comes at a price, however. The delay between the trigger and the first zero crossing might vary a lot from note to note and that could make your rhythm untight, especially if the frequency of the oscillator is low. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | Main output of the envelope. Patch this to your filter, VCA or wherever you like. |
| `negated` (`n`) | CV | The negated output is the same as the output but in negative voltage. |
| `inverted` (`iv`) | CV | The inverted output always outputs *positive* voltages but is inverted relative to the level of the envelope. When the normal `output` outputs 0 V, the inverted output outputs `level` and vice versa. |
| `endofpredelay` (`ep`) | trigger | This output will emit a trigger with a length of 10 ms when the predelay phase has ended. |
| `endofattack` (`ea`) | trigger | This output will emit a trigger with a length of 10 ms when the attack phase has ended. |
| `endofhold` (`eh`) | trigger | This output will emit a trigger with a length of 10 ms when the hold phase has ended. |
| `endofdecay` (`ed`) | trigger | This output will emit a trigger with a length of 10 ms when the decay phase has ended. |
| `endofrelease` (`er`) | trigger | This output will emit a trigger with a length of 10 ms when the release phase has ended. |
