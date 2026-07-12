---
circuit: switchedpot
title: Overlay pot with multiple functions
obsolete: true
ram_bytes: 88
manual_pages: [396, 397]
category: controller-ui
tags: [pot, potentiometer, virtual-pot, overlay, switch, bipolar, obsolete, pickup]
see_also: [pot, button, buttongroup]
impl_difficulty: moderate
controller_binding: controller-required
verification: rack-automated
spec_gap: false
difficulty_note: Obsolete overlay pot; up to 8 virtual pots gated by switch inputs, with a fully documented proportional pickup algorithm and bipolar mapping plus flash persistence.
verification_note: "Reads a physical pot (P-controller), so needs a Rack controller module. The pickup math is exactly specified, so a Rack-driven test that sweeps the pot param and reads outputs verifies it without a human."
---

# switchedpot — Overlay pot with multiple functions

> **⚠️ OBSOLETE.** This circuit has been superseded by the new circuit
> [`pot`](pot.md). `pot` can do all `switchedpot` can do and much more.
> `switchedpot` will be removed soon.

This circuit allows you to use one of your potentiometers on your controllers for
up to eight different functions. It is like creating up to eight *virtual* pots.
With the inputs `switch1 … switch8` you select, which of these virtual pots are
currently active. When you turn the (physical) pot, all active virtual pots are
being changed.

The values of all virtual pots start at center position (0.5).

The current values of all virtual pots are saved in the DROID's internal flash
memory, so next time you power on you have all settings of the virtual pots
reserved.

Here is an example, where one pot is used to control both decay and release of an
envelope.

```droid
[switchedpot]
    pot       = P1.1
    switch1   = B1.1
    switch2   = B1.2
    output1   = _DECAY
    output2   = _RELEASE

[contour]
    gate          = I1
    decay         = _DECAY
    release       = _RELEASE
    output        = O1
```

Now — while you press and hold button `B1.1` and turn the knob, the decay
parameter will change. Holding `B1.2` will change release. Holding *both* at the
same time is also possible and will change decay and release at the same time.

Hints:

- If you do not like to hold the buttons then you might want to use the
  [`button`](button.md) circuit for converting the buttons into toggle buttons.
- If you want one button per function and want always one pot to be selected, you
  can use the [`buttongroup`](buttongroup.md) circuit for combining the buttons
  into a group.

## Picking up the pots

Pots are no encoders. So when reusing a pot for more than one function at a time
there is always the problem that when you switch to one pot function the pot
probably currently is not set to the current value of the function. As an example
let's assume that — using the upper example — you first press `B1.1` and set decay
fully CW `1.0`. Now you select release. Because `0.5` is the start position of
every virtual pot that is the current value of release. But the physical pot is
at `1.0`.

DROID solves this in the following way:

- If you turn the physical pot *right*, then the value of the virtual pot is
  always increased until both pots reach `1.0` at the same time.
- If the physical pot is already at `1.0` when you select a virtual pot, it cannot
  be increased further. You first have to turn the pot left a bit and then right
  again.
- If you turn the physical pot *left*, then the value of the virtual pot is always
  *decreased* until both pots reach `0.0` at the same time.
- If the physical pot is already at `0.0` when you select a virtual pot, it cannot
  be decreased further. You first have to turn the pot right a bit and then left
  again.

Let's assume that the virtual pot is at `0.4` when you select it. And let's
further assume that the physical pot is at position `0.8`. When you turn it left
the physical pot as a way of 0.8 go until 0.0 and the virtual just 0.4. So the
virtual pot is moving with half of the speed, so that both reach 0.0 at the same
time. When you turn the pot *right*, on the other hand, the virtual pot has 0.6 to
go until maximum while the physical pot has just 0.2 left until it reaches its
maximum. So now the virtual pot moves three times faster than the physical.

This algorithm is different than the common "picking up" up pots that you see in
Eurorack land quite a lot in such situations. We preferred our solution over that
because it seems to be more convenient — especially if you just want to change a
value just a little bit. Also it allows to have multiple virtual pots to be
selected at the same time.

By the way: in the upper example it is possible to select *none* of the pots.
That is a convenient way to reset the physical pot to the middle position so that
you always have headroom for movement left *and* right, before selecting one of
the virtual pots.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `pot` (`p`) | 0..1 | | The pot that you want to overlay, e.g. `P1.1`. |
| `bipolar` (`b`) | gate | | If this input is set to 1, the usual pot range of 0 … 1 will be mapped to −1 … +1, which converts this to a bipolar potentiometer. This is done by multiplying the output with 2.0 and substracting 1.0 afterwards. |
| `switch1 … switch8` (`s`) | gate | | These inputs select which of the virtual pots should be changed when the physical pot is being turned. These should be set to 0 or 1 (or off and on). |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output1 … output8` (`o`) | 0..1 | The output of the up to eight virtual pots. |

## See also

- [`pot`](pot.md) — the modern replacement that supersedes this circuit.
- [`button`](button.md), [`buttongroup`](buttongroup.md) — useful for managing the
  switch inputs.
