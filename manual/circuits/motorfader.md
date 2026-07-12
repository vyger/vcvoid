---
circuit: motorfader
title: Create virtual fader in M4 controller
obsolete: false
ram_bytes: 120
manual_pages: [343, 344, 345, 346]
category: controller-ui
tags: [motor-fader, m4, preset, force-feedback, notches, pitch-bend, haptic, db8e, select, fader, sharewithnext]
see_also: [buttongroup, button]
impl_difficulty: hard
controller_binding: controller-required
verification: requires-human
spec_gap: false
difficulty_note: Fader value plus preset/select/overlay logic is implementable, but the defining motorised force-feedback and notch detents have no VCV Rack equivalent.
verification_note: "Requires human (partial): the output CV for a given fader position is headless-verifiable, but motorised position recall, notch detents and force feedback can only be judged by a person operating an M4 — Rack has no motorised-fader widget. The gap is hardware, not documentation; the spec itself is clear."
---

# motorfader — Create virtual fader in M4 controller

The circuit provides the most basic access to motor faders and supports
switching between presets, overlayed functions and force feedback.

For the basics about these ideas and the M4 in general, please read the
introduction to the M4 in the manual ([hardware](../hardware.md)).

## Presets

Let's start with presets and make a simple example with one P2B8 and one M4
controller. First we need to declare both in our patch:

```droid
[p2b8]

[m4]
```

Let's use the first fader as a simple CV source to be output on `O1`. And four
buttons should select four different presets of that fader. Those are grouped
into a button with the circuit [`buttongroup`](buttongroup.md):

```droid
[buttongroup]
    button1 = B1.1
    button2 = B1.2
    button3 = B1.3
    button4 = B1.4
    led1 = L1.1
    led2 = L1.2
    led3 = L1.3
    led4 = L1.4
    output = _PRESET
```

This circuit will switch between the values 0, 1, 2 and 3 and output that number
to the internal cable `_PRESET`. Now let's add the fader definition:

```droid
[motorfader]
    fader = 1
    preset = _PRESET
    output = O1
```

That's really all. `fader = 1` selects the first motor fader in your setup. All
faders are simply enumerated, so `fader = 7` would select the third fader on the
second M4.

The output `O1` now always outputs the current setting of the fader. The range
is 0 V … 10 V – just like with pots of the controllers.

Hitting the buttons will switch to one of the four presets and move the fader to
the position corresponding to current value of that preset.

## Faders with multiple functions

The second way to use the motor faders is to assign multiple functions to one
fader and then switch between those functions. The crucial difference to the
presets is, that for every function there is a *dedicated output*.

Let's now change our example so that we use one fader controlling four CV
sources, but without any presets for the while. The start is the same (just we
renamed the internal cable to `_FUNCTION`):

```droid
[buttongroup]
    button1 = B1.1
    button2 = B1.2
    button3 = B1.3
    button4 = B1.4
    led1 = L1.1
    led2 = L1.2
    led3 = L1.3
    led4 = L1.4
    output = _FUNCTION
```

Now we need a separate motorfader circuit for each function. And instead of
choosing a preset, we need to `select` each circuit when the active button
selects its function:

```droid
[motorfader]
    fader = 1
    select = _FUNCTION
    selectat = 0
    output = O1

[motorfader]
    fader = 1
    select = _FUNCTION
    selectat = 1
    output = O2

[motorfader]
    fader = 1
    select = _FUNCTION
    selectat = 2
    output = O3

[motorfader]
    fader = 1
    select = _FUNCTION
    selectat = 3
    output = O4
```

As you can see: each fader has a `selectat` input matching one of the buttons of
the buttongroup. And each fader also sends its output to one of the main outputs
of the master.

There is one possible simplification: Instead of using `_FUNCTION` and
`selectat`, we also could use the LED outputs of the button group directly:

```droid
[buttongroup]
    button1 = B1.1
    button2 = B1.2
    button3 = B1.3
    button4 = B1.4
    led1 = L1.1
    led2 = L1.2
    led3 = L1.3
    led4 = L1.4

[motorfader]
    fader = 1
    select = L1.1
    output = O1

[motorfader]
    fader = 1
    select = L1.2
    output = O2

[motorfader]
    fader = 1
    select = L1.3
    output = O3

[motorfader]
    fader = 1
    select = L1.4
    output = O4
```

## Notches

Maybe the coolest feature of the M4 is the haptic feedback. The M4 uses its
motors in order to give you force feedback. This is done in various forms.

The most useful form is to use artifical "notches" or "dents". Try that out by
setting `notches` to a number, e.g. 8:

```droid
    notches = 8
```

This changes the behaviour of the fader in two ways:

1. The output value is now a discrete whole number from 0 up to 7.
2. When you move the fader you feel eight artificial dents. That's really hard
   to explain. Try it out!

These notches are super helpful especially in live performances. You instantly
*feel* where you are. You don't need any visual feedback. You can very reliably
set a value without looking.

The maximum number of notches is 201. But if you select more than 25 notches,
the force feedback is turned off as the notches would get too small to work.

There are also two other variants of force feedback:

### Binary switch

If you set `notches = 2`, you turn the fader into a binary switch. The output
will be 0 if the fader is in the bottom position and 1 on the top. Just move the
fader away from its position and it will immediately snap to the other side.

### Pitch bend wheel

