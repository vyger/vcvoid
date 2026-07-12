---
circuit: display
title: Display texts and change parameters on a DB8E
obsolete: false
ram_bytes: 56
manual_pages: [199, 200, 201, 202, 203, 204]
category: controller-ui
tags: [display, db8e, text, value, oled, header, screensaver, takeovercontrols, select, popup, numbermode]
see_also: [pot, encoder, switch, select, button, copy]
impl_difficulty: moderate
controller_binding: controller-required
verification: requires-human
spec_gap: false
difficulty_note: Feasible by rendering the DB8E OLED in the module UI; breadth comes from update/linger/threshold logic, 19 number-modes, screensaver, select and takeovercontrols rather than any one hard part.
verification_note: "Requires-human: the value/text update and linger/threshold change-detection logic is headless-testable, but what actually reaches the OLED (font sizing, gauges, sparklines, note formatting) is visual and needs human comparison against hardware. Depends on emulating the DB8E controller."
---

# display — Display texts and change parameters on a DB8E

The unique feature of the DB8E controller (see the manual
([hardware](../hardware.md))) is its tiny display. In many cases the display
simply works. If you turn a knob or move a slider or fader, you see the exact
position of this control in the display. Furthermore, when then you interactively
change the state of a circuit, like [`pot`](pot.md) or [`encoder`](encoder.md),
which modifies *virtual* values, the display shows the current value that you
just have modified.

There are some cases, however, where you want to show your own things in the
display. You can either show *values* or *texts*. Both is done with this circuit.

## Showing texts

Showing your own text is done with the `text` parameter:

```droid
[display]
    text = "Hello world!"
```

You find more details about texts in patches in the manual
([basics](../basics.md)).

This example is a bit boring since the text never changes. Also is has the
problem that once you turn a pot somewhere, the pot will be displayed and your
text vanishes. It does never come back. Why is that? Because the `display`
circuit only ever sends something to the display, if it *changes*.

Let's make a better example, where you have a button that starts and stops your
clock. Everytime you press the button and start or stop the clock, you want to
see the new state in the display:

```droid
[db8e]
[p2b8]

[button]
    button = B2.1
    led = L2.1
    output = _RUNNING
    inverted = _NOT_RUNNING

[display]
    text = "Started" * _RUNNING

[display]
    text = "Stopped" * _NOT_RUNNING
```

The line `text = "Started" * _RUNNING` sets the text to "Started" when the cable
`_RUNNING` is `1`. This is the case while the button is active.

So by pressing the button for the first time, the `text` parameter changes from
`0` – which means an empty text – to the number of the text `"Started"` (for how
texts are converted into numbers read the manual ([basics](../basics.md))). The
display circuit thus sees a changed value of `text` and sends an update to the
display.

When you press the button again, `_RUNNING` switches back to `0`. The first
`display` circuit sees a change, again. But here a second rule comes into play:
It only sends a display update if the text is not empty. So in this case it does
nothing.

But now the second `display` circuit sees its text changed, this time to
`"Stopped"`, since `_NOT_RUNNING`, the inverted button output, is now `1`. And
the display is updated accordingly.

When you have more than two settings, a [`switch`](switch.md) circuit comes in
handy. Here is a complete example where a pot is used to switch between seven
waveforms of an LFO and the display shows the name of the selected waveform:

```droid
[db8e]
[p2b8]

[pot]
    pot = P2.1
    discrete = 7
    output = _WAVEFORM

[lfo]
    hz = 2
    waveform = _WAVEFORM
    output = O1

[switch]
    input1 = "square"
    input2 = "sawtooth"
    input3 = "triangle"
    input4 = "ramp"
    input5 = "paraboloid"
    input6 = "sine"
    input7 = "cosine"
    offset = _WAVEFORM
    output1 = _TEXT

[display]
    text = _TEXT
    linger = 1
```

First [`pot`](pot.md) is being used to convert the potentiometer `P2.1` into a
switch with seven positions. The cable `_WAVEFORM` thus outputs a number in the
range 0, 1, …, 6. This is fed into `lfo`.

At the same time the waveform number is used to set an offset in a
[`switch`](switch.md) circuit. The seven inputs point to the names of the
waveforms as texts. The output is sent into `_TEXT`, which then contains the name
of the selected waveform. This goes into the `text` input of a `display` circuit.

