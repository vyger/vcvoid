---
circuit: encoderbank
title: Create bank of up to 8 virtual input knobs from E4 encoders
obsolete: false
ram_bytes: 744
manual_pages: [208, 209, 210, 211, 212]
category: controller-ui
tags: [encoder, e4, virtual-knob, led-ring, db8e, display, notch, snapto, discrete, autozoom, preset, select, bipolar]
see_also: [encoder, copy, button]
impl_difficulty: moderate
controller_binding: controller-required
verification: requires-human
spec_gap: false
difficulty_note: Same virtual-encoder engine as encoder applied to up to 8 encoders at once (minus movement triggers, override, sharewithnext); breadth of shared parameters over hardware encoders.
verification_note: "Requires-human: per-encoder value logic (modes, discrete, snapto, presets) is headless-testable with injected steps, but the LED rings and turn-feel across the bank are visual/haptic and need human comparison to hardware. Depends on emulating the E4/DB8E encoders."
---

# encoderbank — Create bank of up to 8 virtual input knobs from E4 encoders

This circuit makes your life easier if you deal with lots of encoders in a
larger setup. Instead of using a single [`encoder`](encoder.md) circuit for
every virtual knob, you can handle up to eight encoders with just one circuit.

The `encoderbank` circuit shares almost all features with the single
[`encoder`](encoder.md) circuit, except for:

- `movedup`, `moveddown` and `valuechanged` triggers
- The `override` input
- The `sharewithnext` functionality

Please read the manual of the [`encoder`](encoder.md) circuit for all the
details and examples of the features. We don't repeat them here for sake of
tersity.

Example of a definition of 3 encoders:

```droid
[encoderbank]
    firstencoder = E2.1
    output1 = O1
    output2 = O2
    output3 = O3
    outputscale = 2V
```

## Defining the number of encoders

This circuit has no setting for the number of encoders it controls. This number
is automaticall defined by the number of outputs, leds or buttons you use. More
precisely, it is the highest `outputX` or `ledX` or `buttonX` you use, whichever
is higher. In the upper example, `output3` is the highest connected output
number, so the circuit controls three encoders, starting from `firstencoder`.

## Display

