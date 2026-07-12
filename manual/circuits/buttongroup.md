---
circuit: buttongroup
title: Connected buttons
obsolete: false
ram_bytes: 440
manual_pages: [160, 161, 162, 163]
category: controller-ui
tags: [buttons, radio-buttons, led, user-interface, toggle, select, layers, overlay, preset]
see_also: [button, minifonion, arpeggio]
impl_difficulty: moderate
controller_binding: controller-required
verification: rack-automated
spec_gap: false
difficulty_note: "Radio-button group logic with min/max active counts, per-button values, overlay select and presets across up to 32 buttons."
verification_note: "Rack-automated: drive button events from an emulated controller and read output/led/buttonoutput registers; selection logic is deterministic, though the ~25 ms burst-detection window needs timing-aware checks."
---

# buttongroup — Connected buttons

This utility circuit combines a number of push buttons into a group that behave
as a unit. One classic operation is to form a group of "radio buttons". This
means that at any time just one of these buttons is on and all others are off.

This circuit is designed to build user interfaces. It is executed at a lower
speed. Don't use it for other purposes.

## Selecting one of several values

The following example uses four buttons for selecting one of the voltages 0 V,
1 V, 2 V and −1 V. This voltage is then being sent to the output jack. This
could be used as an octave switch or the like. The four buttons `B2.1 … B2.4`
are grouped in a way that just one button is on and the others are off. The four
selectable voltages are assigned to one button each. The value of the currently
active button is being sent to the output. The outputs `output1 … output4` will
be set to 1 if their corresponding button is active and are used for controlling
the LEDs within the buttons.

```droid
[buttongroup]
    button1   = B2.1
    button2   = B2.2
    button3   = B2.3
    button4   = B2.4
    led1      = L2.1 # LED in button 2.1
    led2      = L2.2
    led3      = L2.3
    led4      = L2.4
    value1    = 0V
    value2    = 1V
    value3    = 2V
    value4    = -1V
    output    = O1
```

If you set `maxactive` to a number greater than one, more than one button can be
active at the same time. If this is the case then the sum of the values of all
active buttons will be sent to the output. Here is an example, where three
buttons are being used for selecting a number between 0 and 7 by selecting any
combination of the buttons "1", "2", and "4".

```droid
[buttongroup]
    button1   = B2.1
    button2   = B2.2
    button3   = B2.3
    led1      = L2.1 # LED in button 2.1
    led2      = L2.2
    led3      = L2.3
    value1    = 1
    value2    = 2
    value3    = 4
    minactive = 0 # allow all buttons to be off
    maxactive = 3 # allow all buttons to be on
    output    = O1
```

## Overlaying buttons

When you make more complex DROID patches, it's likely that you might run out of
buttons. In such a situation you can overlay buttons with multiple functions and
use other buttons to switch between these layers.

Consider the following example: We have one P2B8 controller. The buttons 1 and 2
should switch between the layers *root note* and *scale*. We do this with a
simple button group (you could also use a [`button`](button.md) circuit and save
one button, but for simplicity we allow us two here):

```droid
[p2b8]

[buttongroup]
    button1 = B1.1
    button2 = B1.2
    led1    = L1.1
    led2    = L1.2
```

The remaining six buttons select either one of six possible root notes or one of
six possible scales (adhering to the scheme of the [`minifonion`](minifonion.md)
circuit). Please note how we have added a `select` input at each of both circuits
to make sure that at any given time exactly one of the two groups is selected:

```droid
[buttongroup]
    select = L1.1 # be active only when L1.1 is active
    button1 = B1.3
    button2 = B1.4
    button3 = B1.5
    button4 = B1.6
    button5 = B1.7
    button6 = B1.8
    led1 = L1.3
    led2 = L1.4
    led3 = L1.5
    led4 = L1.6
    led5 = L1.7
    led6 = L1.8
    value1 = 0 # C
    value2 = 2 # D
    value3 = 5 # F
    value4 = 7 # G
    value5 = 9 # A
    value6 = 10 # Bb
    output = _ROOT
```

```droid
[buttongroup]
    select = L1.2 # be active only when L1.2 is active
    button1 = B1.3
    button2 = B1.4
    button3 = B1.5
    button4 = B1.6
    button5 = B1.7
    button6 = B1.8
    led1 = L1.3
    led2 = L1.4
    led3 = L1.5
    led4 = L1.6
    led5 = L1.7
    led6 = L1.8
    value1 = 1 # major
    value2 = 6 # dorian minor
    value3 = 7 # natural minor
    value4 = 9 # phrygian minor
    value5 = 10 # diminished scale
    value6 = 2 # mixolydian
    output = _DEGREE
```

Here you can patch `_ROOT` and `_SCALE` to some [`minifonion`](minifonion.md),
[`arpeggio`](arpeggio.md) or other circuit that works with scales.

Now, with the top buttons you can switch between root and scale selection and
with the remaining six buttons select either the root or the scale.

## Display

In most cases `buttongroup` does not use the display of a DB8E controller. This
is because it would not make much sense to replicate the visual feedback of the
LEDs in the display.