Here is one speciality: the input `linger`. If you remove it, you will see the
display flicker between the waveform name and the output of the `pot` circuit.
Both fight for the display. The text changes just when you turn the pot enough to
change the actual output value. But the `pot` circuit updates the display even on
a slight movement (this is important for the display to become immediately active
when you use the pot).

The line `linger = 1` make the text linger on at the display for at least one
second, regardless of any other automatic updates that might happen inbetween.
This removes the flickering and the `pot` circuit's updates are hidden.

Another way would have been to disable display updates of `pot` by setting
`display = 0` there.

## Showing values

If you want to show a value (a number), use the input `value` instead of `text`
and simply feed it the value. As soon as the value changes, it will be shown in
the display. Here is a complete example, where you use two pots for fine and
coarse tuning a voltage. The result is shown in the display:

```droid
[db8e]
[p2b8]

[pot]
    pot = P2.1
    outputscale = 0.1V
    output = _FINE

[pot]
    pot = P2.2
    outputscale = 5.0V
    output = _COARSE

[copy]
    input = _FINE + _COARSE
    output = _TUNING

[display]
    value = _TUNING
    linger = 1
    threshold = 0.001V
```

Again, we use `linger = 1`, in order to avoid conflicts with the `pot` circuits.
The line `threshold = 0.001V` avoids display updates for changes less than
0.001 V. This avoids flickering of the last digit, which can happen because pots
always have a minimal noise.

## Using select

The `display` circuit can be *selected* via the input `select` or `selectat`. If
you have never heard about this concept, please read the section "Overlaying
pots" in the [`select`](select.md) documentation first, where the concept of
selecting is explained.

As soon as you use the `select` or `selectat` input, the display circuit is
inactive even if its input value or text changes. And when it *gets* selected, it
immediately sends its current value or text to the display in order to catch up
with any changes it might have lost while it was not selected.

## Taking over controls

When you seen the DB8E for the first time, probably very quickly one idea poppes
into your mind: Can I use the eight buttons and the encoder for my own purpose?
Even without the display a "B8E" controller would be nice, wouldn't it? And of
course you can!

The `display` circuit has the parameter `takeovercontrols`. Set this to `1` and
all are yours:

```droid
[db8e]
[display]
    takeovercontrols = 1
```

You can use the buttons as usual with `B1.1 … B1.8`. And even the push button in
the encoder is available with `B1.9` (all assuming that the DB8E is the first in
your chain).

The encoder needs an `[encoder]` circuit as usual (see [`encoder`](encoder.md)):

```droid
[db8e]
[display]
    takeovercontrols = 1

[encoder]
    encoder = 1
    output = O1
```

But now comes the question: how can you now control the display? The easiest way
is to you allow yourself to toggle between owning the controls for your patch and
leaving them to the display. A good way is to use the push button in the encoder
for this task. This button is not used by the display itself (for just this
reason). Map a `[button]` circuit to it and use its output for selecting whether
to take over controls or not:

```droid
[db8e]

[button]
    button = B1.9
    output = _TAKEOVER

[display]
    takeovercontrols = _TAKEOVER

[encoder]
    encoder = 1
    output = O1
```

Notes:

- This works, because the push button `B1.9` in the encoder is *never* used by
  the display. It is *always* directly accessible by your patch.
- When you don't use `takeovercontrols`, or when it is set to `0`, the registers
  `B1.1 … B1.8` of the DB8E are `0`. Also the encoder does not register any
  movements nor does its LED ring display anything from your patch.
- If you use `select` in you `display` circuit, the parameter `takeovercontrols`
  only is used, when the circuit is selected.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `display` (`dy`) | integer | `1` | Number of the display (starting from 1). |
