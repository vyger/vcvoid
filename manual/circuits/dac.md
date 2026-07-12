---
circuit: dac
title: DA Converter with 12 bits
obsolete: false
ram_bytes: 56
manual_pages: [193, 194]
category: conversion
tags: [converter, binary, bits, digital-to-analog, resolution, gate, msb, lsb]
see_also: [adc]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Straight binary-to-value mapping with an explicit formula and worked truth tables in the manual.
verification_note: "Headless: apply bit combinations and assert output equals the manual's tabulated value scaled into the minimum..maximum range."
---

# dac — DA Converter with 12 bits

This circuit converts a binary representation of up to 12 bits into an output
value in a given range. The reverse circuit is [`adc`](adc.md).

Consider the following example:

```droid
[dac]
    bit1 = I1
    bit2 = I2
    bit3 = I3
    output = O1
```

In this example three bits are being used. Three bits can represent a number
from 0 to 7. These are mapped to the input range from 0 to 1 (or 0 V to 10 V) in
the following way:

| bit1 | bit2 | bit3 | bit value | output |
|------|------|------|-----------|--------|
| 0 | 0 | 0 | 0 | 0.000 |
| 0 | 0 | 1 | 1 | 0.143 |
| 0 | 1 | 0 | 2 | 0.286 |
| 0 | 1 | 1 | 3 | 0.429 |
| 1 | 0 | 0 | 4 | 0.571 |
| 1 | 0 | 1 | 5 | 0.714 |
| 1 | 1 | 0 | 6 | 0.857 |
| 1 | 1 | 1 | 7 | 1.000 |

In other words: this circuit will convert three different gate inputs into one
analog output value. `bit1` has the most influence, `bit3` the least.

The normal output range is 0 to 1 (i.e. 10 V) per default, but you can change
that with the parameters `minimum` and `maximum`. For example you could have the
three bits mapped to just the range of 0.1 to 0.5:

```droid
[dac]
    bit1 = I1
    bit2 = I2
    bit3 = I3
    minimum = 0.1 # 1V
    maximum = 0.5 # 5V
    output = O1
```

Now the table looks like this:

| bit1 | bit2 | bit3 | bit value | output |
|------|------|------|-----------|--------|
| 0 | 0 | 0 | 0 | 0.100 |
| 0 | 0 | 1 | 1 | 0.157 |
| 0 | 1 | 0 | 2 | 0.214 |
| 0 | 1 | 1 | 3 | 0.271 |
| 1 | 0 | 0 | 4 | 0.329 |
| 1 | 0 | 1 | 5 | 0.386 |
| 1 | 1 | 0 | 6 | 0.443 |
| 1 | 1 | 1 | 7 | 0.500 |

If you use more of the bit-outputs you get more resolution. For example if you
use `bit1 … bit8`, the total range will be divided into 256 possible output
values. The maximum is 12 bits. Since bit 1 is the most significant bit, adding
more and more bits will not change the influence of the already used bits.

Please also have a look at the circuit [`adc`](adc.md), which does the exact
opposite!

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `bit1 … bit12` (`b`) | gate | ☞ smart | The 12 bit input bits. `bit1` is the MSB – the most significant bit. The LSB (least significant bit) is the highest input that you actually patch. |
| `minimum` (`m`) | CV | `0.0` | This sets the lower bound of the output range, i.e. the value that the bit sequence `000000000000` will produce. |
| `maximum` (`x`) | CV | `1.0` | This sets the upper bound of the output value, i.e. the value that the bit sequence `111111111111` will produce. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | Output signal. |

## See also

- [`adc`](adc.md) — AD converter; does the exact opposite.
