---
circuit: euklid
title: Euclidean rhythm generator
obsolete: false
ram_bytes: 48
manual_pages: [243, 244]
category: clock-timing
tags: [euclidean, rhythm, trigger, pattern, beats, offbeats, clock, gate, sequencer]
see_also: []
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Deterministic Euclidean pattern generator with worked examples and clear length/beats/offset semantics.
verification_note: "Pattern tables in the doc give exact expected outputs; verify beat placement, offset rotation, and offbeats complement headlessly against computed reference patterns."
---

# euklid — Euclidean rhythm generator

This circuit creates trigger patterns according to the well-known *Euclidean
rhythms* and is of course CV controllable. The pattern is described by three
numbers:

- The number of steps in the pattern
- The number of beats in the pattern
- An offset for shifting the beats forward

The number of beats are distributed as evenly as possible in the pattern — but
of course are all placed precisely on clock beats. Here are a few examples of
various patterns:

| Pattern | Steps (● = beat) |
|---------|------------------|
| length: 16, beats: 4, offset: 0 | ●···●···●···●··· |
| length: 16, beats: 5, offset: 0 | ●··●··●··●···●·· |
| length: 16, beats: 5, offset: 1 | ·●··●··●··●···●· |
| length: 16, beats: 11, offset: 0 | ●●●·●●·●●·●●·●●· |
| length: 13, beats: 5, offset: 0 | ●··●··●·●··●·· |
| length: 13, beats: 5, offset: 1 | ·●··●··●·●··●· |
| length: 4, beats: 2, offset: 1 | ·●·● |

## Example

Here is an example without CV control:

```droid
[euklid]
    clock     = G1
    reset     = G2
    length    = 16
    beats     = 5
    offset    = 0
    output    = G3
```

Now let's change that in order to make the beats controllable by the pot `P1.1`.
Please note how the pot range is being changed from the default 0 … 1 to the
necessary 1 … 16 by using a factor of 15 and an offset of 1:

```droid
[euklid]
    clock     = G1
    reset     = G2
    length    = 16
    beats     = P1.1 * 15 + 1
    offset    = 0
    output    = G3
```

By the way: Since the default for `length` is `16` and for `offset` `0` you can
drop those two lines if you like:

```droid
[euklid]
    clock     = G1
    reset     = G2
    beats     = P1.1 * 15 + 1
    output    = G3
```

## Offbeats

The output `offbeats` does the exact opposite of `output`: it triggers at those
clock beats where `output` does not. So at any given clock tick exactly either
`output` or `offbeats` triggers.

## Gate length

The length of the output gate is the same as that of the input gate. Also the
exact voltage from the input is copied to the output while the current step is
active.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `clock` (`c`) | gate | ☞ smart | Patch a clock signal here. It does not need to be steady — even if this is the most usual application. Note: this input is classified as a gate input, since the length of the gate is being preserved when forwarded to `output` and `offbeats`. |
| `reset` (`r`) | trigger | | A trigger here resets the pattern to the start. |
| `outputsignal` (`os`) | CV | ☞ smart | Usually on active steps `euklid` just lets the original input clock get through to the output. If this parameter is used, it will be sent to the output on active steps instead. The easiest application is just setting it to 1. The output will then become 1 the whole time while the current step is active. This is useful if you want to use `euklid` as modulation CV rather than as trigger. |
| `length` (`l`) | integer | `16` | The length of a pattern. This is interpreted as an integer number, which must be greater than 0. If it is not then 1 is assumed. If you CV control the length, use multiplication. The maximum accepted length is 64. |
| `beats` (`b`) | integer | `5` | The number of active beats that should be distributed amongst the `length` steps. If that number is greater than `length`, it is capped to that number. |
| `offset` (`of`) | integer | `0` | rotates or shifts the pattern by that number of steps. This number can be positive or negative. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | gate | Output of the `beats` in the current pattern. The gate length is directly taken from the input clock — just as the voltage. |
| `offbeats` (`ob`) | gate | Here those impulses will be output where there is `no` beat in the pattern. |
