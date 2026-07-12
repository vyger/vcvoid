---
circuit: encoder
title: Provide access to a knob on the E4 or DB8E controller
obsolete: false
ram_bytes: 184
manual_pages: [213, 214, 215, 216, 217, 218, 219, 220, 221, 222]
category: controller-ui
tags: [encoder, e4, db8e, knob, led-ring, virtual-value, discrete, infinity, bipolar, snapto, autozoom, pitch-bend, display, select, preset]
see_also: [encoderbank, select, buttongroup, lfo, timing, quantizer, copy, button]
impl_difficulty: moderate
controller_binding: controller-required
verification: requires-human
spec_gap: false
difficulty_note: Rich virtual-value engine (6 modes incl. infinities, discrete, notch, snapto, autozoom, smooth, presets, movement triggers) driven by a physical encoder that must be emulated on an E4/DB8E controller.
verification_note: "Requires-human: the output value logic (modes, discrete stepping, outputscale/offset, snapto, movement/valuechanged triggers) is headless-testable by injecting synthetic encoder steps, but the 32-LED ring feedback and turn-feel (sensivity, autozoom, smooth) are visual/haptic and need human comparison to hardware. Depends on emulating the E4/DB8E encoder."
---

# encoder — Provide access to a knob on the E4 or DB8E controller

This circuit provides access to one of the encoders of an E4 or DB8E controller
and allows you to control one "virtual" value with it. Unlike pots or buttons,
the encoders always need a circuit to use them.

## Introduction

The E4 controller (see the manual, [hardware](../hardware.md)) has four rotary
knobs that you can turn endlessly in either direction. These knobs are commonly
called "encoders" — probably because you can encode data with them, which is
weird, because with buttons, faders and switches you can encode data equally
well. The DB8E controller (see the manual, [hardware](../hardware.md)) has one
such knob and you can use it in the same way as the one on the E4.

Unlike a potentiometer, which always has a certain *position*, an encoder does
not have one and thus no current "value". For this reason you cannot directly
use the registers of the encoders, like `E1.1`. Instead you always need a
circuit to access them.

The circuit `encoder` uses one of the encoders and lets you control a virtual
value with it. It is virtual, because you can map as many different values onto
one physical encoder as you like. Mapping multiple functions to one encoder
requires using the `select` input and needs one instance of the `encoder`
circuit per function. Please refer to [`select`](select.md) for the whole story
on `select`.

An alternative to `encoder` is the circuit [`encoderbank`](encoderbank.md). It
does the same, but with a number of encoders at once, so you need less circuits
if you work with many encoders in a similar way.

## What you can do with encoders

Encoders are surprisingly flexible and can do much more than mimicking a
potentiometer. Here are some examples:

- Select a value between 0.0 and ∞ (infinity)
- Select a discrete integer value, e.g. 0, 1, 2, … 7
- Use it as a kind of "pitch bend dial": the value moves back to the center on
  its own if you don't move it.
- Strumming: get a trigger every time the knob is turned by a certain angle

For the purpose of displaying the currently selected value, each encoder is
surrounded by a ring of 32 multi color LEDs.

In addition each encoder has a builtin push button that can be accessed with a
`B` register, e.g. `B1.1` for the first button if the E4 is your first
controller. There is also an `L` register for each encoder (e.g. `L1.1`). This
allows you to use the whole LED ring around the encoder as one big white LED —
nicely overlaying with any actual animation from the encoder itself.

## Basic usage

The most basic example uses your first encoder (on the first E4 or DB8E) to
select a value between `0` and `1`. The result is sent as a voltage to output
`O1`. That's as simple as this:

```droid
[encoder]
    encoder = E1.1
    output = O1
```

`E1.1` stands for encoder number `1` on your first controller (which must be an
E4 or DB8E). `E7.2` would mean encoder `2` on controller number `7`.

When you now turn the encoder, you see a colored dot moving around the square
LED ring. It begins at the bottom, left of the center. This position marks the
value `0.0`. Turn the encoder clockwise and the colored dot moves along, leaving
a trail behind. At the end — denoting the value `1.0` — the dot is again at the
bottom, this time right of the center.

