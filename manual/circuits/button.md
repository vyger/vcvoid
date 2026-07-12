---
circuit: button
title: Does all sorts of useful things with buttons
obsolete: false
ram_bytes: 104
manual_pages: [154, 155, 156, 157, 158, 159]
category: controller-ui
tags: [button, toggle, momentary, longpress, shortpress, double-click, led, states, presets, overloading, select, ui]
see_also: [buttongroup, flipflop, droid, display, switch, lfo]
impl_difficulty: moderate
controller_binding: controller-required
verification: rack-automated
spec_gap: false
difficulty_note: "Simple per-button logic but broad: toggle/momentary, up to four states, long/short press, double-click, select overlay and presets."
verification_note: "Rack-automated: needs an emulated controller to feed button events and read led/output registers; press-timing and toggle logic are numeric and checkable without a human. Hardware, not spec, is the dependency."
---

# button — Does all sorts of useful things with buttons

This is a utility circuit for efficiently working with the buttons of your
controllers. It can implement toggle buttons (that do on/off) or even have three
or four states. It can detect long presses and double clicks and also helps you
to overload one button with several switchable functions. Note: If you just need
a plain momentary button without any of these or other nifty features, you can
use the register `B1.1`, `B1.2`, etc. directly and do not need this circuit.

Note: don't forget to declare your controllers at the top of your patch with
lines like `[p2b8]` or `[b32]`. In the below examples I've omitted these
declarations for sake of simplicity.

> This circuit is designed to build user interfaces. It is executed at a lower
> speed. Don't use it for other purposes.

## Toggle buttons

The most common use of `button` is to implement a toggle button. That's a button
that changes from on to off and back at each press of the button. The current
state of the button will persist on your SD card so you don't lose your state if
you switch off your rack.

Typically you will wire the `button` input to one of your controller's buttons
like `B1.1` and `led` to the LED in that button (`L1.1`). LED will then always
visualise the current state of the button. As a side effect the LED register
`L1.1` will store the button state as a value 0 or 1 and hence can be used by
some other circuit as an input.

Here is a typical example. The button is being used for enabling the loop in a
CV looper:

```droid
[button]
    button          = B1.4
    led             = L1.4

[cvlooper]
    loop            = L1.4
```

If you do not want the state of the button to be persisted on the SD card, use
`dontsave = 1`. This make sense for the CV looper since the loop is apparently
empty anyway when your DROID starts.

```droid
[button]
    button        = B1.4
    led           = L1.4
    dontsave      = 1

[cvlooper]
    loop          = L1.4
```

Usually the button switches between the two values 0 and 1. Sometimes, however,
you need different values. For this purpose there are the two inputs `offvalue`
and `onvalue`. They set two alternative values for the "off" and "on" states.
And the output `output` outputs the selected value (`led` still goes to 0 and
1). Here is an example for a toggle button that switches a clock divider between
2 and 4:

```droid
[button]
    button          = B1.4
    led             = L1.4
    offvalue        = 2
    onvalue         = 4
    output          = _CLOCK_DIV

[clocktool]
    input           = G1 # external clock
    output          = G2
    divide          = _CLOCK_DIV
```

Of course `offvalue` and `onvalue` are CV controllable. How can this make sense?
Well – as they can take variable inputs you can use a button for directly
switching between two different input CV signals. The following example will use
a button to switch between two different wave forms of an LFO (see
[`lfo`](lfo.md)). The button `B3.1` switches between sawtooth and sine and sends
the result to `O1`.

```droid
[lfo]
    hz              = 2
    sawtooth        = _SAWTOOTH
    sine            = _SINE

[button]
    button          = B3.1
    led             = L3.1
    offvalue        = _SAWTOOTH
    onvalue         = _SINE
    output          = O1
```

## Buttons with three or four states

Sometime you might want more than just two values. `button` supports switching
between up to four values. Use the `states` input and set it to 3 or 4. In the
following examples `output` will go through the values 0, 1, 2 and 3:

