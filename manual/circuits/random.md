---
circuit: random
title: Random number generator
obsolete: false
ram_bytes: 32
manual_pages: [373]
category: random
tags: [random, noise, sample-and-hold, clock, steps, minimum, maximum, voltage]
see_also: [bernoulli]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Clocked or free random between min and max with optional discrete steps — simple, but the output is non-deterministic by design.
verification_note: "Headless but partial: cannot assert exact values; verify range bounds, step quantization and clocked sample-and-hold behavior, plus distribution statistics over many samples."
---

# random — Random number generator

A random number generator with clocked and unclocked mode, that can either create
voltages at discrete steps and completely free values.

This circuit creates random numbers between two tunable levels `minimum` and
`maximum`. In clocked mode each clock creates and holds a new random value. In
unclocked mode the random values change at the maximum possible speed (about 6000
times per second).

Simple example for clocked random numbers between 0.0 and 1.0 (1.0 translates into
10 V at the output):

```droid
[random]
    clock      = I1
    output     = O1
```

Example for creating random output voltages between 1 V and 3 V:

```droid
[random]
    clock   = I1
    output = O1
    minimum = 1V
    maximum = 3V
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `clock` (`c`) | trigger | | Optional trigger: if this input is used then the output holds the current random number until the next clock impulse (sample & hold). |
| `minimum` (`m`) | CV | `0.0` | Minimum possible random number. |
| `maximum` (`x`) | CV | `1.0` | Maximum possible random number. |
| `steps` (`s`) | integer | `0` | Number of different voltage levels. If this is set to 0 (default), any voltage can appear, there is no limit. If this is `1`, then there is no random any more since there is only one allowed step (which is the average between `minimum` and `maximum`). At `2` the only two possible output values are `minimum` and `maximum`. At `3` the possible levels are `minimum`, (`minimum` + `maximum`) / 2 and `maximum` and so on… |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | Output of the random number / voltage. |
