---
circuit: case
title: Switch choosing from inputs via conditions
obsolete: false
ram_bytes: 88
manual_pages: [168]
category: switch-routing
tags: [switch, select, condition, priority, precedence, clock-source, routing, fallback, else]
see_also: [switch, select, ifequal]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: "Priority selector that copies the first value whose case input is non-zero, else a fallback."
verification_note: "Headless: set combinations of case inputs and confirm the highest-priority matching value (or else) appears at the output. Fully deterministic."
---

# case — Switch choosing from inputs via conditions

This circuit selects one of several inputs and routes its signal to the output
based on which of several conditions is true. For each signal there is one
related `case` input. The first signal whose case input is non-zero, is selected.

One example application is selecting one out of several clock sources, depending
on which clock is present. In this example we assume that for each clock source
there is an internal cable that is 1 if that clock is present, and 0 otherwise:

```droid
[case]
    case1  = _INTERNAL_CLOCK_PRESENT
    value1 = _INTERNAL_CLOCK
    case2  = _EXTERNAL_CLOCK_PRESENT
    value2 = _EXTERNAL_CLOCK
    case3  = _MIDI_CLOCK_PRESENT
    value3 = _MIDI_CLOCK
    output = _CLOCK
```

The order of the values is important here, since it defines the precedence of the
individual inputs. If `_INTERNAL_CLOCK_PRESENT` is non-zero, `input1` is copied
to `output`, regardless of what happens at the other inputs.

If all of the case inputs are zero, 0 is output, but you can set a different
fallback value with the input `else`.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `case1 … case16` (`c`) | CV | | 1st … 16th case input. The first one that is non-zero defines which input value to use. |
| `value1 … value16` (`v`) | CV | | 1st … 16th value input. One of these is copied to the `output`, depending on which of the `case` inputs is non-zero. |
| `else` (`e`) | gate | `0` | In case none of the `case` inputs is non-zero, this value is copied to the output. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | To this output the selected value input is copied. |
