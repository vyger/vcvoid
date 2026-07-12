---
circuit: faderbank
title: Create multiple virtual faders in M4 controller
obsolete: false
ram_bytes: 688
manual_pages: [247, 248, 249]
category: controller-ui
tags: [motor fader, m4, virtual faders, bank, notches, presets, db8e, touch button, led]
see_also: [motorfader]
impl_difficulty: hard
controller_binding: controller-required
verification: requires-human
spec_gap: false
difficulty_note: Bank of up to 16 virtual motor faders on the M4 with notches, presets, and per-fader LEDs; motorized haptic notches have no VCV analog.
verification_note: "Value logic (notch quantization, presets, startvalue, clear) is partially headless-testable, but motor-fader position feedback, touch buttons, and LED brightness require human interaction; gap is hardware, not docs."
---

# faderbank — Create multiple virtual faders in M4 controller

This circuit is very similar to [`motorfader`](motorfader.md) but controls up to
16 faders at once. Its purpose is to reduce the number of `motorfader` circuits
in situations where you control banks or arrays of parameters in a similar way.
It does not add any extra functionality to `motorfader`.

That being said, it is easiest to just show the differences to a single
`motorfader` circuit. And these are:

- Instead of `fader` you set `firstfader` to specify which faders you want to
  control. The number of faders does not need to be set since it corresponds to
  the number of output jacks you use.
- Instead of `output` you have `output1`, `output2` and so on. This determines
  the number of faders that are controlled by this circuit.
- The parameters `notches` and `ledcolor` are common for all controlled faders.
  They are identical as those in `motorfader`.
- The parameters `ledvalue1`, `ledvalue2`, ... can set the brightness of the
  individual LEDs below the faders.
- Because of memory limitations you only have 6 presets (`motorfader` has 8).

## Example

Here is an example of a fader bank of the three faders 3, 4 and 5 (spreading
over two M4s). We use a pot to select one of six presets (from 0 to 5). Turning
the pot will immediately switch the preset (and the faders will move
accordingly). And the CVs will be sent to outputs O1, O2 and O3:

```droid
[p2b8]
[m4]

[faderbank]
    preset = P1.1 * 6
    output1 = O1
    output2 = O2
    output3 = O3
```

## Display

If you have a DB8E display controller, the `faderbank` circuit automatically
updates the display whenever you move one of the faders.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `firstfader` (`f`) | integer | `1` | First M4 fader of the virtual fader bank (starting with 1). |
| `notches` (`n`) | integer | | Number of artificial notches. `0` disables the notches. `1` creates a pitch bend wheel. `2` creates a binary switch with the output values 0 and 1. Higher number create that number of notches. E.g. `8` creates eight notches and `output` will output one of the value 0, 1, … 8. The maximum number of notches is 201. But if you select more than 25 notches, the force feedback is turned off as the notches would get too small to work. |
| `startvalue` (`sv`) | CV | `0.0` | This sets the value the faders should get when the circuit starts for the first time or when you send a trigger to `clear`. |
| `ledcolor` (`lc`) | CV | | When you use this input, it will set the color of the LED below the faders, when the circuit is selected. If the LED is off, this setting has no impact. |
| `ledvalue1 … ledvalue16` (`lv`) | CV | | When you use this input, it will override the brightness of the LEDs below the faders, but just when this circuit is selected. |
| `display` (`dy`) | integer | `1` | This parameter selects the number of the DB8E display controller, where any change of this circuit's state will show up. Per default, the first DB8E available is used. Set this to 2 if you have a second DB8E and want to use that. Note: any other controller is not being counted here, it's really just the number of the DB8E as counted from the master module. Set this to 0 to suppress any display output from this circuit. |
| `header` (`hr`) | text | ☞ smart | This text is displayed at the top of the DB8E display whenever this circuit shows up in the display. If you omit this, an automatic title is used. For example if the output of this circuit is fed into an internal patch cable, the name of this cable is displayed. Setting `header = ""` or `header = 0` removes the header and just displays the actual value. |
| `select` (`s`) | integer | ☞ smart | The select input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to 1). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | ☞ smart | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is 0, the circuit will be active if `select` is exactly 0 instead of a positive gate signal. In some cases this is more convenient. |
| `preset` (`pr`) | integer | ☞ smart | This is the preset number to save or to load. Note: the first preset has the number 0, not 1! For the whole story on presets please refer to the manual ([basics](../basics.md)). This circuit has 6 presets, so this number ranges from 0 to 5. |
| `loadpreset` (`lp`) | trigger | | A trigger here loads a preset. As a speciality you can use the trigger for selecting a preset at the same time. |
| `savepreset` (`sp`) | trigger | | A trigger here saves a preset. |
| `clear` (`cl`) | trigger | | A trigger here loads the default start state into the circuit. The presets are not affected, unless you use direct preset switching with the `preset` input and without triggers. And that case the current preset is also cleared. |
| `clearall` (`ca`) | trigger | | A trigger here loads the default start state into the circuit and into all of its presets. |
| `dontsave` (`dos`) | gate | `0` | If you set this to 1, the state of the circuit will not saved to the SD card and not loaded from the SD card when the Droid starts. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output1 … output16` (`o`) | CV | The current values of the virtual motor faders are output here. |
| `button1 … button16` (`b`) | gate | Outputs the current value of the touch buttons of the faders to these output, when this circuit is selected. When the circuit is not selected, 0 is output. |

## See also

- [`motorfader`](motorfader.md) — the single-fader equivalent this circuit is based on.