Notes:

- The position of the white pointer of the knob has no meaning. It can be
  anywhere. You can use it for your own orientation, if you like, but it does
  not reflect the current logical value. We use the same knobs as for the
  potentiometers simply because we like the overall look.
- The light of the LEDs is mixing in the opaque material of the front plate so
  the light of one LED position bleeds a bit into its neighbor positions. That's
  the reason why the bottom position lights a bit, was well, in a real E4 or
  DB8E.

## Multiple functions on one knob

Overlaying an encoder with multiple independent functions is a standard task. As
usual it works with the `select` input (see [`select`](select.md) for how all
this works). The following example uses a [`buttongroup`](buttongroup.md) for
selecting three different encoder functions, each outputting a voltage to a CV
output. Here is a complete patch example:

```droid
[p2b8]
[e4]

[buttongroup]
    button1 = B1.1
    button2 = B1.2
    button3 = B1.3
    led1 = L1.1
    led2 = L1.2
    led3 = L1.3

[encoder]
    encoder = E1.1
    select = L1.1
    output = O1

[encoder]
    encoder = E1.1
    select = L1.2
    output = O2

[encoder]
    encoder = E1.1
    select = L1.3
    output = O3
```

It's no mistake that each `encoder` circuit uses `encoder = E1.1`, because we
want to map three different functions on the *same* encoder. Each time you press
another of the three buttons, you switch to the corresponding function and the
LEDs around the encoder get a different color.

## Choosing a different range of values

A range from `0.0` to `1.0` is not always what you want. For your convenience
there are the parameters `outputscale` and `outputoffset`. They are applied to
the output value and work similar as the attenuation and offset of parameter
inputs: the `outputscale` is applied first and then the `outputoffset`.

