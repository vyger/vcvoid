---
circuit: multicompare
title: Compare in input with up to eight possible values
obsolete: false
ram_bytes: 88
manual_pages: [347]
category: logic-math
tags: [compare, select, lookup, ifequal, else, switch, signal-selection]
see_also: [switch]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Straight lookup — match input against up to 16 compare values and output the paired ifequal value, else the fallback.
verification_note: "Deterministic value selection; enumerate compare/ifequal pairs and confirm output against a computed reference including the else case and float-equality behavior."
---

# multicompare — Compare in input with up to eight possible values

With this circuit you can assign a different output value for up to eight
different input values. It allows you to pick one of eight signals based on the
current value of the input.

There is some overlap with [`switch`](switch.md), but there the offset needs to
be an integer in the range 0, 1, 2, etc. without any "holes". With multicompare
you can do a logic like the following:

- If the input is 5, use sawtooth
- If the input is 7, use triangle
- If the input is 98, use square
- else use sine

## Example

Here is an example:

```droid
[lfo]
    sawtooth = _SAWTOOTH
    triangle = _TRIANGLE
    square = _SQUARE
    sine = sine

[multicompare]
    input = _WAVEFORMSWITCH # from somewhere
    compare1 = 5
    ifequal1 = _SAWTOOTH
    compare1 = 7
    ifequal1 = _TRIANGLE
    compare1 = 98
    ifequal1 = _SQUARE
    else = _SINE
    output = O1
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | CV | `0.0` | A value to compare. |
| `compare1 … compare16` (`c`) | CV | | Up to eight reference values to compare the input with. |
| `ifequal1 … ifequal16` (`if`) | CV | | The output values if the according comparison value is found at the input. |
| `else` (`e`) | CV | `0.0` | Specifies the output value in case none of comparison values is found at the input. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | The value of the matching `ifequal` or `else` input. |

## See also

- [`switch`](switch.md) — related signal selector using integer offsets.
