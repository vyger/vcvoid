---
circuit: math
title: Math utility circuit
obsolete: false
ram_bytes: 64
manual_pages: [276, 277, 278]
category: logic-math
tags: [math, sum, difference, product, quotient, modulo, power, sine, cosine, logarithm, square-root, round, floor, ceil]
see_also: [logic]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Stateless math ops (sum/diff/product/quotient/modulo/power/trig/log/round...) with documented edge cases for divide-by-zero, negative roots, and neutral omitted inputs.
verification_note: "Every output is a pure function; verify each op and its documented edge cases (input2=0, negative bases, unpatched-input neutrality) headlessly against a computed reference."
---

# math — Math utility circuit

This circuit provides mathematic operations. Some of these use `input1` and
`input2` – such as `sum` or `product`. Other ones just use `input1` (which can
be abbreviated as `input`) – such as negation or reciprocal.

Example for computing the quotient I1 / I2:

```droid
[math]
    input1   = I1
    input2   = I2
    quotient = O1
```

Example for computing the square root of `I1`:

```droid
[math]
    input = I1
    root  = O1
```

Note: As long as you do not send a value directly to an output like `O1`, the
range of the value is not limited by this circuit. You can generate almost
arbitrary small or large positive and negative numbers. When you send a value to
an output, it will be truncated into the range -1 … +1 (which corresonds to
-10 V … +10 V).

## Unused inputs

When you don't use both inputs for an operation that usually needs to values,
the omitted input will make the operation "neutral". For example in the
multiplication an omitted input will be treated as `1.0` where as in the sum it
defaults to `0.0`. This is useful when you want to temporarily disable a line in
your patch. Consider the following patch, which multiplies the incoming CV from
`I1` with the pot value of `P1.2` and outputs it to `O1`.

```droid
[math]
    input1 = I1
    input2 = P1.2
    product = O1
```

If you now remove the line with `input2`, the output will simply copy the input,
not set it to 0:

```droid
[math]
    input1 = I1
    # input2 = P1.2
    product = O1 # will be set to I1, not 0
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input1`, `input2` (`i`) | CV | | The two inputs. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `sum` (`s`) | CV | input1 + input2 |
| `difference` (`d`) | CV | input1 − input2 |
| `product` (`p`) | CV | input1 × input2 |
| `quotient` (`qu`) | CV | input1 / input2. If input2 is zero, a very large number will be returned, while the correct sign is being kept. This is mathematically not correct but more useful than any other possible result. |
| `modulo` (`md`) | CV | input1 modulo input2. This needs some explanation: With this operation you can "fold" the value from input1 into the range 0 … input2. For example if input2 is 1 V, the output will convert 1.234 V to 0.234 V, -2.1 V to 0.9 V and 0.5 V to 0.5 V. If input2 is zero or negative, the output will be zero. |
| `power` (`pw`) | CV | input1 to the power of input2. Please note that the power has several cases where it is not defined when either the base or the exponent is zero or less than zero. In order to be as useful for your music making as possible the math circuit behaves in the following way: • If input1 < 0, input2 is rounded to the nearest integer. • If input1 = 0 and input2 < 0, a very large number is output. |
| `average` (`a`) | CV | The average of input1 and input2. |
| `maximum` (`x`) | CV | The maximum of input1 and input2. |
| `minimum` (`m`) | CV | The minimum of input1 and input2. |
| `negation` (`n`) | CV | −input1 |
| `reciprocal` (`rc`) | CV | 1 / input1. If input1 is zero, a very large number is being output, while the sign is being kept. |
| `amount` (`am`) | CV | The absolute value of input1 (i.e. −input1 if input1 < 0, else input1). |
| `sine` (`si`) | CV | The sine of input1 in a way, the input range of 0.0 … 1.0 goes exactly through one wave cycle. Or more mathematically expressed: sin(2π × input1). |
| `cosine` (`cs`) | CV | The cosine of input1 in a way, the input range of 0.0 … 1.0 goes exactly through one wave cycle. Or more mathematically expressed: cos(2π × input1). |
| `square` (`q`) | CV | input1² |
| `root` (`ro`) | CV | √input1. Please note that you cannot compute the square root of a negative number. In order to output something useful anyway, the result will be −√−input1, if input1 < 0. |
| `logarithm` (`l`) | CV | The natural logarithm of input1: ln input1. The logarithm is only defined for positive numbers. Otherwise this output behaves like this: • If input1 = 0, a negative very large number is output. • If input1 < 0, −ln −input1 is output. |
| `log2` (`l2`) | CV | The binary logarithm of input1. This is only defined for positive numbers. Otherwise this output behaves like this: • If input1 = 0, a negative very large number is output. • If input1 < 0, −log2 −input1 is output. |
| `round` (`rd`) | CV | The integer number nearest to input1. |
| `floor` (`f`) | CV | The largest integer number that is not greater than input1. |
| `ceil` (`c`) | CV | The smallest integer number that is not less than input1. |