Setting `notches = 1` will convert the fader into a kind of pitch bend wheel. It
always wants to stay in the middle, where it outputs a value of 0.5. If you move
it away from the center position, it creates a force back to the center that is
the greater the nearer you are to the top or bottom. As soon as you release it,
it snaps back to the middle.

## Modifying one value with two virtual faders

The sharing of virtual faders is a bit more tricky to explain and you probably
won't need it. It means that you use two motorfader circuits for controlling the
same output value. Why would you do this?

I have added that feature when building a motor fader based MIDI control for my
audio interface. I have one mode where every of eight faders controls the main
volume of one of eight voices. And then I have a "drill down" for each voice,
where the first fader is the main volume, the second fader the head phone, the
third the volume of an aux channel and so on.

So now I can control the volume of voice 3 either with the third fader in the
"global" volume control or with the first fader the drill down of voice 3. This
leads to an output collision since two circuits would try to modify the same
output, even if always just one of the two motor fader circuits is selected.

The solution to this problem is the `sharewithnext` input. Put the two
motorfader circuits next to each other into your patch. Put a
`sharewithnext = 1` into the first one. Don't use the `output` there. Now both
virtual faders will control the output that is defined in the second motorfader
circuit:

```droid
[motorfader]
    fader = 1
    select = _GLOBAL
    sharewithnext = 1

[motorfader]
    fader = 3
    select = _DRILLDOWN_3
    output = _VOLUME_3
```

Note: if you are using notches, make sure that both motorfader circuits have the
same number of notches!

## Display

If you have a DB8E display controller, the motorfader circuit automatically
updates the display whenever the virtual value of the fader changes (by user
interaction).

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `fader` (`f`) | integer | `1` | The number of the motor fader to use, starting with `1` for the first fader in the first M4. `5` selects the first fader in the second M4 and so on. |
| `startvalue` (`sv`) | CV | `0.0` | This sets the value the fader gets when you start this circuit this first time or when a trigger to `clear` happens. |
| `notches` (`n`) | integer | `0` | Number of artifical notches. `0` disables the notches. `1` creates a pitch bend wheel. `2` creates a binary switch with the output values 0 and 1. Higher number create that number of notches. E.g. `8` creates eight notches and output will output one of the value 0, 1, … 8. The maximum allowed number is `25`. |
| `outputscale` (`os`) | CV | `1.0` | The final output is multiplied with this value. It's a convenient method for scaling up and down the pot range. |
| `outputoffset` (`oo`) | CV | `0.0` | After scaling the fader value with `outputscale`, this value is being added before sending the result to the output. |
| `ledvalue` (`lv`) | CV | | When you use this input, it will override the brightness of the LED below the fader, but just when this circuit is selected. And there is a special trick: When you use a negative value for `ledvalue`, it switches to a magical mode. Here the LED is at full brightness when the current setting of the motorfader matches its `startvalue`, whereas the setting of `ledvalue` is used (made positive) in all other cases. |
| `ledcolor` (`lc`) | CV | | When you use this input, it will set the color of the LED below the fader, when the circuit is selected. If the LED is off, this setting has no impact. |
| `sharewithnext` (`sw`) | gate | `0` | If set to `1`, the output `output` will not be used but the circuit shares its output with the next motorfader circuit. |
| `display` (`dy`) | integer | `1` | This parameter selects the number of the DB8E display controller, where any change of this circuit's state will show up. Per default, the first DB8E available is used. Set this to `2` if you have a second DB8E and want to use that. Note: any other controller is not being counted here, it's really just the number of the DB8E as counted from the master module. Set this to `0` to suppress any display output from this circuit. |
| `header` (`hr`) | text | ☞ smart | This text is displayed at the top of the DB8E display whenever this circuit shows up in the display. If you omit this, an automatic title is used. For example if the output of this circuit is fed into an internal patch cable, the name of this cable is displayed. Setting `header = ""` or `header = 0` removes the header and just displays the actual value. |
| `select` (`s`) | integer | ☞ smart | The `select` input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to `1`). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | ☞ smart | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is `0`, the circuit will be active if `select` is exactly `0` instead of a positive gate signal. In some cases this is more convenient. |
| `preset` (`pr`) | integer | ☞ smart | This is the preset number to save or to load. Note: the first preset has the number `0`, not `1`! For the whole story on presets please refer to the manual ([basics](../basics.md)). This circuit has 8 presets, so this number ranges from `0` to `7`. |
| `loadpreset` (`lp`) | trigger | | A trigger here loads a preset. As a speciality you can use the trigger for selecting a preset at the same time. |
| `savepreset` (`sp`) | trigger | | A trigger here saves a preset. |
| `clear` (`cl`) | trigger | | A trigger here loads the default start state into the circuit. The presets are not affected, unless you use direct preset switching with the `preset` input and without triggers. And that case the current preset is also cleared. |
| `clearall` (`ca`) | trigger | | A trigger here loads the default start state into the circuit and into all of its presets. |
| `dontsave` (`dos`) | gate | `0` | If you set this to `1`, the state of the circuit will not saved to the SD card and not loaded from the SD card when the Droid starts. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | Output the current value of the virtual motor fader (don't use this if you are using `sharewithnext`). |
| `button` (`b`) | gate | This output provides you with the current state of the touch button below the fader, but only if the circuit is selected. While you could do this with an extra [`button`](button.md) circuit, using this output is more convenient in some situations. While the circuit is not selected, the output is set to `0`. |

## See also

- [`buttongroup`](buttongroup.md) — groups buttons for preset / function selection.
- [`button`](button.md) — alternative way to read the fader's touch button.
