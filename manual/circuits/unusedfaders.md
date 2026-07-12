---
circuit: unusedfaders
title: Declare unused motor faders
obsolete: false
ram_bytes: 32
manual_pages: [406]
category: controller-ui
tags: [motor-fader, unused, disable, select, menu, faderbank]
see_also: [faderbank]
impl_difficulty: easy
controller_binding: controller-required
verification: requires-human
spec_gap: false
difficulty_note: Trivial config circuit; declares a range of M4 motor faders as unused so they move to the bottom and go dark, with the usual select/selectat overlay gating.
verification_note: "No signal outputs, so behaviour is only observable as motor-fader position and LED state on an M4 controller. Requires a human (or manual visual check of the emulated fader widget) to confirm the faders drop and darken."
---

# unusedfaders — Declare unused motor faders

The circuit disables motor faders that are not used in certain situations. Those
faders move to the bottom and stay there. Otherwise faders, that are currently
not selected, would keep their old position and LED state. This can be confusing
to the user.

Usage: If you have a situation where not all of the faders are selected with an
active `select`, add a `unusedfaders` circuit for the ununsed faders and make
sure that they are selected.

## Example

The following example disables the faders 6, 7 and 8 while `_MENU_ACTIVE` is `1`:

```droid
[unusedfaders]
    select = _MENU_ACTIVE
    firstfader = 6
    numfaders = 3
```

Note: You could do the same with [`faderbank`](faderbank.md), but that circuit
needs more memory.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `firstfader` (`f`) | integer | `1` | The number of the first unused motor fader motor. |
| `numfaders` (`n`) | integer | `1` | The number of unused faders. |
| `select` (`s`) | integer | ☞ smart | The `select` input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to `1`). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | ☞ smart | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is `0`, the circuit will be active if `select` is *exactly* `0` instead of a positive gate signal. In some cases this is more conventient. |

## See also

- [`faderbank`](faderbank.md) — can achieve the same effect but uses more memory.
