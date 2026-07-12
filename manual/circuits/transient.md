---
circuit: transient
title: Transient generator
obsolete: false
ram_bytes: 56
manual_pages: [402, 403]
category: envelope-lfo
tags: [transient, ramp, slew, envelope, slow, loop, pingpong, phase, clock]
see_also: []
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Linear ramp with slope recomputed from remaining phase x duration; loop/pingpong/freeze/reset and seconds-or-clock-ticks duration are all specified, including in-flight parameter changes.
verification_note: "Deterministic output/phase/endoftransient; verify headless against a computed linear-slope reference, including the documented in-flight behaviour when start/end/duration change."
---

# transient — Transient generator

This circuit creates (possibly very slow) linear transients from a defined start
value to an end value. The duration of that transition is either set in seconds
or specified as a number of clock ticks. This circuit is built in a way that very
long transients are possible, even several days, weeks, months, years or whatever
you like.

## Example

Here is a simple example:

```droid
[transient]
    start = 1V
    end = 3V
    duration = 600
    output = O1
```

Here the duration is meant to be 600 seconds (10 minutes). So at the beginning
`O1` will be at 1 V. Then it rises slowly until after ten minutes it reaches 3 V.
There it stays forever.

There are two ways of restarting it again. Either you send a trigger to `reset`
or you set `loop` to `1`. When `loop` is active, the transient will start over at
`start` immediately when it reaches `end`:

```droid
[transient]
    start = 1V
    end = 3V
    duration = 600
    output = O1
    reset = G1
    loop = 1
```

As an alternative to seconds you can specify the length in terms of clock ticks.
This needs a steady clock signal patched into the `clock` input.

```droid
[transient]
    start = 0.2
    end = 0.7
    duration = 32
    clock = I1
    output = O1
```

Here the duration of one transient is exactly 32 clock ticks. This makes it
simpler to exactly align a transient with a musical structure of a song or the
like.

## Changes while in the air

As `start`, `end` and `duration` are CV inputs, they might change while the
transient is running. This is how `transient` behaves in such situations:

The `start` value is just taken into account whenever the transient starts. this
is:

- When the DROID starts
- When there is a trigger at `reset`
- When the transient reaches the end and `loop` is on.

Whenever that happens, the current output level is set to `start`. Also the
output `phase` is set to 0. Phase is a kind of internal clock that measures which
part of the transient has been run through already.

At any given time `transient` assumes that the *phase* times the duration equals
the time left. And the distance to go in the remaining time is the current
distance from the current output level to the end. These two values directly
translate into a slope. This slope now determines how fast the output level is
moving and into which direction.

From this follows:

- When you make the duration longer in-flight, the speed of change will get
  slower.
- When you change `start` in-flight, nothing happens.
- When you change `end` in-flight to a value that is "farther" away from the
  current level, the speed of change increases.
- If you change `end` to be the current level of the transient, it seems to stop,
  but in fact the slope is just zero and it still lasts until the duration is
  over.
- The output level is always smooth. No sudden steps. With one exception: When
  the transient resets to its start value.

In pingpong mode (see the table of inputs for details) this changes accordingly.
While the transient is on its way back, consider `start` and `end` exchanged.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `start` (`st`) | CV | `0.0` | Start value of the transient. |
| `end` (`e`) | CV | `1.0` | Target value of the transient. |
| `duration` (`d`) | CV | `1.0` | Duration: if the `clock` input is used, it is in clock ticks. Otherwise it is in seconds. A negative duration will be treated as zero. And a zero duration will make the output always be at `end` level. |
| `loop` (`lo`) | gate | `0` | If this is set to `1`, the transient will start over again as soon as it reaches the end. |
| `pingpong` (`pp`) | gate | `0` | If this set to `1`, the transient will start moving backwards towards the start when it has reached end. It will swing back and forth, in fact looping infinitely. |
| `freeze` (`f`) | gate | `0` | while this is set to `1`, the transient it frozen at its current position. |
| `reset` (`r`) | trigger | | A trigger here will immediately set the transient back to its start value. |
| `clock` (`c`) | gate | | If you patch a clock here, the duration will be set in terms of clock ticks, not of seconds. This needs to be a steady clock in order to get predictable results. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | Here comes the current value of the transient. |
| `phase` (`p`) | CV | This output reflects the current phase of the transient. It behaves as if `start` would be 0 and `end` would be 1. |
| `endoftransient` (`et`) | gate | When loop and pingpong is off, this output goes to `1` when the transient has reached the end – and stays there. In loop mode just a short trigger is sent. In pingpong mode that trigger is not sent when the transient has reach the `end`-value, but when it is back at start (i.e. after one full cycle). |
