---
circuit: fadermatrix
title: Matrix of up to 4x4 virtual motor faders
obsolete: false
ram_bytes: 712
manual_pages: [250, 251, 252, 253, 254, 255]
category: controller-ui
tags: [motor fader, m4, matrix, row, column, rowcolumn, notches, presets, db8e, adsr, buttongroup]
see_also: [motorfader, faderbank, buttongroup, contour]
impl_difficulty: hard
controller_binding: controller-required
verification: requires-human
spec_gap: false
difficulty_note: 4x4 virtual motor-fader matrix on the M4 with row/column selection, per-column notches, presets, and LEDs; motorized haptics have no VCV analog.
verification_note: "Row/column mapping, notch quantization, and preset logic are partially headless-testable, but motor-fader recall/position feedback, touch buttons, and LEDs require human interaction; gap is hardware, not docs."
---

# fadermatrix — Matrix of up to 4x4 virtual motor faders

This circuit is a clever way of controlling a four by four matrix of parameters,
which allows you to select either a row or a column.

As an example let's think of a bank of four envelope generators. Each of them
has the settings attack, decay, sustain and release (ADSR). That nicely forms a
4×4 matrix:

| | | | |
|---|---|---|---|
| Att 1 | Dec 1 | Sus 1 | Rel 1 |
| Att 2 | Dec 2 | Sus 2 | Rel 2 |
| Att 3 | Dec 3 | Sus 3 | Rel 3 |
| Att 4 | Dec 4 | Sus 4 | Rel 4 |

The `fadermatrix` has 16 outputs that map to these matrix positions:

| | | | |
|---|---|---|---|
| `output11` | `output12` | `output13` | `output14` |
| `output21` | `output22` | `output23` | `output24` |
| `output31` | `output32` | `output33` | `output34` |
| `output41` | `output42` | `output43` | `output44` |

Now when you design a patch for controlling these 16 parameters with 4 motor
faders you basically have the choice of selecting *rows* or *columns*! One way
would be to always select one of the envelopes to be displayed and edited on
your faders, for example the second one (selecting the row Att 2 … Rel 2).

An alternative would be to have control over all decay parameters of the four
envelopes — in order to shape four synth voices at the same time without
switching between those (selecting the column Dec 1 … Dec 4).

With [`faderbank`](faderbank.md) you would have to decide for one of the two
options. But with `fadermatrix` you can have both at the same time.

With the `rowcolumn` input you can select each column and each row as follows:

| rowcolumn | selects |
|-----------|---------|
| 0 | Att 1, Dec 1, Sus 1, Rel 1 |
| 1 | Att 2, Dec 2, Sus 2, Rel 2 |
| 2 | Att 3, Dec 3, Sus 3, Rel 3 |
| 3 | Att 4, Dec 4, Sus 4, Rel 4 |
| 4 | Att 1, Att 2, Att 3, Att 4 |
| 5 | Dec 1, Dec 2, Dec 3, Dec 4 |
| 6 | Sus 1, Sus 2, Sus 3, Sus 4 |
| 7 | Rel 1, Rel 2, Rel 3, Rel 4 |

## Example

If you create a [`buttongroup`](buttongroup.md) with eight buttons and patch the
output to the `rowcolumn` input, you have access to all four rows and columns.
The nice thing about the `buttongroup` is that it automatically outputs the
values from 0 to 7. Here is an example:

```droid
[p2b8]
[m4]

[buttongroup]
    button1 = B1.1
    button2 = B1.3
    button3 = B1.5
    button4 = B1.7
    button5 = B1.2
    button6 = B1.4
    button7 = B1.6
    button8 = B1.8
    led1 = L1.1
    led2 = L1.3
    led3 = L1.5
    led4 = L1.7
    led5 = L1.2
    led6 = L1.4
    led7 = L1.6
    led8 = L1.8
    output = _ROWCOLUMN
```

Now we add a `fadermatrix`. We send all 16 outputs to internal patch cables to
be picked up later by four [`contour`](contour.md) circuits:

```droid
[fadermatrix]
    rowcolumn = _ROWCOLUMN
    output11 = _ATTACK_1
    output12 = _DECAY_1
    output13 = _SUSTAIN_1
    output14 = _RELEASE_1
    output21 = _ATTACK_2
    output22 = _DECAY_2
    output23 = _SUSTAIN_2
    output24 = _RELEASE_2
    output31 = _ATTACK_3
    output32 = _DECAY_3
    output33 = _SUSTAIN_3
    output34 = _RELEASE_3
    output41 = _ATTACK_4
    output42 = _DECAY_4
    output43 = _SUSTAIN_4
    output44 = _RELEASE_4
```

And here is the example for the first contour (the other three are similar):

```droid
[contour]
    gate = I1
    attack = _ATTACK_1
    decay = _DECAY_1
    sustain = _SUSTAIN_1
    release = _RELEASE_1
    output = O1
```

If you don't want to waste 8 buttons for just switching, you can also use a pot
and scale it to the range of 0 … 7:

```droid
    rowcolumn = P1.1 * 7
```

And of course the rotary switch of an S10 would also be a perfect match, since
it outputs exactly the number from 0 to 7.

## Notches

As discussed in [`motorfader`](motorfader.md), faders can be set to have
artificial notches. Also in the fader matrix you can set notches. Here the idea
is that every parameter in the same *column* of the matrix has the same number
of notches. Example:

