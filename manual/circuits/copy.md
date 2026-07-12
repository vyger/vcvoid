---
circuit: copy
title: Copy a signal, while applying attenuation and offset
obsolete: false
ram_bytes: 24
manual_pages: [188, 188]
category: utility
tags: [copy, attenuate, offset, scaling, mixer, adder, precision-adder, utility]
see_also: []
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Pure pass-through; all scaling/offset is the standard per-input A*B+C math the master already applies.
verification_note: "Headless: apply factor/offset to the input and assert output equals the computed A*B+C value."
---

# copy — Copy a signal, while applying attenuation and offset

This circuit is a simple utility that copies a signal from an input to an
output. Since every input generally can be attenuated and offset this can be
used for scaling and offsetting a signal on its path.

Build a simple precision adder (CV mixer), that adds the voltages of `I1` and
`I2`:

```droid
[copy]
    input = I1 + I2
    output = O1
```

Provide an attenuated signal from `I1` into an internal patch cable:

```droid
[copy]
    input = I1 * 0.5
    output = _INPUT_CV
```

Note: Previous versions of `copy` had an `inverted` output. This has been
removed in blue-3. But the same effect can be achieved by subtracting a signal
from `1`. This converts 0 V into 10 V, 2 V into 8 V, 10 V into 0 V and so on:

```droid
[copy]
    input = 10V - I1
    output = O1
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | CV | `0.0` | Connect the signal you want to copy here. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | The resulting signal will be sent here. |
