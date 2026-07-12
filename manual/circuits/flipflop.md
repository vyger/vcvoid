---
circuit: flipflop
title: Simple flip flop
obsolete: false
ram_bytes: 40
manual_pages: [259]
category: logic-math
tags: [flip flop, toggle, set, reset, clock divider, bit, latch]
see_also: [button]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Single-bit flip flop with toggle/set/reset/clear/load triggers; trivial deterministic state machine.
verification_note: "Drive trigger sequences and assert the output bit headlessly; toggle-as-clock-divider-by-two is a simple deterministic check."
---

# flipflop — Simple flip flop

This circuit implements a flip flop that stores one bit, which can be manipulated
with various triggers.

## Example

Here is a simple example for a flip flop that toggles at each trigger. Fun fact:
this implements a clock divider by two:

```droid
[flipflop]
    toggle = I1
    output = O1
```

As an alternative you can work with `set` and `reset` triggers:

```droid
[flipflop]
    set = I1
    reset = I2
    output = O1
```

Note: The flip flop does not save its state to the SD card. And it has no
presets. If you need any of these, have a look at [`button`](button.md).

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `toggle` (`t`) | trigger | | A trigger here inverts the state of the flip flop. It changes 0 to 1 and 1 to 0. |
| `set` (`s`) | trigger | | Sets the flip flop to 1. |
| `reset` (`r`) | trigger | | Sets the flip flop to 0. |
| `clear` (`cl`) | trigger | | Sets the flip flop to the value defined by `startvalue`. |
| `startvalue` (`sv`) | gate | `0` | The flip flop starts its live with this value. Also `clear` will set the flip flop to this value. |
| `load` (`ld`) | trigger | | Loads the value into the flip flop that's defined with `loadvalue`. |
| `loadvalue` (`lv`) | gate | `1` | Value to set the flip flop to, when `load` is triggered. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | gate | Outputs the current value of the flip flop: either 0 or 1. |

## See also

- [`button`](button.md) — a button/toggle that supports presets and state saving.
