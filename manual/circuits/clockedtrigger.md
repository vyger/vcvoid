---
circuit: clockedtrigger
title: Delay a trigger until the next clock tick
obsolete: false
ram_bytes: 40
manual_pages: [177]
category: clock-timing
tags: [trigger, clock, sync, sample-and-hold, delay, quantize, button, pending]
see_also: []
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: "Small trigger/value sample-and-hold that releases a pending trigger on the next clock tick."
verification_note: "Headless: feed trigger and clock edges and confirm the trigger/value are held then emitted once on the next clock, including the same-cycle pass-through case."
---

# clockedtrigger — Delay a trigger until the next clock tick

This is a small helper circuit that helps you synchronize a trigger with the
clock. This can for example be used to delay the press of a button until the
next clock tick or next start of a sequence.

The most simple form of operation is:

```droid
[clockedtrigger]
    clock = _CLOCK
    trigger = B1.1
```

In addition you can delay (the change of) a value together with the trigger:

```droid
[clockedtrigger]
    clock = _CLOCK
    trigger = B1.1
    clockedtrigger = _BUTTON_PRESSED
    value = P1.1
    clockedvalue = _VALUE
```

Here when the button is pressed, the value of the pot `P1.1` is read and stored
until the button is pressed and then presented at `clockedvalue`. This way you
can capture a value that is short lived and only valid while the button is
pressed.

Note: When the trigger and the clock happen at exactly the same time (the same
internal loop cycle), the trigger is immediately output and the optional `value`
input copied to its output.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `clock` (`c`) | trigger | | Each time a clock signal is seen here, a potentially pending trigger is sent to the output `clockedtrigger`. The "clock signal" does not need to be a steady clock signal. It can be any trigger source you like. |
| `trigger` (`t`) | trigger | | When a trigger appears at this input, it is stored and pending until the next trigger to `clock` is seen. At this point it is output at `clockedtrigger`. The length of the input trigger is not preserved. |
| `clear` (`cl`) | trigger | | A trigger here removes and forgets a potential previous pending trigger. The `clockedvalue` output is set to `0.0`. |
| `value` (`v`) | CV | `0.0` | This input allows you to freeze a value at the point of time when the `trigger` is seen. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `clockedtrigger` (`ct`) | trigger | A trigger is output here when a clock signal arrives and there is a pending trigger previously seen at `trigger`. This output trigger is only happening once, even if there were multiple input triggers since the last clock signal. |
| `clockedvalue` (`cv`) | CV | Here the value that has been frozen by the last trigger is sent when the next clock signal arrives. It stays the same until the next clocked trigger is output. |
| `pendingtrigger` (`p`) | gate | Here you can "peek" if a trigger is pending but not yet output. |
| `pendingvalue` (`pv`) | CV | Here you can "peek" the value that has been frozen from `value` and will be sent to `clockedvalue` when the next clock signal arrives. |
