---
circuit: queue
title: Clocked CV shift register
obsolete: false
ram_bytes: 312
manual_pages: [372]
category: sequencer
tags: [shift-register, queue, delay, clock, cv, cells, melody-delay]
see_also: []
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: 64-cell clocked shift register with eight placeable output taps — trivial deterministic state.
verification_note: "Headless: clock a known CV sequence and assert each output tap reflects the cell at its outputpos after N shifts."
---

# queue — Clocked CV shift register

This circuit implements a shift register (a queue) with 64 cells. Each cell
contains one CV value. At each clock impulse the CVs each move one cell forwards.
The last CV is dropped. And the current input value is copied to the first cell.

There are eight outputs, which you can place at any of the 64 cells you like. If
you do not specify any placement, the outputs are placed at the first eight cells
– and thus the information in the remaining 56 cells is not being used.

The following example reads CVs from the input `I1`. `O4` always shows the CV
value that was seen at the input four cycles previously:

```droid
[queue]
    input = I1
    clock = I2
    output4 = O4
```

The next example places three outputs at the positions 3, 24 and 64:

```droid
[queue]
    input = I1
    clock = I2
    outputpos1 = 3
    outputpos2 = 24
    outputpos3 = 64
    output1 = O1
    output2 = O2
    output3 = O3
```

Please note:

- Since the DROID is very precise in processing CV voltages you can use the
  `queue` in order to delay melodies from sequencers etc.
- As always also the inputs `outputpos1 … outputpos8` may be CV controlled and
  change in time.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | CV | `0.0` | This CV will be pushed into the first cell of the shift register whenever a clock occurs. |
| `clock` (`c`) | trigger | | Each clock signal at this jack will move the CV content from every cell of the shift register to the next cell. The CV in the last cell will be dropped. |
| `outputpos1 … outputpos8` (`op`) | integer | ☞ smart | Specifies the position of each of the eight outputs – i.e. which cell of the shift register it should output. Allowed are values from 1 up to 64. These jacks default to 1, 2, … 8, so if you do not wire them the eight outputs reflect the first eight positions of the shift register. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output1 … output8` (`o`) | CV | Eight outputs for eight different positions of the register. If you do not wire `outputpos1 … outputpos8`, these outputs show the content of the 1st, 2nd, … 8th cell. |
