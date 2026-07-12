---
circuit: adc
title: AD Converter with 12 bits
obsolete: false
ram_bytes: 56
manual_pages: [125, 126]
category: conversion
tags: [converter, binary, bits, gate, analog-to-digital, resolution]
see_also: [dac]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: "Deterministic bit-slicing of a scaled input range; the min/max mapping and MSB-first encoding are fully specified."
verification_note: "Headless: sweep the input across the min/max range and compare each bit output against the computed binary threshold table. Fully deterministic, no gaps."
---

# adc — AD Converter with 12 bits

Converts an input value into a binary representation of up to 12 bits. Each bit
is emitted as a separate gate output. The reverse circuit is [`dac`](dac.md).

The applications are various and often surprising. For example, feed different
LFO wave forms (other than square) into `input` to get slower and faster gate
patterns.

## How it works

The input range (`minimum` … `maximum`, default `0.0` … `1.0`, i.e. 0 V … 10 V)
is divided into equal pieces and encoded across the used bit outputs. `bit1` is
the MSB (most significant bit). Using more bits gives more resolution: with
`bit1 … bit8` the range is divided into 256 equal pieces. Adding more bits never
changes the behaviour of `bit1`.

Values below `minimum` map to all zeros; values above `maximum` map to all ones.

## Example

```droid
[adc]
    input = I1
    bit1 = O1
    bit2 = O2
    bit3 = O3
```

Three bits represent a number from 0 to 7, mapped across the input range
0 … 1 (0 V … 10 V):

| input | bit1 | bit2 | bit3 | bit value |
|-------|------|------|------|-----------|
| −∞ … 0.125 | 0 | 0 | 0 | 0 |
| 0.125 … 0.250 | 0 | 0 | 1 | 1 |
| 0.250 … 0.375 | 0 | 1 | 0 | 2 |
| 0.375 … 0.500 | 0 | 1 | 1 | 3 |
| 0.500 … 0.625 | 1 | 0 | 0 | 4 |
| 0.625 … 0.750 | 1 | 0 | 1 | 5 |
| 0.750 … 0.875 | 1 | 1 | 0 | 6 |
| 0.875 … ∞ | 1 | 1 | 1 | 7 |

With `minimum = 0.1` (1 V) and `maximum = 0.5` (4 V) the same three bits instead
span 0.1 … 0.5:

| input | bit1 | bit2 | bit3 | bit value |
|-------|------|------|------|-----------|
| −∞ … 0.15 | 0 | 0 | 0 | 0 |
| 0.15 … 0.20 | 0 | 0 | 1 | 1 |
| 0.20 … 0.25 | 0 | 1 | 0 | 2 |
| 0.25 … 0.30 | 0 | 1 | 1 | 3 |
| 0.30 … 0.35 | 1 | 0 | 0 | 4 |
| 0.35 … 0.40 | 1 | 0 | 1 | 5 |
| 0.40 … 0.45 | 1 | 1 | 0 | 6 |
| 0.45 … ∞ | 1 | 1 | 1 | 7 |

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | CV | `0.0` | Input signal to convert to binary representation. |
| `minimum` (`m`) | CV | `0.0` | The lowest assumed input value. This value and all lower values are converted to the bit sequence `000000000000`. |
| `maximum` (`x`) | CV | `1.0` | The highest assumed input value. This value and all higher values are converted to the bit sequence `111111111111`. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `bit1 … bit12` (`b`) | gate | The 12 bit outputs. `bit1` is the MSB. The LSB (least significant bit) is the highest output you actually patch. If you do not need the full 12-bit resolution, just use the first couple of outputs. |

## See also

- [`dac`](dac.md) — DA converter; does the exact opposite.