If you have a DB8E display controller (see the manual
([hardware](../hardware.md))), the `encoderbank` circuit automatically updates
the display whenever you turn one of the encoders. It then shows the updated
value of `outputX` of that encoder in the display.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `firstencoder` (`e`) | integer | `1` | The first encoder to use. You can either use it's register name, like `E8.2` for encoder 2 on controller 8. As an alternative you can use a number like 6. That would specify the 6th encoder of your setup: the encoder number 2 on your second E4. For each of the `output` jacks you use, one encoder is used, following the order of your controllers. This value is read just once when the DROID starts. Making this parameter dynamic does not work. |
| `led1 … led8` (`l`) | `0..1` | | You can use the rings of LEDs around the encoders as virtual LEDs using this parameter. This is similar to using the according `L` registers of the E4, but honors the `select` input. |
| `startvalue` (`sv`) | CV | `0.0` | This sets the value the encoder gets when you start this circuit for the first time or when you send trigger to `clear`. Note: the range of this value refers to the situation before `outputscale` and `outputoffset` is applied. So if `mode` is unused or at 0, a startvalue of 0.5 sets the encoder's virtual value exactly to the center — regardless of any scaling or offsetting that happens afterwards. |
| `notch` (`no`) | CV | `0.0` | This parameter helps you to dial in exactly the center of the selected range, which is 0.5 in normal mode and 0.0 in bipolar mode. The value of `notch` specifies the portion of one complete 360 cycle of the pot during which the center position should be assumed. 0.1 is probably a good value. Notch does not work if `mode` selects positive or negative infinity. |
| `outputscale` (`os`) | CV | `1.0` | The output is multiplied by this value. This is just for convenience and may save a [`copy`](copy.md) circuit in some situations. |
| `outputoffset` (`oo`) | CV | `0.0` | After scaling the virtual value with `outputscale`, this value is being added before sending the result to the output. |
| `mode` (`m`) | integer | `1` | Selects the possible range of the virtual value. This setting is ignored if `discrete` is in use. Note: The mode 0 is for situations where encoders are overlayed with `select` and an encoder is unused. Setting `mode = 0` can be used to disable this encoder and blank its LEDs. See the mode table below. |
| `smooth` (`sm`) | CV | `0.5` | Unlike a potentiometer, an encoder does not output continous values but steps. If you directly wire the output of an encoder to a CV input of an audio module, the steps might become audible. Therefore the final values of the encoder are smoothed out by a simulation of a low pass filter. That's essentially the same as in the `slew` circuit. The smoothing is enabled per default but you can change it with this parameter. A value of `0.0` turns off smoothing and you get access to the tiny steps of the encoder. `1.0` selects a maximum smoothing, which has also the effect that fast turns of the encoder are slowed down a bit. The default value of `0.5` does just a mild slew limiting. If you use `discrete`, the smoothing is not applied. |
| `discrete` (`d`) | integer | `0` | Set this to an integer number of `2` or higher to enable discrete mode. In this mode the encoder works like a rotary switch for selecting one of the numbers 0, 1, 2 and so on. The number you set for `discrete` selects the number of positions in this "switch". For example `discrete = 4` produces the values 0, 1, 2 or 3. In this mode the inputs `notch`, `mode` and `smooth` are ignored. |
| `snapto` (`sn`) | CV | ☞ smart | Use this parameter to define a position where the encoder value automatically returns to if it is not turned. This behaves a bit like a pitch bend wheel. You can get crazy CV modulations if you use a dynamic CV for `snapto`, such as the output of an LFO. The encoder's value will try to follow the LFO but you can still turn the encoder and work "against" the LFO. This mechanism also works if the encoder is not selected. |
| `snapforce` (`sf`) | CV | `0.5` | Specifies the speed or "force" with that the encoder moves back to the `snapto` position if that is used. A force of `0.0` deactivates `snapto`. |
| `sensivity` (`se`) | CV | `1.0` | The sensivity determines how far you need to turn the knob to get which amout of value change. Per default one turn of 360 degrees changes to the value from 0.0 to 1.0. A `sensivity` of 2, doubles the speed of change, 0.5 slows it down to the half. If you use `mode` to enable infinity, there is no total range. In this case one turn changes the value by `sensivity`. If you use `discrete`, one turn of the knob changes the virtual switch by eight positions, if `sensitivity` is at 1.0, and accordingly faster or slower if you change that. At this point we must apologize for the fact that this parameter has been illnamed. Sensivity is not an existing English word. It meant to be sensitivity. But fixing this would render many existing patches invalid and impose despair and annoinance on all our beloved users, so we decided to leave it as it is. |
| `autozoom` (`a`) | `0..1` | `0.0` | The "auto zoom" feature allows you to fine adjust values when turning the knob slowly and coarse adjust when you turn it fast. If `autozoom` is at the maximum value of `1.0`, turning the knob just slowly changes the value by super tiny amounts, while turning it fast operates way faster than usual. Use any value between `0.0` and `1.0` for `autozoom` to select the level of this slowing down for slow movements. `autozoom` has no effect if `discrete` is used. |
| `color` (`co`) | CV | ☞ smart | Color of the pointer in the LED ring. See the color table below for some example color values. |
| `negativecolor` (`nc`) | CV | ☞ smart | If you use this parameter, it defines the color of the LEDs in case the current logical value is negative. |
| `ledfill` (`lf`) | integer | `1` | Selects whether the LED ring displays the current value with just a single colored dot (`ledfill = 0`) or by additionally illuminating all LEDs between 0 and the current value in half brightness (`ledfill = 1`). |
| `display` (`dy`) | integer | `1` | This parameter selects the number of the DB8E display controller, where any change of this circuit's state will show up. Per default, the first DB8E available is used. Set this to 2 if you have a second DB8E and want to use that. Note: any other controller is not being counted here, it's really just the number of the DB8E as counted from the master module. Set this to 0 to suppress any display output from this circuit. |
| `header` (`hr`) | text | ☞ smart | This text is displayed at the top of the DB8E display whenever this circuit shows up in the display. If you omit this, an automatic title is used. For example if the output of this circuit is fed into an internal patch cable, the name of this cable is displayed. Setting `header = ""` or `header = 0` removes the header and just displays the actual value. |
| `select` (`s`) | integer | ☞ smart | The `select` input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to 1). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | ☞ smart | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is 0, the circuit will be active if `select` is exactly 0 instead of a positive gate signal. In some cases this is more conventient. |
| `preset` (`pr`) | integer | ☞ smart | This is the preset number to save or to load. Note: the first preset has the number 0, not 1! For the whole story on presets please refer to the manual ([basics](../basics.md)). This circuit has 8 presets, so this number ranges from 0 to 7. |
| `loadpreset` (`lp`) | trigger | | A trigger here loads a preset. As a speciality you can use the trigger for selecting a preset at the same time. |
| `savepreset` (`sp`) | trigger | | A trigger here saves a preset. |
| `clear` (`cl`) | trigger | | A trigger here loads the default start state into the circuit. The presets are not affected, unless you use direct preset switching with the `preset` input and without triggers. And that case the current preset is also cleared. |
| `clearall` (`ca`) | trigger | | A trigger here loads the default start state into the circuit and into all of its presets. |
| `dontsave` (`dos`) | gate | `0` | If you set this to 1, the state of the circuit will not saved to the SD card and not loaded from the SD card when the Droid starts. |

### `mode` values

| Value | Meaning |
|-------|---------|
| 0 | Off: the encoder is unsed, its LEDs are off |
| 1 | Normal mode: fixed range between `0.0` and `1.0` |
| 2 | Bipolar mode: fixed range between `-1.0` and `1.0` |
| 3 | Positive infinity: open range between `0.0` and ∞ |
| 4 | Negative infinity: open range between −∞ and `0.0` |
| 5 | Bipolar infinity: open range between −∞ and ∞ |
| 6 | Circular infinity: range is `0.0` … `1.0`, but repeats over in both directions |

### `color` example values

| Value | Color |
|-------|-------|
| 0.2 | cyan |
| 0.4 | green |
| 0.6 | yellow |
| 0.73 | orange |
| 0.8 | red |
| 1.0 | magenta |
| 1.1 | violet |
| 1.2 | blue |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output1 … output8` (`o`) | CV | Output the current value if the virtual encoder value (don't use this if you are using `sharewithnext`). |
| `button1 … button8` (`b`) | gate | This outputs provides you with the current states of the push buttons in the encoders, but only if the circuit is selected. While you could do this with an extra [`button`](button.md) circuits, using this output is more convenient in some situations. |

## See also

- [`encoder`](encoder.md) — the single-encoder version; read it for the full feature details.
- [`copy`](copy.md) — for scaling/offsetting values.
- [`button`](button.md) — for reading the encoder push buttons separately.
