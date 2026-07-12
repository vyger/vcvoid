---
circuit: slew
title: Slew limiter
obsolete: false
ram_bytes: 48
manual_pages: [388, 389]
category: utility
tags: [slew, glide, portamento, exponential, linear, s-curve, slew-limiter, cv]
see_also: []
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: true
difficulty_note: Per-sample slew limiter with three shapes; linear/gate/up-down logic is straightforward DSP.
verification_note: "Linear mode has an exact spec (120 V/s at pot 0.5) verifiable headless. Exponential and S-curve are only 'tuned to sound similar' with no stated time constant, so faithful curves need a reference trace or hardware capture."
---

# slew — Slew limiter

This is a CV controllable slew limiter for CVs. Special about it is that it
implements three alternative algorithms. The traditional exponential algorithm
(as is commonly implemented in analog circuits), a linear algorithm and a
special S-shaped curve.

Here is a simple example for a slew limiting on `I1` → `O1` which is controlled
with the pot `P1.1`:

```droid
[slew]
    input       = I1
    slew        = P1.1
    exponential = O1
```

## Exponential shape

This is the "classical" slew limit shape, which originates from the (negative)
exponential loading current of a capacitor. It is also the shape of a low pass
filter that is used for slew limiting. The slope is proportional to the distance
between the current and the target voltage. Or in other words the voltage changes
fast at the beginning and slower at the end.

## Linear shape

The *linear* algorithm simply limits the voltage change per time to a certain
change rate, e.g. to 10 V per second. If the input voltage changes faster (for
example suddenly jumps up), the output voltage follows that with that maximum
rate. At a pot position of 0.5 the maximum slew is 120 V per second.

## S-Curve shape

The S-curve — when applied to pitches — sounds different than an exponential
curve since it more reflects the way e.g. a trombone player accelerates and
deaccelerates his arm in order to move to another pitch. In our algorithm we
assume that in the first half of the time the arm accelerates at a constant rate
(which is controlled by the `slew` parameter) and at the second half of the time
it deaccelerates (again at that rate, just negative), until it exactly reaches
the target pitch.

There is one audible difference to a real trombone player, however. The real
musician would start to move his arm *before* the new note begins, in order to be
at the target position right in time. But here the movement is initiated by the
pitch change it self so it is delayed by the slew limiting.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | CV | | Wire the CV that you wish to slew limit here. |
| `slew` (`sw`) | CV | `1.0` | This controls the slew rate. A value of `0.0` disables slew limiting. The output immediately follows the input without any delay. A value of for example `2.0` in linear mode means that 2.0 seconds are needed for a change of 1 V (which is a value of 0.1 or one octave if used as pitch). In the other two modes the slew time is tuned to sound similar. Negative values of this parameter are treated as `0.0`. |
| `slewup` (`u`) | CV | `1.0` | This allows a special handling when the voltage moves *upwards*. The slew limiting for upwards is `slew` multiplied with `slewup`. Since `slew` defaults to `1.0` you can just use `slewup` and `slewdown` if you want to control both directions separately. |
| `slewdown` (`d`) | CV | `1.0` | Sets the slew rate for downwards movement. |
| `gate` (`g`) | gate | ☞ smart | If this jack is patched, the slew limiting is only active while this gate is high. Otherwise it's like setting the `slew` parameter to zero. When left unpatched, slew limiting is always active. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `exponential` (`e`) | CV | Output for the resulting CV with the exponential (classical) slew algorithm applied. |
| `linear` (`l`) | CV | Output for linear slew limiting. |
| `scurve` (`c`) | CV | Output with the slew limitation according to the S-curve algorithm. |