Let's assume you want the output to be in the range `0.2` … `0.5`. This can be
achieved by scaling the output by `0.3` first (because 0.5 − 0.2 = 0.3) and then
adding `0.2` (because that's the start value):

```droid
[encoder]
    encoder = E1.1
    output = O1
    outputscale = 0.3
    outputoffset = 0.2
```

## Setting the sensitivity of the knob

For technical reasons an encoder of the E4 or DB8E has 96 discrete steps in each
rotation. This means that all changes of the output value are stepped. If your
range is `0.0` … `1.0`, one such step is 1/96, which is 0.0104 — or 0.104 V. If
you use the output as the pitch of a VCO, these steps are clearly audible.

You can use the parameter `sensivity` to amend this (sorry for the illnamed
input, it *should* be named sensitivity, but isn't). If you set this e.g. to
`0.1` (which is `10%`), one rotation just does 10% of 1/96 and the step size is
reduced to 10% of its original value. This means, that you need to turn the knob
10 times around until the colored dot reaches its end position:

```droid
[encoder]
    encoder = E1.1
    output = O1
    sensivity = 10%
```

If you like, you can use the push button in the encoder for switching the
sensitivity. The output `button` gives you the current state of the encoder's
button, when it is selected. This you can self-patch into the sensitivity (the
input is called `sensivity`). In the following example the sensitivity is 10%.
But as long as you hold the button it goes up to 100% (the button adds 90% to
the standard 10%):

```droid
[encoder]
    encoder = E1.1
    output = O1
    button = _BUTTON
    sensivity = _BUTTON * 90% + 10%
```

A different way for changing the sensitivity is to use `autozoom`. This
parameter needs a value from `0` (auto zoom is off) to `1` (maximum auto zoom).
When auto zoom is enabled, slow movements of the encoder change the virtual
value just by very tiny amounts while very fast movements change the value even
more than usual. With `autozoom = 1`, this effect is dramatic, so maybe start
with using `0.5` as a first try:

```droid
[encoder]
    encoder = E1.1
    output = O1
    autozoom = 0.5
```

## Bipolar values

Sometimes you need bipolar values, e.g. those that range from `-1.0` to `+1.0`.
This can be achived by setting `mode = 2`. The following example outputs a value
between -2 V and 2 V. It achieves this by setting the output scale to `2V`,
which is applied to the original output range of -1.0 … +1.0:

```droid
[encoder]
    encoder = E1.1
    output = O1
    mode = 2
    outputscale = 2V
```

While you *could* achive a similar effect with `outputscale = 4V` and
`outputoffset = -2V`, using `mode` is better, since you get a nice bipolar LED
display where the center of the value (denoting 0.0) is top center and the LED
trail starting from there.

## Infinity

The fact that the encoder has no "end" can be used to choose a value from an
infinite range. There are three different values for `mode` that setup an
infinite range:

- `mode = 3`: Positive infinity: 0 … ∞
- `mode = 4`: Negative infinity: −∞ … 0
- `mode = 5`: Bipolar infinity: −∞ … ∞

When working with infinity, it can be handy to be able to reset the value to
0.0. This is done with the `clear` input, which can be self-patched to the
button:

```droid
[encoder]
    encoder = E1.1
    output = O1
    mode = 3 # positive infinity
    button = _BUTTON
    clear = _BUTTON
```

## Circular infinity

The encoder supports yet another type of infinite movement. This is selected by
`mode = 6`:

- `mode = 6`: Circular infinity: range is `0.0` … `1.0`, but repeats over in
  both directions

In this mode the `output` always is in the range `0.0` … `1.0` — or to be more
precise more like `0.0` … `0.99999...`, since `1.0` is never reached. If the
value is already near 1 and you continue turning clockwise, it starts over at 0.
Or if you are at 0 and turn counter clock wise, the value jumps to `0.9999...`
from there.

```droid
[encoder]
    encoder = E1.1
    output = O1 # always in range 0 ... 1
    mode = 6 # circular infinity
```

Two applications spring into mind where this is useful:

- Setting the `phase` or `syncphase` of the [`lfo`](lfo.md) circuit.
- Setting the relative timing of the [`timing`](timing.md) circuit.

## Colors and LEDs

Per default, every virtual encoder function gets its own color for the LED ring.
You can set your favourite color with the `color` parameter. Here is a simple
example:

```droid
[encoder]
    encoder = E1.1
    color = 0.4 # green
    output = O1
```

If you are using a bipolar range (e.g. `mode = 2`) you can set a different color
for the negative range, for example red:

```droid
[encoder]
    encoder = E1.1
    output = O1
    mode = 2
    outputscale = 2V
    color = 1.2 # blue
    negativecolor = 0.8 # red
```

This makes the LED indication red in the negative half and keeps the usual color
(here blue) in the positive half. The center position is always marked white.

Here are some example colors:

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

## Discrete values

Sometimes you need a control for discrete values like 0, 1, 2 and so on. This
might be a setting for the length of a sequence, a clock division or a value for
`select` inputs. You can set an encoder to output such numbers by using the
parameter `discrete`.

Set `discrete` to the number of different values the encoder should output. Of
course, this number needs to be a least 2. The output values always start from
0. Here is an example for the encoder switching between the values 0, 1, 2 or 3:

```droid
[encoder]
    encoder = E1.1
    discrete = 4 # values 0, 1, 2, 3
    output = _SELECT # goes somewhere
```

Working with `discrete` could be simulated by using the normal mode and applying
a [`quantizer`](quantizer.md) circuit afterwards. But that does not have the
nice visual feedback that `discrete` has.

The LED ring shows the possible "switch positions" of the encoder as colored
dots while the currently selected position is white.

The parameters `outputoffset` and `outputscale` can be used to scale or shift
these numbers. Let's build a switch for selecting one of the values -2 V, -1 V,
0 V, 1 V and 2 V. Such a switch can be used as an octave switch for a VCO.

To do this we set `discrete = 5`, because it's five different values (0 … 4).
Then we scale this by 1 V to get 0 V … 4 V. And if we then substract 2 V by
setting `outputoffset = -2V`, we get what we want:

```droid
[encoder]
    encoder = E1.1
    output = O1
    discrete = 5
    outputscale = 4V
    outputoffset = -2V
```

Note: `mode` does not work with `discrete`. You cannot have infinite numbers to
choose from, but you can set `discrete = 1000000` if you really need something
practically near to infinity.

## Output triggers on encoder movements

You can get a trigger whenever the encoder has moved in either direction by
using the outputs `movedup` and `moveddown`. `movedup` means that the virtual
value would increase, hence clockwise rotation.

Per default you get a trigger for every 5 internal movement steps of the
encoder. One full rotation is 96 steps. You can change this resolution with
`movementticks`. For example if you want 12 triggers per rotation, set
`movementticks = 8`.

```droid
[encoder]
    encoder = E1.1
    output = O1 # output is optional
    movementticks = 8
    moveddown = O2 # CCW
    movedup = O3 # CW
```

Note: If you set `movementticks` to a low number and/or turn the knob super
fast, the triggers can become so frequent that they would merge together and the
receiving end would see them as one big blurb. This is because the duration of a
trigger is 10 ms — as it is standard in DROID.

In order to avoid this, the triggers are queued and delayed in a way that there
is at least a gap of 10 ms between two triggers. This way no trigger is lost, but
after a fast turn of the knob it can take some fraction of a second until the
last trigger has been sent out.

There is an alternative trigger that fires whenever the virtual value has
changed:

```droid
[encoder]
    encoder = E1.1
    output = O1 # output is optional
    valuechanged = O2
```

There are a few of differences to the `moveddown` and `movedup` outputs. Those
are more "lowlevel" and tell you about the movement of the actual encoder, while
`valuechanged` watches over the virtual value that you edit with the encoder.
Here are the differences:

- You don't get triggers if the minimum or maximum value has been reached and
  the knob is still turned.
- When `discrete` is used, `valuechanged` triggers exactly at the point when the
  encoder snaps to the next position.

## Yet more features

There are even more features of the encoder circuit, which you find in the list
of parameters below. Specifically have a look at `notch`, `smooth`, `snapto`,
`snapforce`, `ledfill` and `override`.

## Display

If you have a DB8E display controller (see the manual,
[hardware](../hardware.md)), the `encoder` circuit automatically updates the
display whenever you turn the encoder. It then shows the updated value of
`output` in the display.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `encoder` (`e`) | integer | `1` | The encoder to use. You can either use its register name, like `E8.2` for encoder 2 on controller 8. As an alternative you can use a number like `6`. That would specify the 6th encoder of your setup. This value is read just once when the DROID starts. Making this parameter dynamic does not work. |
| `override` (`or`) | CV | `☞ smart` | Use this parameter to convert the encoder into a mere display. The knob is completely ignored and the value from the input is used as the value that is displayed in the LED ring. The parameter `discrete` still works, so you can use the LED ring for displaying a discrete number such as the current step in a sequence. Also `mode` is honored; values that do not fit into the selected range or number of `discrete` values will be rounded to the nearest allowed value. `override` honors `select`, so if you use `select`, it does nothing to the LEDs while the encoder circuit is not selected. By default there is no override and the encoder works normally. |
| `sharewithnext` (`sw`) | gate | `0` | If set this to `1`, the output `output` will not be used but the circuit shares its output with the next `encoder` circuit *and operates on the same virtual value as that*. Use this if you want to control the same value with two different encoder circuits (which might be available in different contexts of your user interface). If you do this, make sure that both encoder circuits have the same settings of `mode` and `discrete`. |
| `movementticks` (`mv`) | integer | `5` | Specifies the number of encoder ticks you need to turn to get one trigger at `movedup` or `moveddown`. One complete rotation of the encoder creates 96 such ticks. |
| `led` (`l`) | 0..1 | `☞ smart` | You can use the ring of LEDs around the encoder as one virtual LED using this parameter. This is similar to using the according `L` register of the E4 or `Lx.9` on the DB8E, but honors the `select` input. If you set `led` to `1`, all LEDs will get brighter or white, if they would be black otherwise. By default no such override happens. |
| `startvalue` (`sv`) | CV | `0.0` | This sets the value the encoder gets when you start this circuit for the first time or when you send trigger to `clear`. Note: the range of this value refers to the situation *before* `outputscale` and `outputoffset` is applied. So if `mode` is unused or at `0`, a startvalue of 0.5 sets the encoder's virtual value exactly to the *center* — regardless of any scaling or offsetting that happens afterwards. |
| `notch` (`no`) | CV | `0.0` | This parameter helps you to dial in *exactly* the center of the selected range, which is `0.5` in normal mode and `0.0` in bipolar mode. The value of `notch` specifies the portion of one complete 360 cycle of the pot during which the center position should be assumed. `0.1` is probably a good value. Notch does not work if `mode` selects positive or negative infinity. |
| `outputscale` (`os`) | CV | `1.0` | The `output` is multiplied by this value. This is just for convenience and may save a [`copy`](copy.md) circuit in some situations. |
| `outputoffset` (`oo`) | CV | `0.0` | After scaling the virtual value with `outputscale`, this value is being added before sending the result to the output. |
| `mode` (`m`) | integer | `1` | Selects the possible range of the virtual value. See the mode table below. This setting is ignored if `discrete` is in use. Note: The mode `0` is for situations where encoders are overlayed with `select` and an encoder is unused. Setting `mode = 0` can be used to disable this encoder and blank its LEDs. |
| `smooth` (`sm`) | CV | `0.5` | Unlike a potentiometer, an encoder does not output continous values but steps. If you directly wire the output of an encoder to a CV input of an audio module, the steps might become audible. Therefore the final values of the encoder are smoothed out by a simulation of a low pass filter. That's essentially the same as in the `slew` circuit. The smoothing is enabled per default but you can change it with this parameter. A value of `0.0` turns off smoothing and you get access to the tiny steps of the encoder. `1.0` selects a maximum smoothing, which has also the effect that fast turns of the encoder are slowed down a bit. The default value of `0.5` does just a mild slew limiting. If you use `discrete`, the smoothing is not applied. |
| `discrete` (`d`) | integer | `0` | Set this to an integer number of `2` or higher to enable *discrete* mode. In this mode the encoder works like a rotary switch for selecting one of the numbers 0, 1, 2 and so on. The number you set for `discrete` selects the number of positions in this "switch". For example `discrete = 4` produces the values 0, 1, 2 or 3. In this mode the inputs `notch`, `mode` and `smooth` are ignored. |
| `snapto` (`sn`) | CV | `☞ smart` | Use this parameter to define a position where the encoder value automatically returns to if it is not turned. This behaves a bit like a pitch bend wheel. You can get crazy CV modulations if you use a dynamic CV for `snapto`, such as the output of an LFO. The encoder's value will try to follow the LFO but you can still turn the encoder and work "against" the LFO. This mechanism also works if the encoder is not selected. By default no snapto position is set. |
| `snapforce` (`sf`) | CV | `0.5` | Specifies the speed or "force" with that the encoder moves back to the `snapto` position if that is used. A force of `0.0` deactivates `snapto`. |
| `sensivity` (`se`) | CV | `1.0` | The sensivity determines how far you need to turn the knob to get which amout of value change. Per default one turn of 360 degrees changes to the value from 0.0 to 1.0. A `sensivity` of 2, doubles the speed of change, 0.5 slows it down to the half. If you use `mode` to enable infinity, there is no total range. In this case one turn changes the value by `sensivity`. If you use `discrete`, one turn of the knob changes the virtual switch by eight positions, if sensitivity is at 1.0, and accordingly faster or slower if you change that. At this point we must apologize for the fact that this parameter has been illnamed. Sensivity is not an existing English word. It meant to be sensi*t*ivity. But fixing this would render many existing patches invalid and impose despair and annoinance on all our beloved users, so we decided to leave it as it is. |
| `autozoom` (`a`) | 0..1 | `0.0` | The "auto zoom" feature allows you to fine adjust values when turning the knob slowly and coarse adjust when you turn it fast. If `autozoom` is at the maximum value of `1.0`, turning the knob just slowly changes the value by super tiny amounts, while turning it fast operates way faster than usual. Use any value between `0.0` and `1.0` for `autozoom` to select the level of this slowing down for slow movements. `autozoom` has no effect if `discrete` is used. |
| `color` (`co`) | CV | `☞ smart` | Color of the pointer in the LED ring. See the example color values in the Colors and LEDs section. By default every virtual encoder function gets its own automatically assigned color. |
| `negativecolor` (`nc`) | CV | `☞ smart` | If you use this parameter, it defines the color of the LEDs in case the current logical value is negative. By default the same color as `color` is used. |
| `ledfill` (`lf`) | integer | `1` | Selects whether the LED ring displays the current value with just a single colored dot (`ledfill = 0`) or by additionally illuminating all LEDs between 0 and the current value in half brightness (`ledfill = 1`). |
| `display` (`dy`) | integer | `1` | This parameter selects the number of the DB8E display controller, where any change of this circuit's state will show up. Per default, the first DB8E available is used. Set this to `2` if you have a second DB8E and want to use that. Note: any other controller is not being counted here, it's really just the number of the DB8E as counted from the master module. Set this to `0` to suppress any display output from this circuit. |
| `header` (`hr`) | text | `☞ smart` | This text is displayed at the top of the DB8E display whenever this circuit shows up in the display. If you omit this, an automatic title is used. For example if the output of this circuit is fed into an internal patch cable, the name of this cable is displayed. Setting `header = ""` or `header = 0` removes the header and just displays the actual value. |
| `select` (`s`) | integer | `☞ smart` | The `select` input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to `1`). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. By default the circuit is always selected. |
| `selectat` (`sa`) | integer | `☞ smart` | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is `0`, the circuit will be active if `select` is exactly `0` instead of a positive gate signal. In some cases this is more conventient. By default a positive gate selects the circuit. |
| `preset` (`pr`) | integer | `☞ smart` | This is the preset number to save or to load. Note: the first preset has the number `0`, not `1`! For the whole story on presets please refer to the manual ([basics](../basics.md)). This circuit has 16 presets, so this number ranges from 0 to 15. |
| `loadpreset` (`lp`) | trigger | | A trigger here loads a preset. As a speciality you can use the trigger for selecting a preset at the same time. |
| `savepreset` (`sp`) | trigger | | A trigger here saves a preset. |
| `clear` (`cl`) | trigger | | A trigger here loads the default start state into the circuit. The presets are not affected, unless you use direct preset switching with the `preset` input and without triggers. And that case the current preset is also cleared. |
| `clearall` (`ca`) | trigger | | A trigger here loads the default start state into the circuit and into all of its presets. |
| `dontsave` (`dos`) | gate | `0` | If you set this to `1`, the state of the circuit will not saved to the SD card and not loaded from the SD card when the Droid starts. |

### `mode` values

| Value | Meaning |
|-------|---------|
| 0 | Off: the encoder is unused, its LEDs are off |
| 1 | Normal mode: fixed range between `0.0` and `1.0` |
| 2 | Bipolar mode: fixed range between `-1.0` and `1.0` |
| 3 | Positive infinity: open range between `0.0` and ∞ |
| 4 | Negative infinity: open range between −∞ and `0.0` |
| 5 | Bipolar infinity: open range between −∞ and ∞ |
| 6 | Circular infinity: range is `0.0` … `1.0`, but repeats over in both directions |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | Outputs the current virtual value of this circuit. Don't use this if you are using `sharewithnext`. |
| `button` (`b`) | gate | This output provides you with the current state of the push button in the encoder, but only if the circuit is selected. While you *could* do this with an extra [`button`](button.md) circuit, using this output is more convenient in some situations. While the circuit is not selected, the output is set to `0`. |
| `moveddown` (`md`) | trigger | Outputs a trigger whenever you have turned the encoder left (counter clock wise) by a certain amount (which can be altered by `movementticks`). Beware: If you turn too fast, triggers might overlap and merge together. |
| `movedup` (`mu`) | trigger | Outputs a trigger whenever you have turned the encoder right (clock wise) by a certain amount (which can be altered by `movementticks`). Beware: If you turn too fast, triggers might overlap and merge together. |
| `valuechanged` (`vc`) | trigger | Outputs a trigger whenever the virtual value has changed. |

## See also

- [`encoderbank`](encoderbank.md) — the same functionality, but for a number of
  encoders at once.
- [`select`](select.md) — how to overlay multiple functions on one encoder.
