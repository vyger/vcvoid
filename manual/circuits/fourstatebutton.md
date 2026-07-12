---
circuit: fourstatebutton
title: Button switching through 4 states
obsolete: true
ram_bytes: 40
manual_pages: [262]
category: controller-ui
tags: [button, state, switch, toggle, led, octave, obsolete]
see_also: [button, togglebutton]
impl_difficulty: easy
controller_binding: controller-required
verification: requires-human
spec_gap: false
difficulty_note: Obsolete (superseded by button); cycles a button through 4 states selecting value1..4, with a 4-level LED. Simple state logic but needs a controller button/LED.
verification_note: "State-advance and value-selection logic is deterministic and testable via cable-driven button input, but the 4-brightness LED feedback requires human visual inspection."
---

# fourstatebutton — Button switching through 4 states

> **⚠️ OBSOLETE.** This circuit has been superseded by the new circuit
> [`button`](button.md). `button` can do all `fourstatebutton` can do and much
> more. So `fourstatebutton` will be removed soon.

This circuit converts one of the push buttons of your controllers into a button
that switches through up to four different states. This is very similar to
[`togglebutton`](togglebutton.md) but that supports just two states.

The LED will be off in state 1, 100% bright in state 4 and somewhere in between
in the other two states.

The use case is to have a way to manually switch through three or four options.
The following example implements an octave switch for a VCO. The button steps
you through the sequence 0 → 1 → 2 → 3 → 0 octaves. The pitch is being read from
`I1` and output again at `O1` – possibly shifted by up to 3 octaves (3 V).

```droid
[fourstatebutton]
    button = B1.1
    led    = L1.1
    value1 = I1 + 0V
    value2 = I1 + 1V
    value3 = I1 + 2V
    value4 = I1 + 3V
    output = O1
```

Of course the values need not be fixed values. The next examples shows you a
DROID patch where the button is used to cycle through four different wave forms
of an LFO and send that to output `O1`:

```droid
[lfo]
    hz       = 2
    square   = _W1
    triangle = _W2
    sawtooth = _W3
    sine     = _W4

[fourstatebutton]
    button   = B1.1
    led      = L1.1
    value1   = _W1
    value2   = _W2
    value3   = _W3
    value4   = _W4
    output   = O1
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `button` (`b`) | gate | | The button. |
| `reset` (`r`) | trigger | | A positive trigger here will reset the button to the first state. |
| `value1 ... value4` (`v`) | CV | | The values that `output` should get when the four various states are active. |
| `startvalue` (`sv`) | integer | | By setting this to 0, 1, 2 or 3 you set the initial state of the button when the DROID is powered up to state 1, 2, 3 or 4. It also disabled the automatic saving of the button's state in the DROID's internal flash memory. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | Depending on the current state of the button here the value of `value1`, `value2`, `value3` or `value4` will be copied. |
| `led` (`l`) | `0..1` | The LED in the button. |

## See also

- [`button`](button.md) — the replacement circuit; does everything this one does and more.
- [`togglebutton`](togglebutton.md) — the two-state equivalent.
