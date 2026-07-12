---
circuit: compare
title: Compare two values
obsolete: false
ram_bytes: 32
manual_pages: [181, 182]
category: logic-math
tags: [compare, comparator, less, greater, equal, precision, decision, switch, ifequal, ifless, ifgreater]
see_also: [ifequal, multicompare]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Stateless three-way comparator with optional precision band; direct arithmetic implementation.
verification_note: "Headless: feed input/compare/precision pairs and assert the selected ifless/ifequal/ifgreater/else branch matches computed expectations, including the empty-inputs default to ifequal=1."
---

# compare — Compare two values

This simple utility circuit allows you to make a decision by comparing an input
value (at `input`) against a reference value (at `compare`) and output one of
three values depending on whether the input is less than, greater than or equal
to the reference.

The following simple example checks if the pot `P1.1` is left of the center (a
value less than 0.5). If that is so, it outputs `1`, otherwise `0`.

```droid
[compare]
    input = P1.1
    compare = 0.5
    ifless = 1
    output = O1
```

You can change the default output value of `0` with the input `else`. That
specifies what happens if the condition is *not* met. The following example
outputs `-1`, if `P1.1` is greater or equal to 0.5.

```droid
[compare]
    input = P1.1
    compare = 0.5
    ifless = 1
    else = -1
    output = O1
```

## Equality, analog unprecision

You can also check if two values are *equal*. This is done with `ifequal`.
Check this out:

```droid
[compare]
    input = B1.1
    compare = 1
    ifequal = 4
    else = 8
    output = O1
```

Now while you hold the button `B1.1` this circuit will output the value `4` and
otherwise `8`.

Note: equality can be tricky when it comes to values from *analog* things like
inputs or potentiometers. They always undergo tiny random fluctuations. So the
following example, that should compare the current voltages of two inputs, will
never really work:

```droid
[compare]
    input = I1
    compare = I2
    ifequal = 1 # will never happen!
    output = O1 # This won't work!
```

If you try this out, you will probably *never* get both inputs equal. Even a
single electron too much could theoretically make the difference. So in order to
make such comparisons possible, there is a way to allow for a *slight
unprecision* when doing the comparison. This is set with the `precision`
parameter:

```droid
[compare]
    input = I1
    compare = I2
    precision = 0.1
    ifequal = 1
    output = O1
```

Now the inputs `I1` and `I2` are being treated as equal as long as their
difference is `0.1` (1 V) at most.

## Making a three-way switch

It is possible to check all three relations at once. Make sure that you apply a
`precision` if you deal with analog values:

```droid
[compare]
    input = I1
    compare = I2
    precision = 0.1
    ifless = 0
    ifequal = 1
    ifgreater = 2
    output = O1
```

Now you get `0`, `1` or `2`, depending on whether `I1` is less, equal or greater
than `I2`.

Note: Better do not use just `ifless` and `ifgreater` without using `ifequal` or
`else`. This lets the equality undefined and will output 0 if for any chance the
two input values are equal. Better use `ifless` / `ifgreater` in combination
with `else` if you are not interested in the exact equality.

## Omitted inputs

It is allowed to omit any of the inputs `ifless`, `ifequal`, `ifgreater` or
`else`. Any of these is treated as 0 with one exception: If you omit all four,
`ifequal` defaults to `1`. This makes a super basic `compare` circuit just check
if two values are equal:

```droid
[compare]
    input = B1.1
    compare = 0
    output = O1
```

This will output `1` if button `B1.1` has the value 0 (is not pressed).

## Dynamic output values

As often, instead of using fixed values for `ifless`, `ifequal`, `ifgreater` and
`else` you can use dynamic values from somewhere else, of course. The following
example will output a sine wave at `O1` if the pot is left of the center or else
a square wave:

```droid
[lfo]
    hz = 2
    sine = _SINE
    square = _SQUARE

[compare]
    input = P1.1
    compare = 0.5
    ifless = _SINE
    else = _SQUARE
    output = O1
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | CV | `0.0` | A value to compare. |
| `compare` (`c`) | CV | `0.0` | A reference value to compare the input with. |
| `ifgreater` (`g`) | CV | ☞ smart | Value to be output if `input` is greater than `compare`. If you patch nothing here, the value of the input `else` will be used. |
| `ifless` (`l`) | CV | ☞ smart | Value to be output if `input` is less than `compare`. If you patch nothing here, the value of the input `else` will be used. |
| `ifequal` (`q`) | CV | ☞ smart | Value to be output if `input` is equal to `compare` within the precision defined by `precision`. If you patch nothing here, the value of the input `else` will be used. |
| `else` (`e`) | CV | `0.0` | Specifies the output value in case none of the stated conditions are met. |
| `precision` (`pc`) | CV | `0.0` | An optional precision to be used by `ifequal`. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | Here one of `ifgreater`, `ifless` or `ifequal` is output. |