```droid
[button]
    button = B1.1
    led = L1.1
    states = 4
    output = _SOMETHING
```

If you don't like the default values, use the inputs `value1` through `value4`
for setting the four values. In fact `offvalue` is the same as `value1` and
`onvalue` as `value2`. If you specify `value3` or `value3`, `states` is
automatically set accordingly and you can simply omit it. The following example
switches between four different wave forms of an LFO:

```droid
[lfo]
    hz              = 2
    sawtooth        = _SAWTOOTH
    sine            = _SINE
    square          = _SQUARE
    triangle        = _TRIANGLE

[button]
    button          = B3.1
    led             = L3.1
    value1          = _SAWTOOTH
    value2          = _SINE
    value3          = _SQUARE
    value4          = _TRIANGLE
    output          = O1
```

If you have three or four states, the LED will use different brightness levels
for indicating the current state.

## Momentary buttons

If you just need a momentary button (one that just lights up while you hold it
down), strictly spoken you don't need a `button` circuit. You can directly use
the `B` register, like in this example:

```droid
[algoquencer]
    nextpattern = B1.1
```

Sometimes, however, you may want to make use of some of the features of the
`button` circuit without creating a toggle button. This is easily done by
setting `states = 1`:

```droid
[button]
    states = 1
    button = B1.1
    led = L1.1

[algoquencer]
    nextpattern = L1.1
```

Now you are ready for adding some fun stuff like overlaying one button with
multiple functions (see below) or using the `longpress` output.

## Long and short presses

When creating patches, you will constantly run out of buttons. One way to
increase the effective number of buttons is to map two different actions on a
button depending on wether it is pressed long or short. For this purpose there
is the `longpress` output. Consider the following example:

```droid
[button]
    button    = B1.1
    led       = L1.1
    output    = _SOME_STATE
    longpress = _LONG
```

A button press with a duration below 1.5 secs will toggle the LED `L1.1` as
usual. If you hold the button longer than 1.5 seconds, the output `longpress`
will get high until you release the button. And the state of `L1.1` does not
toggle.

If you don't want the button to toggle any state, but just distinguish between
long and short presses, you can use the `shortpress` output:

```droid
[button]
    button     = B1.1
    longpress = _LONG
    shortpress = _SHORT
```

Note: The output `led` is not used here since we are just interested in the
presses and you cannot really see the LED anyway while you finger is on the
button. If you want the LED anyway, set `states = 1` so it won't toggle:

```droid
[button]
    button     = B1.1
    led        = L1.1
    states     = 1
    longpress = _LONG
    shortpress = _SHORT
```

Using `output` does not do the same as `shortpress`: it always is high as long
as your finger is on the button (and the button is selected).

## Sharing buttons

You can never have too many buttons! It's more likely that you have too few. So
you want to overlay one or more buttons with multiple functions.

They key to this is the `select` input of the `button` circuit. If you patch
this, the circuit will only interact with the actual button and LED if `select`
is active (e.g. set to 1). Otherwise it will continue to output its current
value to `output` and leave the control of the button and the LED to some other
circuit.

The following example uses the button `B1.1`, (which is not overloaded!) for
switching between two "layers" or "banks" of buttons. And in each bank the
button has a different meaning. Note how I use the `negated` output of the
button. That is 0 if the normal output is 1 and vice versa.

In order to keep things short, the bank just consists of the single button
`B1.2`. Of course in practice this wouldn't make sense since you wouldn't
actually save a button, but you get the idea...

```droid
[button]
    button = B1.1
    led = L1.1
    output = _BANK1
    negated = _BANK2

[button]
    select = _BANK1
    button = B3.1
    led = L3.1
    output = _VIRTUAL_BUTTON_1

[button]
    select = _BANK2
    button = B3.1
    led = L3.1
    output = _VIRTUAL_BUTTON_2
```