```droid
    notches3 = 8
```

This sets all four parameters in column 3 to have eight notches. This affects
the four outputs `output13`, `output23`, `output33` and `output43` so that they
get notches when selected and also change their output behaviour to outputting
one of the values 0, 1, 2 … 7.

As you can see the matrix always assumes that you edit four similar *things* with
four parameters each. Every row of the matrix is one such thing. Every column is
one parameter.

## Smaller matrices

You also can create smaller matrices, for example 3×3. Simply omit the outputs
`output14`, `output24`, `output34`, `output44`, `output41`, `output42` and
`output43` in that case. Also 2×2 is possible.

Because we always need to be able to swap rows and columns, those numbers always
have to be identical. So you cannot create a 3×4 matrix, for example.

## Larger matrices

If you have eight faders, you can create even larger matrices. An 8×8 matrix can
be created by four `fadermatrix` circuits. Here you need some extra logic. At any
time exactly two of the circuits must be selected. Use the `select` inputs in
combination with `rowcolumn` in order to set this up (left as an exercise) ;-)

## Display

If you have a DB8E display controller, the `fadermatrix` circuit automatically
updates the display whenever you move one of the faders. The title in the display
is derived from the target of the `outputN` parameter. If that goes into a cable,
the name of that cable is used as the title.

You can use `header` to override this. But there is just one `header` input. If
that is always the same, it's probably not very helpful.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `firstfader` (`f`) | integer | `1` | First M4 fader of the virtual fader matrix (starting with 1). |
| `rowcolumn` (`rc`) | integer | `0` | Currently selected row or column: 0 → control output11, output12, output13, output14; 1 → output21…output24; 2 → output31…output34; 3 → output41…output44; 4 → output11, output21, output31, output41; 5 → output12, output22, output32, output42; 6 → output13, output23, output33, output43; 7 → output14, output24, output34, output44. |
| `notches1 … notches4` (`n`) | integer | `0` | Number of artificial notches in the respective column. For example `notches2` controls the notches of `output12`, `output22`, `output32` and `output42`. `0` disables the notches; `1` creates a pitch bend wheel; `2` creates a binary switch; `3` creates a switch with four positions; `8` creates eight notches; `25` creates 25 notches. Enabling notches also changes the output value. When you have two or more notches, the output values become discrete. For example with four notches the output will be 0, 1, 2 or 3. Note: The maximum number of notches is 201. But if you select more than 25 notches, the force feedback is turned off as the notches would get too small to work. |
| `startvalue1 … startvalue4` (`sv`) | CV | | These inputs allow to set a defined start value for each column. When the DROID starts first and there is either no saved state or state saving is disabled via `dontsave = 1`, these start values are used. Also a trigger to `clear` loads the start values. There is one start value for each column. All rows share the same start value for a column. |
| `ledvalue11 … ledvalue14` (`l1`) | CV | | With these inputs you can address the LEDs below the virtual faders of `output11 … output14`. As opposed to using direction (e.g. `L1.1`), these inputs will only affect the LED if the according output is selected. |
| `ledvalue21 … ledvalue24` (`l2`) | CV | | With these inputs you can address the LEDs below the virtual faders of `output21 … output24`. As opposed to using direction (e.g. `L1.2`), these inputs will only affect the LED if the according output is selected. |
| `ledvalue31 … ledvalue34` (`l3`) | CV | | With these inputs you can address the LEDs below the virtual faders of `output31 … output34`. As opposed to using direction (e.g. `L3.2`), these inputs will only affect the LED if the according output is selected. |
| `ledvalue41 … ledvalue44` (`l4`) | CV | | With these inputs you can address the LEDs below the virtual faders of `output41 … output44`. As opposed to using direction (e.g. `L4.2`), these inputs will only affect the LED if the according output is selected. |
| `ledcolor1 … ledcolor4` (`lc`) | CV | | Sets the color of the LEDs below the faders if `ledvalueXY` is used. There are just four inputs since every column of outputs has the same LED color (in order to identify them). The color works as with the R registers for the LEDs on the master module. |
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
| `output11 … output14` (`o1`) | CV | Outputs for the CV values of the first row of parameter. |
| `output21 … output24` (`o2`) | CV | Outputs for the CV values of the second row of parameter. |
| `output31 … output34` (`o3`) | CV | Outputs for the CV values of the third row of parameter. |
| `output41 … output44` (`o4`) | CV | Outputs for the CV values of the fourth row of parameter. |
| `button11 … button14` (`b1`) | gate | Give access to the state of the touch button below the faders when the respective output in the first row is selected. |
| `button21 … button24` (`b2`) | gate | Give access to the state of the touch button below the faders when the respective output in the second row is selected. |
| `button31 … button34` (`b3`) | gate | Give access to the state of the touch button below the faders when the respective output in the third row is selected. |
| `button41 … button44` (`b4`) | gate | Give access to the state of the touch button below the faders when the respective output in the fourth row is selected. |

## See also

- [`motorfader`](motorfader.md) — a single virtual motor fader.
- [`faderbank`](faderbank.md) — a one-dimensional bank of virtual faders.
- [`buttongroup`](buttongroup.md) — handy for driving the `rowcolumn` input.
