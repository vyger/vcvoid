---
circuit: timing
title: Shuffle/swing and complex timing generator
obsolete: false
ram_bytes: 56
manual_pages: [398, 399]
category: clock-timing
tags: [shuffle, swing, groove, clock, timing, delay, beat, quantize, pattern]
see_also: []
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Up-to-8-step relative timing map over an input clock, with documented collision/ordering rules (merge coincident beats, drop late-before-early, replay never-played delayed beats).
verification_note: "Deterministic given a steady clock; all edge cases are specified, so verify output trigger timing headless against a computed reference clock."
---

# timing — Shuffle/swing and complex timing generator

This circuit converts a steady input clock into an output clock with flexible
timing modifications. The most common use is a "swing" feeling where every
second note is delayed. But this circuit is much more flexible.

The length of a timing pattern can be up to eight steps. That means that you can
set a different relative time shift for each clock pulse in a sequence of up to
eight.

## Example

Let's start with a simple swing pattern, which is just a sequence of two. We
assume an external input clock at G1 and output the resulting modified clock to
G2:

```droid
[timing]
    clock = G1
    output = G2
    timing1 = 0.0
    timing2 = 0.3
```

In this example every second clock pulse is delayed by 30% of one clock tick's
duration — which gives a standard swing pattern.

Creating a reverse swing, where every second pulse is early is as easy as using
a negative number for `timing2`:

```droid
[timing]
    clock = G1
    output = G2
    timing1 = 0.0
    timing2 = -0.3
```

Creating a sequence with an odd number of steps can create rather weird groove
patterns. Look at the following example:

```droid
[timing]
    clock = G1
    output = G2
    timing1 = 0.0
    timing2 = 0.2
    timing3 = 0.1
```

Now every second note *of three* is delayed by 20% and every third note by 10%.

Of course, you can use `timing` in order to create a simple clock shift by
creating a pattern with just one timing, as well. The following example will
shift the input clock *forwards*, so that it always comes a bit earlier. This can
be used for compensating a slight delay of a master clock:

```droid
[timing]
    clock = G1
    output = G2
    timing1 = -0.03
```

## Notes

- This circuit needs a steady and stable input clock.
- In order to get a synchronized start together with the rest of your patch, it
  is advisable also to make use of the `reset` input.
- You cannot shift a beat forward or backward by more than 99.99% of a clock
  tick.
- When you set your timings in a way that two beats happen at the same time, just
  one trigger is output for these two beats.
- When you set your timings in a way that a later beat would come before an
  earlier beat, the later beat is not played.
- For each input beat there is at max one output beat. If for any input beat the
  corresponding output beat has already been played, it will not be replayed even
  if you suddenly shift it into the future.
- If an output beat has not yet played because it is delayed and then you suddenly
  reduce the delay by an amount that would shift that beat into the past, it is
  played immediately (so it is not lost).

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `clock` (`c`) | gate | | Patch a steady clock here for this circuit to be of any use. |
| `reset` (`r`) | trigger | | A trigger here resets the internal step counter and restart at step 1. |
| `timing1 … timing8` (`t`) | CV | ☞ smart | Specifies a *relative* timing for each step in relation to the input clock. A timing of 0.3 will shift the respective beat 30% of a clock cycle behind, while -0.3 will make it 30% early. The timing values are clipped into the range -0.9999 … 0.9999. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | gate | Here comes the modified output clock. |
