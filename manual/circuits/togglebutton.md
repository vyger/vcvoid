---
circuit: togglebutton
title: Create on/off buttons
obsolete: true
ram_bytes: 48
manual_pages: [400, 401]
category: controller-ui
tags: [button, toggle, on-off, latch, led, switch, layers, obsolete]
see_also: [button, cvlooper, clocktool, lfo]
impl_difficulty: easy
controller_binding: controller-required
verification: requires-human
spec_gap: false
difficulty_note: Obsolete toggle/latch logic with on/off values, double-click mode, up-to-4 switch layers and flash persistence; simple state machine bound to a controller button and its LED.
verification_note: "Toggle logic and output/led/negated/inverted register values are verifiable headless or Rack-automated by driving the button param and reading registers. The physical button-LED illumination it drives is visual, so confirming that part is partial requires-human."
---

# togglebutton — Create on/off buttons

> **⚠️ OBSOLETE.** This circuit has been superseded by the new circuit
> [`button`](button.md), which can do all `togglebutton` can do and much more. So
> `togglebutton` will be removed soon.

This small utility circuit converts a normal push button into a toggle button
that is either *on* or *off*. It toggles its state every time the button is being
pressed. It even can persist the current state of the button in the DROID's
internal flash memory, so at the next time you start your modular the button will
have the same state as just before you switched it off.

Typically you will wire `button` to one of your controllers' buttons like `B1.1`
and `led` to the LED in that button (`L1.1`). `led` will then always visualise
the current state of the button. As a side effect the LED register `L1.1` will
store the button state as a value `0` or `1` and hence can be used by some other
DROID as an input.

## Example

Here is a typical example. The button is being used for enabling the loop in the
CV looper:

```droid
[togglebutton]
    button = B1.4
    led = L1.4

[cvlooper]
    loop = L1.4
```

If you do not want the state of the button to be persisted in the DROID's flash
memory then use `startvalue` for setting a start value. This make sense for the
CV looper since the loop is apparently empty anyway if you start your DROID. By
the way: `off` is a synonym for `0`.

```droid
[togglebutton]
    button = B1.4
    led = L1.4
    startvalue = off

[cvlooper]
    loop = L1.4
```

Since a multiplication with `0` or `1` can switch off or on a signal you can use
the LED register directly for enabling a signal. The next example uses a button
for switching between 0 V and the output of an LFO:

```droid
[togglebutton]
    button = B1.4
    led = L1.4

[lfo]
    level = L1.4 # 0 or 1
    sine = O1
```

Usually the toggle button switches between the two values `0` and `1`. Sometimes
you need different values. Therefore there are the two inputs `offvalue` and
`onvalue` for two alternative values for these two states and the output
`output1` where you can fetch that value (since `led` will continue to send `0`
or `1` in order for the LED to work properly). Here is an example for a toggle
button that switches a clock divider between `2` and `4`:

```droid
[togglebutton]
    button = B1.4
    led = L1.4
    offvalue = 2
    onvalue = 4
    output = _CLOCK_DIV

[clocktool]
    input = G1 # external clock
    output = G2
    divide = _CLOCK_DIV
```

Of course `offvalue` and `onvalue` are CV controllable. How can make this sense?
Well — as they can take variable inputs you can use a togglebutton for directly
switching between two different input CV signals. The following example will send
two different wave forms of an LFO to `O1`. The button `B3.1` switches between
sawtooth and sine:

```droid
[lfo]
    hz = 2
    sawtooth = _SAWTOOTH
    sine = _SINE

[togglebutton]
    button = B3.1
    led = L3.1
    offvalue = _SAWTOOTH
    onvalue = _SINE
    output = O1
```

Hint: if you need to have not only two but three or four different states for
your button then have a look at the circuit [`button`](button.md).

## Buttons with up to four layers

The toggle button can overloaded with up to four functions. For switching
between these layers you need a CV. This example assigned three different layers
to one button. Each layer has its own state.

```droid
[togglebutton]
    button = B1.4
    led = L1.4
    output1 = _ENABLE_LOOP
    output2 = _FANCY_STUFF
    output3 = _FOO_BAR
    switch = I1 * 2
```

Now if `I1` is near zero volts, then the button behaves like in the previous
example. But when you set it to 5 V (resulting in a number of `0.5` which is
multiplied by 2 and thus evaluates to `1`), then a second copy of the button is
activated with its own state. The LED now shows the state of that second button
which `output` will outputs the value of the first button.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `button` (`b`) | gate | | The actual push button. Usually you want to wire this to `B1.1`, `B1.2` and so on: to one of the push buttons of your controllers. Each time that input goes from low to high the state of the push button will toggle. |
| `reset` (`r`) | trigger | | A positive trigger edge here will reset the button into the state "not pressed" – regardless of its current state. |
| `onvalue` (`ov`) | CV | `1.0` | Value sent to `output` when the push button is on. Setting this to a different value than the default value saves you attenuating its value later on when you use it as a CV. |
| `offvalue` (`fv`) | CV | `0.0` | Value sent to `output` when the push button is off. |
| `doubleclickmode` (`dm`) | gate | `off` | This input can enable a *double click mode* when set to `1`. In that mode the button only toggles it's constant state if you double press it in a short time. Otherwise it behaves like a momentary button, that inverts the persisted state (which you toggle with the double click). |
| `startvalue` (`sv`) | gate | | State of the push button when you switch on your system. Setting this to `on` or `off` will force the button into that state. Using this jack disables the persistence of the state! In switched mode this will be used for the other button layers as well. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `led` (`l`) | gate | When the button's state is on a value of `1.0` will be sent to that output – regardless of the values in `onvalue` and `offvalue`. Usually you will wire this jack to the LED within the button, e.g. to `L1.1`, `L1.2` and so on. |
| `output` (`o`) | CV | This jack will output either `onvalue` or `offvalue` depending on the state of the 1st … 4th button. If you have not wired those inputs then this is the same as the `led` output. |
| `inverted` (`iv`) | CV | The same as `output1`, but sends `onvalue` when the button is off and `offvalue` when the button is on. Note: there is no inverted version of `output2` … `output4`. |
| `negated` (`n`) | gate | Similar to `inverted`, but always sends `1` when the button is off and `0` when the button is on – independent of the values of `onvalue` and `offvalue`. |

## See also

- [`button`](button.md) — the modern replacement for this circuit.