| `text` (`tx`) | text | ☞ smart | Text to display in the center of the display on a DB8E. |
| `header` (`hr`) | text | ☞ smart | Text to display a the top of the display. If you omit this, no header will be displayed. A header will only be displayed, if you also use either `text` or `value`. It is not possible to just display a header. |
| `fontsize` (`f`) | integer | `0` | Size of the font to use in the display when displaying a text. There are three possible sizes. The default is to use the largest possible font where the text fits into the display. Note: This setting is not effective when displaying using the `value` input. In that case the display format of the number can be adjusted with the buttons on the DB8E. See the table below. |
| `value` (`v`) | CV | ☞ smart | Numeric value to display. This parameter does not work, if you used `text` at the same time. |
| `threshold` (`th`) | CV | ☞ smart | If you use the input `value`, the display will become active whenever the input value changes. In order to avoid constant display flicker when the input is unstable (such as a physical potentiometer), the change needs to exceed a value that you specify with this parameter in order to be recognized. This value is set to a small default value of 0.0005. If this display is triggered randomly by small irrelevant changes, increase `threshold`. If the display does not react to small changes, decrese `threshold`. |
| `numbermode` (`nm`) | integer | `0` | When you display a number using the `value` input, the user can use the buttons on the DB8E to set the number mode (like voltage, sparkline, etc.). With this input you can fix the mode. The value is then always displayed in a certain way an the buttons cannot be used for changing the mode anymore. See the table below. |
| `linger` (`l`) | CV | `0.01` | When using `text` or `value`, the display is only updated when the input text or value changes. When other things in your patch also send information to the display, for example a [`pot`](pot.md) circuit with `discrete` being used for switching the thing that is show here, your text might be overwritten immediately. This option specifies a number of seconds. Using it forces your text to linger on this time, even if other circuits would want to change the display's contents. |
| `trigger` (`t`) | trigger | | This input allows you to bring "popup" messages in the display when a certain event happens. When this input is patched, a trigger into this input forces the text in `text` or `value` to be displayed. It is not watched for changes any longer. The text is visible until some other information source finds its way to the display. |
| `useasdefault` (`u`) | gate | `0` | Set this to `1` if you want the output of this circuit to be the default view of the display. This means it will always be shown if nothing else is shown (still needs to be selected, if you use `select`). |
| `screensaver` (`ss`) | CV | ☞ smart | Sets the time in seconds until the screen saver kicks in. In order not to burn in specific pixels in the OLED display of the DB8E, after 60 seconds of no user interaction, the DB8E switches to showing a screen saving animation. It snaps back to normal as soon as you press any button, turn any knob, switch any switch or move any fader oder slider on any of your controllers. You can tweak the screen saver time with this parameter. Longer times can make working more conveniant in some situations but might wear off your display a little bit sooner. |
| `takeovercontrols` (`to`) | gate | ☞ smart | Set this to `1` to take over the encoder and the eight buttons and its according LEDs for use in your patch. The push button in the encoder is not affected by this setting since it is always available in your patch. See circuit description for details. |
| `select` (`s`) | integer | ☞ smart | The `select` input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to `1`). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | ☞ smart | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is `0`, the circuit will be active if `select` is exactly `0` instead of a positive gate signal. In some cases this is more convenient. |

### `fontsize` values

| Value | Meaning |
|-------|---------|
| 0 | automatic size |
| 1 | small font size |
| 2 | medium font size |
| 3 | large font size |

### `numbermode` values

| Value | Meaning |
|-------|---------|
| 0 | Use the buttons on the DB8E to set the number mode |
| 1 | Integer number with no decimal places (`0`) |
| 2 | Normal number with own decimal place (`0.1`) |
| 3 | Normal number with two decimal places (`0.12`) |
| 4 | Normal number with three decimal places (`0.123`) |
| 5 | Normal number with four decimal places (`0.1234`) |
| 6 | Voltage with one decimal place (`1.2V`) |
| 7 | Voltage with two decimal places (`1.23V`) |
| 8 | Voltage with three decimal places (`1.234V`) |
| 9 | Percentage without decimal places (`12%`) |
| 10 | Percentage with one decimal place (`12.3%`) |
| 11 | Percentage with two decimal places (`12.34%`) |
| 12 | Note value assuming 1 volt per octave (`E♭1 -19`) |
| 13 | Simple gauge |
| 14 | Fancy gauge with percentage without decimal places |
| 15 | Fancy gauge with percentage with one decimal place |
| 16 | Fancy gauge with percentage with two decimal places |
| 17 | Sparkline unipolar |
| 18 | Sparkline bipolar |

This circuit has no outputs.

## See also

- [`pot`](pot.md), [`encoder`](encoder.md) — controls whose virtual values the
  display shows and that can be overlaid with `takeovercontrols`.
- [`switch`](switch.md) — select between several texts to display.
- [`select`](select.md) — the "Overlaying pots" concept used by `select` /
  `selectat`.
