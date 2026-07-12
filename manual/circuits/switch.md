---
circuit: switch
title: Adressable/clockable switch
obsolete: false
ram_bytes: 112
manual_pages: [394, 395]
category: switch-routing
tags: [switch, multiplexer, router, sequential-switch, forward, backward, offset, shuffle, matrix, addressable]
see_also: []
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: n x m addressable/clocked routing with offset, forward/backward, reset, shuffle and sort; edge cases (reset-wins-within-5ms, unconnected inputs as 0 V) are all specified.
verification_note: "Routing math is deterministic and fully documented, verifiable headless against a computed reference. Shuffle uses randomness so verify its invariants (each output a distinct input) rather than an exact permutation."
---

# switch — Adressable/clockable switch

This circuit supports a set of various switching operations. It can switch
several inputs to one output either by means of addressing the input via CV or by
stepping forward and backward. You can do the same vice versa: connecting one
input to one of several outputs while setting the inactive outputs to 0 V.

You can even use several inputs *and* outputs at the same time and thus create an
*n* × *m* switch with the option of rotating the outputs against the inputs by
means of addressing or stepping.

At minimum you need to patch two inputs and one output (or vice versa), plus a
switch like `forward`, `backward` or `offset`.

The first example switches four inputs `I1 … I4` to one output `O1` be means of a
trigger at `forward`. At the beginning `I1` is wired to `O1`. Each time a trigger
is seen at `forward` the switch switches to the next input and at the end starts
over at `I1` again. So it cycles through `I1` → `I2` → `I3` → `I4` → `I1`:

```droid
[switch]
    input1 = I1
    input2 = I2
    input3 = I3
    input4 = I4
    output = O1
    forward = I8
```

Please note, that `output` and `output1` are synonyms here. You can use either
way you like. Just the same is `input` just a shorthand for `input1`.

Now Let's do the opposite thing: distribute one input to four different outputs:

```droid
[switch]
    input = I1
    output1 = O1
    output2 = O2
    output3 = O3
    output4 = O4
    forward = I8
```

Now, if you try this out, you might notice that a trigger to `forward` moves the
selected output *backwards*! This is no bug but very logical. The reason will get
more clear if we build a switch with several inputs *and* outputs. Let's make a
3×3 switch:

```droid
[switch]
    input1 = I1
    input2 = I2
    input3 = I3
    output1 = O1
    output2 = O2
    output3 = O3
    forward = I8
```

Now a trigger to `forward` moves each output forward to the next input. That is
the same as saying each input moves *backward* to the previous output. Of course
you can change the direction by using `backward` instead of `forward`.

Instead of moving the switch with a trigger you also can *address* it by using a
CV at the input `offset`. In this example we use a steady CV being either 0 (for
selecting `O1`) or 1 (10 V) for selecting `O2`:

```droid
[switch]
    input   = I1
    output1 = O1
    output2 = O2
    offset  = I7
```

Using two inputs and two outputs creates a switch that can swap these two. Here
with offset 0 `input1` is connected to `output1` and `input2` to `output2`. If
`offset` is 1, `input1` will be connected to `output2` and `input2` to `output1`.

```droid
[switch]
    input1 = I1
    input2 = I2
    output1 = O1
    output2 = O2
    offset = I7
```

Now let's make another example for a CV addressable switch. The CV is read from
`I7`. At a voltage of 0 V `output1` is connected to `input1`, at 1 V to `input2`,
at 2 V to `input3`, at 3 V to `input4`, at 4 V to `input1` again, at 5 V to
`input2` and so on:

```droid
[switch]
    input1 = I1
    input2 = I2
    input3 = I3
    input4 = I4
    output1 = O1
    offset = I7 * 10 # 1 V per switch step
```

Generally speaking, if you connect less inputs than outputs, the unconnected
inputs are regarded as getting a 0 V input. If you connect less outputs then
inputs, the unconnected outputs send their values into the black horrible void.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input1 … input16` (`i`) | CV | `0.0` | 1st … 16th input. Use these inputs in order and don't leave gaps. |
| `forward` (`f`) | trigger | | If a trigger or gate is received here, the switch adds one to the current internal switch offset. So every output moves to the next input and every input moves to the previous output. |
| `backward` (`b`) | trigger | | Similar then `forward`, but switches backwards. |
| `reset` (`r`) | trigger | | Resets the switch to its initial position. Assuming `offset` is at 0, `input1` is connected to `output1`, `input2` to `output2` etc. Also any previous trigger to `shuffle` will be "forgotten". If `reset` and a trigger at `forward` / `backward` happen at the same time (within 5 ms), the reset will win and the switch is being reset to offset 0. This avoids problems with unprecise timing of external sequencers. |
| `shuffle` (`s`) | trigger | | A trigger here randomly assignes each output to a (new) random input. No input is used more than once. The inputs `offset`, `forward` and `backward` still work after shuffling, so you can move around the shuffled connections. You can shuffle as often as you like. |
| `sort` (`so`) | trigger | | A trigger here undoes all previous triggers to `shuffle`. It goes back to a unshuffled, ordered state. The current offset, that is set by `offset`, `forward` and `backward` is not affected by this trigger. It is conserved (that's the difference to the `reset` input trigger). |
| `offset` (`of`) | integer | `0` | This allows CV addressable switching. The number read here is being used a shifting offset and is always added to the internal offset. For example if you send 5 here, it is like you have triggered `forward` five times after the last reset. Please note, then 5 would mean 50 Volts, not 5 Volts. So if you patch an external CV like `I1` here, you probably want to multiply with some useful number. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output1 … output16` (`o`) | CV | 1st … 16th output. Use these outputs in order and don't leave gaps. |
