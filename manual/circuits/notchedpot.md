---
circuit: notchedpot
title: Helper circuit for pots
obsolete: true
ram_bytes: 40
manual_pages: [348]
category: controller-ui
tags: [pot, notch, center, bipolar, hemisphere, potentiometer, obsolete]
see_also: [pot, algoquencer]
impl_difficulty: easy
controller_binding: controller-required
verification: headless
spec_gap: false
difficulty_note: Obsolete (superseded by pot); a tiny pot-shaping helper (notch, bipolar, abs, left/right hemispheres) whose input is a controller pot.
verification_note: "Every output is a pure function of the pot value; sweep 0..1 and compare notch/bipolar/half outputs against a computed reference. Being obsolete, it may be skipped in favor of pot."
---

# notchedpot — Helper circuit for pots

> **⚠️ OBSOLETE.** This circuit has been superseded by the new circuit
> [`pot`](pot.md). It will be removed in the next firmware version. `pot` can do
> all `notchedpot` can do and much more, so if you use it in your patch, better
> replace it.

This little circuit simulates a potentiometer with a notch at the center. It
helps you exactly selecting the center position by defining a range that is
considered to be the center. This range is called "notch" and defaults to 10%
of the available range. You can set the size of the notch via the `notch` input.

## Example

```droid
[notchedpot]
    pot      = P1.1
    notch    = 15%
    output   = _ACTIVITY

[algoquencer]
    activity = _ACTIVITY
    ...
```

For a second use case there is the output `bipolar`. That converts a normal pot
into one with range from -1.0 to 1.0. This example also shows how to disable the
notch, if you do not need it here:

```droid
[notchedpot]
    pot      = P1.1
    notch    = 0
    bipolar = O1 # Send -10V ... +10V to O1
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `pot` (`p`) | 0..1 | | Wire your pot here, e.g. `P1.1`. |
| `notch` (`no`) | CV | `0.1` | Optionally set the notch size, if you do not like the default of `0.1`. The maximum allowed value is `0.5`. Greater values will be reduced to that. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | 0..1 | Your pot output comes here. It still goes from `0.0` to `1.0`. |
| `bipolar` (`b`) | CV | Optional output with a range from -1.0 to 1.0, where the center notch is at 0.0. |
| `absbipolar` (`ab`) | CV | A variation of `bipolar` that always outputs a positive value, i.e. the pot will go 1 … 0.5 … 0 … 0.5 … 1. |
| `lefthalf` (`l`) | CV | This output allows you to split the pot into two hemispheres. Here you get 1.0 … 0.0 while the pot is in the left half. In the middle and right of it you always get 0. |
| `righthalf` (`r`) | CV | This is the same but for the right half. It outputs 0 while the pot is in the left half and 0.0 … 1.0 from the middle to the fully right position. |
| `lefthalfinv` (`li`) | CV | This outputs 1.0 − `lefthalf`, i.e. the value range 0.0 … 1.0 … 1.0 when the pot moves left → mid → right. |
| `righthalfinv` (`ri`) | CV | This outputs 1.0 − `righthalf`, i.e. the value range 1.0 … 1.0 … 0.0 when the pot moves left → mid → right. |

## See also

- [`pot`](pot.md) — the modern replacement; does everything notchedpot does and more.
- [`algoquencer`](algoquencer.md) — example consumer of the notched pot output.