There is one exception, however: If `maxactive` is 1 *and* you use the `output`
jack, the value of the output is being displayed. This is for situations where
you use the buttongroup to select one of several values. Here you get that value
visualized.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `minactive` (`ma`) | integer | `1` | Minimum number of active buttons. If you set this to `2`, then it is guaranteed that at least 2 buttons are active. If you set this to `0`, then it is possible to switch off all buttons. The `output` will be set to `0.0` in that case. |
| `maxactive` (`xa`) | integer | `1` | Maximum number of active buttons. It is an error to set this to `0`, since this would make this circuit useless. So a value of `0` is ignored and considered to be `1`. |
| `longpresstime` (`lt`) | CV | `1.5` | The number of seconds after which a button press is considered as a *long press*. |
| `button1 … button32` (`b`) | trigger | | 1st … 32nd button of the group. Any positive trigger seen here will toggle this button. And another button might go on or off in order to make sure that the number of active buttons is within the allowed range. |
| `value1 … value32` (`v`) | CV | `☞ smart` | Value that will be sent to the output if the 1st … 32nd button is active. These inputs default to 0 for `value1`, 1 for `value2` and so on and 31 for `value32`. |
| `startbutton` (`sb`) | integer | `1` | If you set this parameter to the number of a button, that button will be selected (and all others deselected) at the start when no state is loaded or at a trigger to `clear`. This allows you to set useful default values for your button groups. If `minactive = 0`, you also can set `startbutton = 0`. Then a `clear` will clear all buttons. If you set `startbutton = -1`, the maximum number of allowed buttons will be set. This is useful in situations where `maxactive` is greater than 1. If `maxactive` is less than the number of buttons, the selected buttons are filled from the start. |
| `display` (`dy`) | integer | `1` | This parameter selects the number of the DB8E display controller, where any change of this circuit's state will show up. Per default, the first DB8E available is used. Set this to `2` if you have a second DB8E and want to use that. Note: any other controller is not being counted here, it's really just the number of the DB8E as counted from the master module. Set this to `0` to suppress any display output from this circuit. |
| `header` (`hr`) | text | `☞ smart` | This text is displayed at the top of the DB8E display whenever this circuit shows up in the display. If you omit this, an automatic title is used. For example if the output of this circuit is fed into an internal patch cable, the name of this cable is displayed. Setting `header = ""` or `header = 0` removes the header and just displays the actual value. |
| `select` (`s`) | integer | `☞ smart` | The select input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to `1`). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | `☞ smart` | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is `0`, the circuit will be active if `select` is exactly `0` instead of a positive gate signal. In some cases this is more convenient. |
| `preset` (`pr`) | integer | `☞ smart` | This is the preset number to save or to load. Note: the first preset has the number `0`, not `1`! For the whole story on presets please refer to the manual ([basics](../basics.md)). This circuit has 16 presets, so this number ranges from `0` to `15`. |
| `loadpreset` (`lp`) | trigger | | A trigger here loads a preset. As a speciality you can use the trigger for selecting a preset at the same time. |
| `savepreset` (`sp`) | trigger | | A trigger here saves a preset. |
| `clear` (`cl`) | trigger | | A trigger here loads the default start state into the circuit. The presets are not affected, unless you use direct preset switching with the `preset` input and without triggers. And in that case the current preset is also cleared. |
| `clearall` (`ca`) | trigger | | A trigger here loads the default start state into the circuit and into all of its presets. |
| `dontsave` (`dos`) | gate | `0` | If you set this to `1`, the state of the circuit will not be saved to the SD card and not loaded from the SD card when the Droid starts. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `led1 … led32` (`l`) | gate | This output will be on / 1.0, whenever the 1st … 32nd button is active and off / 0.0 otherwise. Wire this to the LED in the button. If you have wired `select`, these LED outputs will do nothing (not even send 0) unless this circuit is selected. |
| `buttonoutput1 … buttonoutput32` (`bo`) | CV | These are individual outputs for every button in the group. They output the button's value when it is active, otherwise 0. If `valueX` is not defined for `buttonX`, the value 1 is output (not the button's number!). Note: in contrast to the `led` output, these outputs are not affected by `select` but always functional. One application of these outputs is to use a `buttongroup` with `maxactive = X` and `minactive = 0` as a cheap bunch of X toggle buttons in one single circuit and still use `select`. |
| `output` (`o`) | CV | The sum of the values of all active buttons will be sent here. If no button is active, `0.0` is being output. |
| `buttonpress` (`bp`) | trigger | Emits a trigger if any button is being pressed. |
| `longpress` (`lop`) | trigger | Emits a trigger, when any button is pressed for at least 1.5 seconds. If this jack is used, `buttonpress` will emit a signal if the button in question is released before the 1.5 seconds, not immediately. This way you trigger *either* at `buttonpress` or at `longpress`, not at both. |
| `selectionchanged` (`sc`) | trigger | Emits a trigger when the selection of the buttons has changed. This is not quite the same as `buttonpress`, since a button press might not lead to a change. Also in multi button situations (e.g. `maxactive = 4` where you have 7 buttons) the change is delayed up to 25 ms due to detection of bursts of quasi simultaneous presses. |
| `extrapress` (`ep`) | trigger | Emits a trigger, when one of the buttons was pressed but the selection has **not** changed. This can be used to build clever interfaces like in the Motor Fader Performance Sequencer, where a press on the already selected track toggles the current page. |

## See also

- [`button`](button.md) — a single toggle/push button, useful when you only need one.
- [`minifonion`](minifonion.md) — scale/root-note quantizer whose scheme the root/scale examples follow.
- [`arpeggio`](arpeggio.md) — another circuit that consumes root and scale selections.