Note: If you need more than two banks, consider switching with a
[`buttongroup`](buttongroup.md).

## Buttons as logic gates

Here is an important caveat for all you hardcore hackers out there: The `button`
circuit is designed to interface with real buttons that real users press. You
*can* misuse a button as a kind of logic gate, for example for inverting a
signal or even build a super fast oscillator.

Don't do this. Have a look at [`flipflop`](flipflop.md) instead.

Why? In order to optimize the execution speed of your patch, several user
interface circuits are executed at just 12.5% of the normal speed. This saves
valuable time for the execution of more time critical circuits. So instead of
checking buttons at sub-millisecond intervals, your master rather spends its
time in executing your sequencers with a timing as precise as possible.

This means, that `button`, `buttongroup`, `pot` and similar circuits are
executed just every 8th loop cycle.

If you experience any trouble with this "UI slowdown", you can disable it by
using a [`droid`](droid.md) circuit:

```droid
[droid]
    uislowdown = 0
```

## Display

If you have a DB8E display controller, the `button` circuit automatically
displays it's state, whenever `states = 3` or `states = 4`. This visual feedback
makes it easer to select one of the four states than just guessing the
brightness of the LED.

There is no immediate way to assign texts or labels to the four states. If you
want to do this, you can use a combination of [`display`](display.md) and
[`switch`](switch.md) like this:

