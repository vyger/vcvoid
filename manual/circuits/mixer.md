---
circuit: mixer
title: CV mixer
obsolete: false
ram_bytes: 48
manual_pages: [314]
category: mixer-cv
tags: [mixer, sum, add, minimum, maximum, average, offset, attenuate, submix]
see_also: []
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Sum plus min/max/average over up to eight inputs; the only subtlety is that unpatched inputs are excluded from min/max/average.
verification_note: "Deterministic arithmetic; compare all four outputs against a computed reference, taking care to only include patched inputs in min/max/average."
---

# mixer — CV mixer

The main task of this circuit is simply adding up to eight inputs. Furthermore
it can do simple operations like minimum, maximum and average. Please note that
since every input can always be offset and attenuated, it's like a mixer with a
CV controlled level and CV controlled offset per input channel.

## Example

Minimal example, mixing together two inputs:

```droid
[mixer]
    input1 = I1
    input2 = I2
    output = O1
```

Since every input can add an offset, mixing four inputs can be done with two
lines if you like:

```droid
[mixer]
    input1 = I1 + I2
    input2 = I3 + I4
    output = O1
```

Please note that an unpatched input is (sometimes) not the same as an input
where 0.0 is being sent. The difference arises if you use `minimum`, `maximum`
and `average`, since these just consider the patched inputs.

If eight inputs are not enough then you can simply create a mesh by mixing
together the outputs of several submixers.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input1 … input8` (`i`) | CV | `0.0` | 1st … 8th mixing input. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | Sum of all patched inputs. |
| `maximum` (`x`) | CV | Maximum of all patched inputs of this circuit. This can e.g. be used for mixing together the envelopes from several sequencer tracks without making them "louder" or distorting them when two sequencers play a note at the same time. |
| `minimum` (`m`) | CV | Minimum of all patched inputs of this circuit. |
| `average` (`a`) | CV | Average of all patched inputs of this circuit. |
