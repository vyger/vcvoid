---
circuit: select
title: Copy a signal if selected
obsolete: false
ram_bytes: 24
manual_pages: [381]
category: switch-routing
tags: [select, copy, led, menu-page, gate, overlay, selectat]
see_also: [copy]
impl_difficulty: easy
controller_binding: controller-enhanced
verification: headless
spec_gap: false
difficulty_note: Copies input to output only while selected (leaving the target register free otherwise) — trivial logic, but its purpose is UI overlays so it is typically wired to button/LED registers.
verification_note: "Headless: assert output follows input when select/selectat matches, and leaves the register untouched (available to other circuits) otherwise."
---

# select — Copy a signal if selected

Copies a value just when the circuit is selected via `select`.

This solves the problem of having an LED displaying something, but just when a
certain "menu page" or similar is active. Simply setting the LED with
[`copy`](copy.md) or some other circuit's output will always set it. Checking
some select state and sending 0 does not help, since it will override any other
circuit's values for the LED even when those are selected.

## Example

Here is an example of letting the LED `L1.1` flash when `_SELECTED` is high, and
otherwise **don't copy anything** to the LED:

```droid
[lfo]
    output = _FLASH

[select]
    select = _SELECT
    input = _FLASH
    output = L1.1
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | CV | `0.0` | Connect the signal you want to copy. |
| `select` (`s`) | integer | ☞ smart | The select input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to 1). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | ☞ smart | This input makes the select input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is 0, the circuit will be active if `select` is exactly 0 instead of a positive gate signal. In some cases this is more convenient. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | The input will be copied here, but just when the circuit is selected via `select`. |

## See also

- [`copy`](copy.md) — copies a signal unconditionally.