```droid
[button]
    button = B2.1
    led = L2.1
    states = 4
    output = _WAVEFORM

[switch]
    input1 = "Square"
    input2 = "Triangle"
    input3 = "Sawtooth"
    input4 = "Sine"
    offset = _WAVEFORM
    output = _WAVEFORM_NAME

[display]
    header = "Waveform"
    text = _WAVEFORM_NAME
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `button` (`b`) | gate | | The actual push button. Usually you want to wire this to `B1.1`, `B1.2` and so on: to one of the push buttons of your controllers. Each time that input goes from low to high, the state of the push button will toggle. |
| `onvalue` (`ov`) | CV | `1.0` | Value sent to `output` when the push button is on. You can also use a dynamic signal here. This is an alternative name for the input `value1`. |
| `offvalue` (`fv`) | CV | `0.0` | Value sent to `output` when the push button is off. This is an alternative name for the input `value2`. |
| `value1 ... value4` (`v`) | CV | | The up to four values to output at `output` when the button is on the according state. `value1` is the same as `offvalue` and `value2` is the same as `onvalue`. The default values of these four inputs are 0, 1, 2 and 3, so in many cases you don't need to specify them. |
| `doubleclickmode` (`dm`) | gate | `off` | This input can enable a *double click mode* when set to `1`. In that mode the button only toggles it's constant state if you double press it in a short time. Otherwise it behaves like a momentary button, that inverts the persisted state (which you toggle with the double click). Note: The double clock mode is only makes sense if the number of states is 2. |
| `longpresstime` (`lt`) | CV | `1.5` | The number of seconds after which a button press is considered as a *long press*. |
| `states` (`st`) | integer | `2` | Number of states this button can have. The default value is `2`, which creates a toggle button which changes between on and off at each press. A value of `1` creates a momentary button. Note: If you just need a plain momentary button, you can directly use `B1.1`, `B1.2` and so on. You don't need an extra circuit. But if you want things like overloading (with `select`) or the `longpress` output, this does make sense. The maximum number of states is 4. When the button has 3 or 4 states, every press will switch to the next state and then back to the first state again. |
| `startvalue` (`sv`) | integer | `0` | State of the push button when you switch on your system or on a trigger to `clear`. If you have three states, the start value needs to be 0, 1 or 2. With four states, it can also be 3. |
| `display` (`dy`) | integer | `1` | This parameter selects the number of the DB8E display controller, where any change of this circuit's state will show up. Per default, the first DB8E available is used. Set this to 2 if you have a second DB8E and want to use that. Note: any other controller is not being counted here, it's really just the number of the DB8E as counted from the master module. Set this to 0 to suppress any display output from this circuit. |
| `header` (`hr`) | text | ☞ smart | This text is displayed at the top of the DB8E display whenever this circuit shows up in the display. If you omit this, an automatic title is used. For example if the output of this circuit is fed into an internal patch cable, the name of this cable is displayed. Setting `header = ""` or `header = 0` removes the header and just displays the actual value. |
| `select` (`s`) | integer | ☞ smart | The `select` input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to 1). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. If left unpatched the circuit is always selected. |
| `selectat` (`sa`) | integer | ☞ smart | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is 0, the circuit will be active if `select` is exactly 0 instead of a positive gate signal. In some cases this is more conventient. |
| `preset` (`pr`) | integer | ☞ smart | This is the preset number to save or to load. Note: the first preset has the number 0, not 1! For the whole story on presets please refer to the manual ([basics](../basics.md)). This circuit has 16 presets, so this number ranges from 0 to 15. |
| `loadpreset` (`lp`) | trigger | | A trigger here loads a preset. As a speciality you can use the trigger for selecting a preset at the same time. |
| `savepreset` (`sp`) | trigger | | A trigger here saves a preset. |
| `clear` (`cl`) | trigger | | A trigger here loads the default start state into the circuit. The presets are not affected, unless you use direct preset switching with the `preset` input and without triggers. And that case the current preset is also cleared. |
| `clearall` (`ca`) | trigger | | A trigger here loads the default start state into the circuit and into all of its presets. |
| `dontsave` (`dos`) | gate | `0` | If you set this to 1, the state of the circuit will not saved to the SD card and not loaded from the SD card when the Droid starts. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `led` (`l`) | CV | When the button state is on, a value of `1.0` will be sent to that output – regardless of the values in `onvalue` and `offvalue`. If the number of states is 3 or 4 the output get's intermediate values so the attached LED will be dimmed into different brightness levels. Usually you wire that output to a LED register, e.g. to `L1.1`, `L1.2` and so on. |
| `output` (`o`) | CV | This output will output the current button states. This is usually 0 for off and 1 for on. If `states` is 3 or 4, the values 2 or 3 are output for the additional states. You can modify all four values with the inputs `offvalue`/`value1`, `onvalue`/`value2`, `value3` and `value4`. Note: if you haven't changed any of these inputs and `states` is unchanged or 1 or 2, the `led` output will output the same values. |
| `inverted` (`iv`) | CV | The same as `output`, but sends `onvalue` when the button is off and `offvalue` when the button is on. If `states` is 3 or 4, the order of the four output values will be mirrored (probably a feature that is rarely of any use). |
| `negated` (`n`) | gate | Similar to `inverted`, but always sends 1 when the button is off and 0 when the button is on – independent of the values of `onvalue` and `offvalue`. When `states` is 3 or 4, this output will be 1 if the button is off and 0 in the other three states. |
| `longpress` (`lop`) | gate | Goes from 0 to 1, when the button is pressed and hold for at least 1.5 seconds. If this output is used, the effect of toggling the button's state is delayed until the button is *released*. When it's released after 1.5 secs, no toggling happens. This will avoid double actions for long presses. |
| `shortpress` (`shp`) | trigger | Emits a trigger, when the button is pressed, regardless of the settings of `states`. If at the same time `longpress` is used (which is the whole point in this output), the trigger is delayed until the button is released and only sent, if it was not a long press. |

## See also

- [`buttongroup`](buttongroup.md) — switch between more than two banks of buttons.
- [`flipflop`](flipflop.md) — use this instead of misusing a button as a logic gate.
- [`droid`](droid.md) — disable the UI slowdown with `uislowdown`.
- [`display`](display.md) and [`switch`](switch.md) — label the four states on a DB8E display.
- [`lfo`](lfo.md) — used in the wave-form switching examples.
