---
circuit: logic
title: Logic operations utility
obsolete: false
ram_bytes: 56
manual_pages: [273, 274, 275]
category: logic-math
tags: [logic, and, or, xor, nand, nor, negate, gate, threshold, count]
see_also: [copy, math]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Stateless logic ops (AND/OR/XOR/NAND/NOR/negate/count) over up to 8 threshold-gated inputs with configurable low/high/count values.
verification_note: "Enumerate input combinations and thresholds and assert every output headlessly against a computed truth table."
---

# logic — Logic operations utility

Utility circuit for logic operations on gate signals. It can do operations like
AND, OR, NAND, NOR, etc.

## Basic operation

In this example we do an `and` operation. `O1` will output 1 (on) if all of
`I1`, `I2` and `I3` see on (voltage above 1 V):

```droid
[logic]
    input1     = I1
    input2     = I2
    input3     = I3
    and        = O1
```

Here is how to do a logic negate of a signal:

```droid
[logic]
    input   = I1
    negated = O1
```

If you do not like the 1 V threshold, you can change it:

```droid
[logic]
    input     = I1
    negated   = O1
    threshold = 5V
```

## Doing logic without this circuit

Please note, that many times when you think you need the logic circuit you can
do the same much simpler. Here is an example, where you use a toggle button to
switch on a clock, which is sent to output `O1`. The idea is to make an AND
combination of the clock signal and the button state:

```droid
[button]
    button = B1.1
    led    = L1.1

[lfo]
    hz     = 2
    square = _LFO

[logic]
    input1 = L1.1
    input2 = _LFO
    and    = O1
```

While this works pretty well, here is a solution that makes use of the fact,
that the *multiplication* of two gate signals is in fact a kind of AND
combination, since A × B is just 1, if A and B are 1 and 0 otherwise:

```droid
[button]
    button = B1.1
    led    = L1.1

[lfo]
    hz     = 2
    square = _LFO

[copy]
    input = _LFO * L1.1
    output = O1
```

You even can avoid the Copy-circuit if you make use of the `level` input of the
LFO, since setting the level to 0 disables it:

```droid
[button]
    button = B1.1
    led    = L1.1

[lfo]
    hz     = 2
    square = _LFO
    level  = L1.1
```

Another nice solution is to make use of `offvalue` and `onvalue` of the `button`
circuit. `offvalue` is 0 per default, so we just need to define `onvalue`:

```droid
[lfo]
    hz     = 2
    square = _LFO

[button]
    button  = B1.1
    led     = L1.1
    onvalue = _LFO
```

If you need to combine two gates in order to create a common gate pattern, you
can use addition – which is very similar to a logic OR combination. The
following example creates two overlayed euclidean rhythms:

```droid
[euklid]
    length = 16
    beats = 3
    output = _E1

[euklid]
    length = 13
    beats = 2
    output = _E2

[copy]
    input = _E1 + _E2
    output = O1
```

Note: When both `_E1` and `_E2` are 1 at the same time, the sum is 2, of course.
This does not matter, since the output voltage is capped at 10 V (`1.0`) anyway.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input1 … input8` (`i`) | gate | ☞ smart | 1st … 8th input. Note: this input is declared as a gate input, but in fact you can use it as a CV input in combination with various or random values set for the `threshold`. |
| `threshold` (`th`) | CV | `0.1` | Input values at, or above this threshold value, are considered high or on. The default is `0.1` which corresponds to an input voltage of 1 V. You can get interesting results when both the inputs are variable CVs (like from LFOs) and this threshold is being modulated as well. |
| `lowvalue` (`l`) | CV | `0.0` | Output value that is output for logic low, false or off. |
| `highvalue` (`h`) | CV | `1.0` | Output value that is output for a logic high, true or on. |
| `countvalue` (`cv`) | CV | `0.1` | This input affects the `count` and `countlow` outputs. If you use this, the value of those outputs are multiplied with the value of `countvalue`. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `and` (`a`) | CV | A logic AND operation on all patched inputs: This output is set to `highvalue` if all inputs are high (i.e. at least `threshold`), else `lowvalue`. |
| `or` (`o`) | CV | A logic OR operation on all patched inputs: This output is set to `highvalue` if at least one of the inputs is high. |
| `xor` (`x`) | CV | Exclusive OR: This is high, if the number of high inputs is odd! This means that any change in one of the inputs will also change the output. |
| `nand` (`na`) | CV | Like AND but the outcome is negated. |
| `nor` (`no`) | CV | Like OR but the outcome is negated. |
| `negated` (`n`) | CV | Logical negate of `input1` (which can abbreviated as `input`). Note: The inputs `input2 … input7` are ignored here. Another note: If you use `input1` anyway, `negated` always outputs exactly the same as `nand` and `nor`. It's just more convenient to write and easier to understand. Hence a dedicated output for a logic negate. |
| `count` (`c`) | integer | Outputs the number of inputs that are high. This number is multiplied with `countvalue` if you use that input. |
| `countlow` (`cl`) | CV | Outputs the number of inputs that are low. This number is multiplied with `countvalue` if you use that input. |
