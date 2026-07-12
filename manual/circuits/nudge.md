---
circuit: nudge
title: Modify a value in steps using two buttons
obsolete: false
ram_bytes: 152
manual_pages: [352, 353, 354]
category: controller-ui
tags: [nudge, buttons, increment, decrement, step, cv-source, octave-switch, wrap, offset, leds, db8e, display]
see_also: [euklid, button]
impl_difficulty: moderate
controller_binding: controller-required
verification: headless
spec_gap: false
difficulty_note: Two-button increment/decrement is simple, but min/max clamping, wrap, offset, both-pressed reset, the gradient LED indicator, presets and optional DB8E display add up.
verification_note: "Step/clamp/wrap/reset logic is deterministic and checkable headless from scripted button edges; the gradient LED brightness and DB8E value display are visual and need a human or driven-in-Rack pass."
---

# nudge — Modify a value in steps using two buttons

This small utility allows you to modify a value up and down in fixed steps using
two buttons. This value can be persistent so it survives a power cycle.

Here is an example for a simple CV source that outputs a value between -2 V and
2 V:

```droid
[nudge]
    minimum    = -2V
    maximum    = 2V
    amount     = 1V
    buttonup   = B1.1
    buttondown = B1.3
    ledup      = L1.1
    leddown    = L1.3
    output     = O1
```

**Note:** If you press both buttons at the same time, the value will be reset to
its start value.

You can extend this into an octave switch by using the input `offset`, which
will be added to the output:

```droid
[nudge]
    minimum    = -2V
    maximum    = 2V
    amount     = 1V
    buttonup   = B1.1
    buttondown = B1.3
    ledup      = L1.1
    leddown    = L1.3
    output     = O1
    offset     = I1
```

If you now feed some V/Oct source, such as the pitch output of a sequencer, to
`I1`, it will be shifted up and down for up to two octaves.

Another application might be to fine tune an oscillator. Here you set the nudge
steps (set by `amount`) a lot smaller. Also it is allowed to leave out `minimum`
and `maximum` and thus make the possible range unrestricted. Note: `1V / 1200`
means essentially a step size of 1⁄1200 of an octave, which is 1⁄100 of a
semitone, which is also known as *one cent*:

```droid
[nudge]
    amount     = 1V / 1200
    buttonup   = B1.1
    buttondown = B1.3
    ledup      = L1.1
    leddown    = L1.3
    output     = O1
    offset     = I1
```

A third application could be a button for selecting a certain input number for –
let's say – an euclidean rhythm pattern:

```droid
[nudge]
    amount = 1
    buttonup = B1.1
    ledup = L1.1
    minimum = 3
    maximum = 7
    wrap = 1
    output = _BEATS

[euklid]
    clock      = G1
    length     = 16
    beats      = _BEATS
    output     = G3
```

Note: Here only one button is wired. In addition `wrap` is set to `1`, which
means that after reaching the maximum value, the next value will be the minimum
value. Here each press of the button `B1.1` forwards the number of beats in the
matter 3 → 4 → 5 → 6 → 7 → 3 and so on...

## Understanding the LEDs

By nudging the value below the center value the `buttonup` LED will be off and
the brightness of the `buttondown` LED will gradually increase indicating how
much the value is set below this center value. It remains maximally bright at the
minimum.

Vice versa by nudging the value above the center value the `buttondown` LED will
be off and the brightness of the `buttonup` LED will gradually increase
indicating how much the value is set above this center value. It remains
maximally bright at the maximum.

And if the value is exactly in the middle between maximum and minimum, both LEDs
are maximally bright. Here you have to have in mind that this must be exactly in
the middle. Of course, this only works if the distance between `maximum` and
`minimum` is an exact odd number of `amount`s.

## Display

If you have a DB8E display controller, the `nudge` circuit automatically updates
the display whenever you nudge or reset to the start value by pressing both
buttons at once. It displays the output value.

If you use integer numbers for `amount` and `startvalue` and an integer value is
selected (which is then very likely), the display simply shows that number.

If your nudged value might be a non-integer fraction, the display automatically
switches to its value display mode, where you can use the buttons of the DB8E to
tweak the number format, e.g. a simple bar.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `buttonup` (`u`) | gate | | Button for nudging the value up by one step. |
| `buttondown` (`d`) | gate | | Button for nudging the value down by one step. |
| `amount` (`am`) | CV | `0.1` | Amount to modify the value by on each press. This must be a value > 0. |
| `startvalue` (`sv`) | CV | `0.0` | The value this circuit starts with or is being reset to if you use the `clear` input. |
| `minimum` (`m`) | CV | ☞ smart | The minimum possible value. If you do not wire this, the value can go down infinitely. |
| `maximum` (`x`) | CV | ☞ smart | The maximum possible value. If you do not wire this, the value can go up infinitely. |
| `wrap` (`w`) | gate | `0` | Set this to 1 in order to have the value wrap around if the minimum or the maximum has been exceeded. Note: `wrap` does only work if you set `minimum` and `maximum`. |
| `offset` (`of`) | CV | `0.0` | This value is being added to the output. |
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
| `ledup` (`lu`) | gate | Wire this to the LED in the button for nudging up. It will indicate the current value. |
| `leddown` (`ld`) | gate | Wire this to the LED in the button for nudging down. It will indicate the current value. |
| `output` (`o`) | CV | The output of the current value plus value of `offset`. |
| `changed` (`c`) | trigger | Emits a trigger whenever the value was nudged up or nudged down or reset by pressing both buttons at the same time. The trigger is only sent if there was an actual change. If this was not the case because the maximum or minimum was already reached or the value was already at its starting position on reset, no trigger is emitted. |
