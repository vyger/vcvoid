---
circuit: matrixmixer
title: Matrix mixer for CVs
obsolete: false
ram_bytes: 176
manual_pages: [279, 280, 281]
category: mixer-cv
tags: [matrix, mixer, cv, buttons, leds, unity-gain, maximum, cascade, presets]
see_also: [mixer, crossfader]
impl_difficulty: moderate
controller_binding: controller-enhanced
verification: headless
spec_gap: false
difficulty_note: Mix/max math is trivial, but 16 toggle-button states, mixmax blending, startvalue modes and 16 presets add stateful breadth.
verification_note: "Feed gate inputs and compare output/max sums against a computed reference; toggle-state, mixmax weighting and preset recall are all deterministic. Buttons/LEDs are typed as gate/0..1 jacks so they work master-only, but are intended for a controller's buttons+LEDs."
---

# matrixmixer — Matrix mixer for CVs

This circuit is a 4×4 matrix mixer with four inputs and four outputs that is
operated by push buttons. Each of the 16 matrix nodes has a toggle button for
adding or removing one specific input to or from one specific output. The mixing
is always done with unity gain. This means that each output is the sum of all
inputs that are enabled on its path.

The following picture shows a matrix with the four inputs `I1 … I4` and the four
outputs `O1 … O4`. As you can see the button 23 mixes input 2 to output 3.

If you have not pushed any buttons yet, the mixer enables four buttons in a
diagonal so that inputs `I1` is connected to output `O1` and so on:

```
      11    12    13    14
I1 →  ■  →  □  →  □  →  □
I2 →  □  →  ■  →  □  →  □   (21 22 23 24)
I3 →  □  →  □  →  ■  →  □   (31 32 33 34)
I4 →  □  →  □  →  □  →  ■   (41 42 43 44)
      ↓     ↓     ↓     ↓
      O1    O2    O3    O4
```

As an alternative operation, instead of summing the enabled signals you can
compute the `maximum` signal. This is useful when combining envelope signals –
e.g. from different rhythmic patterns. Adding envelope signals would either make
them "too loud" or even distort them.

The current state of the sixteen buttons is saved in the DROID's internal flash
memory.

Of course it is possible to use a smaller part of the matrix, e.g. just 3×2,
simply by not patching the according inputs, outputs and buttons. Here is an
example of a 3×2 mixer:

```droid
[matrixmixer]
    input1    = I1
    input2    = I2
    input3    = I3
    output1 = O1
    output2 = O2
    button11 = B1.1
    button12 = B1.2
    button21 = B2.1
    button22 = B1.3
    button31 = B1.4
    button32 = B2.3
    led11     = L1.1
    led12     = L1.2
    led21     = L2.1
    led22     = L1.3
    led31     = L1.4
    led32     = L2.3
```

This matrix looks like this:

```
      11    12
I1 →  ■  →  □
I2 →  □  →  ■   (21 22)
I3 →  □  →  □   (31 32)
      ↓     ↓
      O1    O2
```

## Mixers with more inputs / outputs

The four auxiliary inputs `auxin1 … auxin4` can be used to create matrix mixers
with more than four inputs. You can create a mixer with 8 inputs and 4 outputs
by sending the four outputs of one matrix mixer into the four auxiliary inputs
of a second one.

If you want to create a mixer with more than 4 outputs then simply use several
mixers and feed the same inputs to all of them.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input1 … input4` (`i`) | CV | `0.0` | The up to four CV inputs that you want to mix. |
| `auxin1 … auxin4` (`a`) | CV | | These auxiliary inputs will be mixed directly into the four outputs `output1 … output4` and are used for cascading several matrix mixers into one with more than four inputs. |
| `mixmax` (`m`) | 0..1 | `0.0` | If this is `0.0`, normal mixing is done (the enabled inputs CVs will be added). At a value of `1.0` instead each outputs is the maximum of the enabled inputs. Any number in between will create a weighted average between these two values. |
| `startvalue` (`sv`) | integer | `1` | This input selects in which state the matrix begins life. Also a trigger to `clear` will create that starting state. The following three configurations can be selected with `startvalue`: `0` = All buttons are cleared. `1` = The buttons on the diagonal are active. `2` = All buttons are set. When set to 1, input1 is sent to output1, input2 to output2 and so on. |
| `button11 … button14` (`b1`) | gate | | These four buttons decide, to which of the four outputs `input1` is being mixed. |
| `button21 … button24` (`b2`) | gate | | These four buttons decide, to which of the four outputs `input2` is being mixed. |
| `button31 … button34` (`b3`) | gate | | These four buttons decide, to which of the four outputs `input3` is being mixed. |
| `button41 … button44` (`b4`) | gate | | These four buttons decide, to which of the four outputs `input4` is being mixed. |
| `select` (`s`) | integer | ☞ smart | The `select` input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to `1`). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | ☞ smart | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is 0, the circuit will be active if `select` is exactly 0 instead of a positive gate signal. In some cases this is more conventient. |
| `preset` (`pr`) | integer | ☞ smart | This is the preset number to save or to load. Note: the first preset has the number `0`, not `1`! For the whole story on presets please refer to the manual ([basics](../basics.md)). This circuit has 16 presets, so this number ranges from `0` to `15`. |
| `loadpreset` (`lp`) | trigger | | A trigger here loads a preset. As a speciality you can use the trigger for selecting a preset at the same time. |
| `savepreset` (`sp`) | trigger | | A trigger here saves a preset. |
| `clear` (`cl`) | trigger | | A trigger here loads the default start state into the circuit. The presets are not affected, unless you use direct preset switching with the `preset` input and without triggers. And that case the current preset is also cleared. |
| `clearall` (`ca`) | trigger | | A trigger here loads the default start state into the circuit and into all of its presets. |
| `dontsave` (`dos`) | gate | `0` | If you set this to 1, the state of the circuit will not saved to the SD card and not loaded from the SD card when the Droid starts. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output1 … output4` (`o`) | CV | The four outputs. |
| `led11 … led14` (`l1`) | 0..1 | The LEDs in the buttons `button11 … button14`. |
| `led21 … led24` (`l2`) | 0..1 | The LEDs in the buttons `button21 … button24`. |
| `led31 … led34` (`l3`) | 0..1 | The LEDs in the buttons `button31 … button34`. |
| `led41 … led44` (`l4`) | 0..1 | The LEDs in the buttons `button41 … button44`. |
