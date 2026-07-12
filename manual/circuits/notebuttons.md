---
circuit: notebuttons
title: Note Selection Buttons
obsolete: false
ram_bytes: 136
manual_pages: [349, 350, 351]
category: controller-ui
tags: [buttons, note, selector, radio-buttons, semitone, keyboard, leds, db8e, display]
see_also: [buttongroup]
impl_difficulty: moderate
controller_binding: controller-required
verification: headless
spec_gap: false
difficulty_note: Twelve radio-style buttons with LEDs selecting a note; adds clock-quantized presses, startnote, presets and optional DB8E display.
verification_note: "Given button gate inputs, output/semitone/gate are deterministic and checkable headless; the LED indicators and DB8E note display are visual and best confirmed by a human or driven-in-Rack pass."
---

# notebuttons — Note Selection Buttons

This simple utility combines 12 buttons, just like radio buttons, into a
selector for a note such as C, C♯, D, D♯ and so on. It is similar to
[`buttongroup`](buttongroup.md), but much simpler. And it allows 12 buttons. The
output is either a number from 0 to 11 – or alternatively on a 1⁄12 V per
semitone base. The latter one is ideal for sending it to external sequencers or
quantizers as they often adopt that scheme.

The following example uses all eight buttons of the first controller plus the
first column of the second controller for selecting the twelve notes. It sends
the currently selected note to `O7` in a 1 V per octave scheme:

```droid
[notebuttons]
    button1 = B1.1
    button2 = B1.2
    button3 = B2.1
    button4 = B1.3
    button5 = B1.4
    button6 = B2.3
    button7 = B1.5
    button8 = B1.6
    button9 = B2.5
    button10 = B1.7
    button11 = B1.8
    button12 = B2.7
    led1      = L1.1
    led2      = L1.2
    led3      = L2.1
    led4      = L1.3
    led5      = L1.4
    led6      = L2.3
    led7      = L1.5
    led8      = L1.6
    led9      = L2.5
    led10     = L1.7
    led11     = L1.8
    led12     = L2.7
    semitone = O7
```

## Display

If you have a DB8E display controller, `notebuttons` automatically displays the
selected note in the display.

The title of the display is derived from the target of the `output` parameter
or, if that is not patched, from `semitone`. You can use `header` to use your
own title instead.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `button1 … button12` (`b`) | gate | | Wire 12 buttons to these 12 inputs. |
| `clock` (`c`) | trigger | | When you use this jack, all button presses are quantized in time to the next clock pulse arriving here. That makes it easier to switch the note exactly in time. |
| `startnote` (`sn`) | integer | | Specify the note that should be selected when the Droid starts and no state is loaded, or when a trigger to `clear` or `clearall` happened. This is an integer number from 0 to 11. |
| `display` (`dy`) | integer | `1` | This parameter selects the number of the DB8E display controller, where any change of this circuit's state will show up. Per default, the first DB8E available is used. Set this to 2 if you have a second DB8E and want to use that. Note: any other controller is not being counted here, it's really just the number of the DB8E as counted from the master module. Set this to 0 to suppress any display output from this circuit. |
| `header` (`hr`) | text | ☞ smart | This text is displayed at the top of the DB8E display whenever this circuit shows up in the display. If you omit this, an automatic title is used. For example if the output of this circuit is fed into an internal patch cable, the name of this cable is displayed. Setting `header = ""` or `header = 0` removes the header and just displays the actual value. |
| `select` (`s`) | integer | ☞ smart | The `select` input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to 1). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | ☞ smart | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is 0, the circuit will be active if `select` is exactly 0 instead of a positive gate signal. In some cases this is more convenient. |
| `preset` (`pr`) | integer | ☞ smart | This is the preset number to save or to load. Note: the first preset has the number 0, not 1! For the whole story on presets please refer to the manual ([basics](../basics.md)). This circuit has 16 presets, so this number ranges from 0 to 15. |
| `loadpreset` (`lp`) | trigger | | A trigger here loads a preset. As a speciality you can use the trigger for selecting a preset at the same time. |
| `savepreset` (`sp`) | trigger | | A trigger here saves a preset. |
| `clear` (`cl`) | trigger | | A trigger here loads the default start state into the circuit. The presets are not affected, unless you use direct preset switching with the `preset` input and without triggers. And that case the current preset is also cleared. |
| `clearall` (`ca`) | trigger | | A trigger here loads the default start state into the circuit and into all of its presets. |
| `dontsave` (`dos`) | gate | `0` | If you set this to 1, the state of the circuit will not saved to the SD card and not loaded from the SD card when the Droid starts. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `led1 … led12` (`l`) | gate | Wire the LEDs in the buttons to these 12 outputs. |
| `output` (`o`) | integer | Here you get a number from 0 to 11, according to the currently selected button. |
| `semitone` (`st`) | pitch | Here you get the same as `output`, but divided by 120. When you patch this output to a CV output of the DROID, like `O1`, it will output the note as a semitone on a 1 V per octave scheme. |
| `gate` (`g`) | gate | This output is 1 as long as one of the buttons is held. You can use that together with the `semitone` output to use the `notebuttons` as a CV/gate keyboard with 12 keys. |

## See also

- [`buttongroup`](buttongroup.md) — a more general radio-button style selector.
