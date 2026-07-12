---
circuit: ifequal
title: Check if two values are equal
obsolete: false
ram_bytes: 32
manual_pages: [266]
category: logic-math
tags: [equal, compare, condition, if, else, logic]
see_also: [compare]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Stateless exact-equality select between ifequal and else values; trivial conditional.
verification_note: "Verify the equality branch and floating-point exact-match caveat headlessly against a computed reference."
---

# ifequal — Check if two values are equal

This circuit is a simplified version of [`compare`](compare.md). It uses less
memory and CPU and just checks if two values are equal.

The following example shows the usage: The values of `input1` and `input2` are
compared. If they are equal (i.e. if `_TRACK` = 2), the value specified by
`ifequal` is output to `output`, otherwise the value of `else`.

```droid
[ifequal]
    input1 = _TRACK
    input2 = 2
    ifequal = _TRACK2_GATES
    else = 0
    output = O1
```

Notes:

- A comparison of a value read from an analog input will probably *never* work
  since there is always a tiny amount of jitter and noise.
- Comparison of floating point values like 0.3 might fail, as well, because these
  number can introduce internal rounding errors.
- If you run into these issues, use [`compare`](compare.md) instead. That circuit
  deals with unprecision by introducing a allowed range for the values to be
  equal.
- The main purpose of this circuit is to save a bit a RAM and CPU in cases where
  you don't use the full feature set of [`compare`](compare.md).

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input1` (`i1`) | CV | `0.0` | A value. |
| `input2` (`i2`) | CV | `1.0` | Another value. |
| `ifequal` (`q`) | CV | `1.0` | Value to be output if `input1` is exactly equal to `input2`. |
| `else` (`e`) | CV | `0.0` | Value to output otherwise. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | Here comes the result. |

## See also

- [`compare`](compare.md) — the fuller comparison circuit with an allowed range for equality.
