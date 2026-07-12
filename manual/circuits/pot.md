---
circuit: pot
title: Helper circuit for pots
obsolete: false
ram_bytes: 128
manual_pages: [362, 363, 364, 365, 366, 367, 368, 369]
category: controller-ui
tags: [pot, potentiometer, notch, slope, virtual-pot, preset, led-gauge, bipolar, hemisphere, select, overlay, ui]
see_also: [notchedpot, switchedpot, buttongroup, math, compare, droid, display, encoderbank]
impl_difficulty: moderate
controller_binding: controller-required
verification: rack-automated
spec_gap: false
difficulty_note: Broad feature set (notch, slope, hemispheres, discrete, virtual-pot pickup, 16 presets, LED gauge) but every behavior is precisely specified; needs a real pot to drive it.
verification_note: "Rack-automated: drive the bound pot param and read outputs to check notch/slope/hemisphere math and the virtual-pot pickup algorithm; LED-gauge color/position is a visual detail needing a human spot-check."
---

# pot — Helper circuit for pots

This circuit adds plenty of functionality to the controller pots in one circuit.
It helps with various tasks. It replaces the former circuits
[`notchedpot`](notchedpot.md) and [`switchedpot`](switchedpot.md) and these are
also the main applications of `pot`: the simulation of a precise center dent
(notch) and the sharing of one pot for several different functions.

> This circuit is designed to build user interfaces. It is executed at a lower
> speed. Don't use it for other purposes.

## Convert a knob to bipolar output voltage

Let's start with some simple features. There are a couple of useful outputs, all
of which you could do externally by use of some math. The following example
converts a pot (which is ranging from 0 to 1) to a bipolar pot ranging from -1 to
+1 (or -10 V to +10 V if you send it to an output):

```droid
[pot]
    pot         = P1.1
    bipolar     = O1 # Send -10V ... +10V to O1
```

Have a look into the table of jacks below about further useful things like
splitting the pot's way in two halfs.

## Center notch

`pot` can simulate a potentiometer with a notch at the center. It helps to
exactly select the center position by defining a "range of tolerance" that is
considered to be the center. This range is called "notch" and is given in a
percentage of the available range. I suggest using 10% so you don't lose too
much pot resolution, but it's still easy enough to hit the center reliably. Here
is an example:

```droid
[pot]
    pot                   = P1.1
    notch                 = 10%
    output                = _ACTIVITY

[algoquencer]
    activity = _ACTIVITY
    ...
```

## Slope

Sometimes you want a bit more resolution at the smaller values of the pot range.
Maybe the pot controls a time from 0.0 to 1.0 seconds. And in the low range, say
about 0.1 seconds, you need finer control.

You can change the slope of the pot in a way that either small values or values
near 1.0 are "streched out". The default is `slope = 1.0`. Look at the following
diagram for the impact of different slope values:

| slope | Effect on output curve |
|-------|------------------------|
| 0.5   | Values near 1.0 zoomed in (concave) |
| 1.0   | Linear (default) |
| 2.0   | Small values near 0.0 zoomed in (convex) |
| 3.0   | Small values near 0.0 zoomed in even more |

The curves run from 0 at 0% pot movement to 1 at 100% pot movement, bending more
strongly as the slope moves away from 1.0.

As a slope value of 0.0 does not make sense, because the pot would stick to 0.0
all the time, a minimum value of 0.001 is enforced.

If you are curious about the algorithm: This operation is just x^slope. So it's
not "logarithmic" or "exponential" but polynomial.

## Splitting the pot into two hemispheres

The jacks `lefthalf`, `righthalf`, `lefthalfinv` and `righthalfinv` allow you to
split the pot in the middle into two ranges and use them for something completely
different. Let's make an example:

```droid
[pot]
    pot = P1.1
    lefthalf = O1
    righthalf = O2
```

Now let's start with the pot in the center position. Both outputs will be at
0.0. If you now turn the pot to the left, just `lefthalf` (at O1) is going to
rise until it reaches 1.0 at the left end of the pot range. `righthalf` is
staying at 0 all the time.

At the right half of the pot range, likewise `lefthalf` stays zero and
`righthalf` will raise from 0 to 1.

The jacks `lefthalfinv` and `righthalfinv` are similar, but are 1.0 in the
neutral position in the center and fall to 0.0 at the edges.

## Virtual pots

This circuit can handle so called "virtual pots". This is a situation where the
physical position of the potentiometer does not match its output value. There are
three situations where the `pot` circuit automatically switches to this virtual
mode:

- When you share (overlay) pots using the `select` input
- When you enable presets (using `preset` or `loadpreset`)
- When you send a trigger to `clear`

Of course you can even use combinations of this: Overlay a pot with multiple
functions, work with presets and set a start value at the same time.

If none of these three features are used, there is no virtual pot and the
physical position always counts.

In virtual mode, the last virtual value of the pot is always saved to the SD card
and restored the next time you start your Droid.

## The LED gauge

In virtual mode the "LED gauge" is automatically activated. This displays the
current virtual value of a pot using the 16 LEDs of your MASTER. If you use `pot`
with a continous value, it is displayed with a "dot" that is moving from the LED
of O5 (0.0) over the LEDs of I2 and I3 (0.5) to the LED of O8 (1.0). When you use
`discrete`, values from 0 to 15 are displayed using the LEDs in the order from
left to right and from top to bottom.

If you are using a MASTER18, your master does not have an LED matrix. But if you
have at least one B32 in your patch, the gauge is displayed using the upper 4 × 4
half of your first B32. Without a B32 you don't have an LED gauge on a MASTER18.

If you have a DB8E controller (see the manual ([hardware](../hardware.md))), it
takes over the display and there never is an LED gauge being displayed.

## Sharing / overlaying pots

Potentiometers are valuable ressources and sooner or later you will run into a
situation where you wish you had more pots. So you come up with the idea of using
one pot for more than one function and switch between those with a button.

Previously DROID offered the circuit [`switchedpot`](switchedpot.md) for that
task but that had certain limitations and also was not consistent with other
circuits.

Let's make an example: Our task is to share pot `P1.1` so it sets *individual*
release values for four different envelopes.

First we need something to switch between these four. We do this with a
[`buttongroup`](buttongroup.md):

```droid
[p2b8]

[buttongroup]
    button1 = B1.1
    button2 = B1.2
    button3 = B1.3
    button4 = B1.4
    led1 = L1.1
    led2 = L1.2
    led3 = L1.3
    led4 = L1.4
```

Now at any given time, exactly one of the four buttons (i.e. their LEDs) is
active. Now we add four `pot` circuits using the *same pot*. The trick is the
`select` input. Each of these four should be selected just if one specific button
is active. The output of each is being sent to one of the envelopes:

```droid
[pot]
    pot = P1.1
    select = L1.1
    output = _RELEASE1

[pot]
    pot = P1.1
    select = L1.2
    output = _RELEASE2

[pot]
    pot = P1.1
    select = L1.3
    output = _RELEASE3

[pot]
    pot = P1.1
    select = L1.4
    output = _RELEASE4
```

Finally we can add the four envelopes:

```droid
[contour]
    trigger = I1
    release = _RELEASE1
    output = O1

[contour]
    trigger = I2
    release = _RELEASE2
    output = O2

[contour]
    trigger = I3
    release = _RELEASE3
    output = O3

[contour]
    trigger = I4
    release = _RELEASE4
    output = O4
```

Now you can switch between the four envelopes with the buttons and use the pot to
adjust the release time of the selected envelope.

Hints:

- Don't mix up `B1.1` and `L1.1`. If you would use `B1.1` for the switching, you
  would need to *hold* the button down while turning the knob. In which case you
  wouldn't need the `buttongroup` circuit.
- It is supported (and maybe useful) to select *several* of the "virtual" pots at
  the same time. In such a situation the turning of the real knob will adjust all
  of the selected values at the same time.
- Pots are no motorized faders. So they cannot show the current value correctly
  after switching. See below for details.
- In certain cases the `selectat` input might come handy: if you do the switching
  with *one* number that changes, not a bunch of gate signals. See the jack table
  below for details.

## Working with presets

The `pot` circuit supports up to 16 presets. With the use of the `preset` input
you can select one of these. Set a number from 0 to 15 there to switch between
presets. A change of that number immediately switches to another preset.

As an alternative you can work in a triggered mode by patching `loadpreset` and
`savepreset` in addition. Switching presets happens just on these triggers. In
triggered mode it's like having one more preset: the current "working" position
of the pot.

On the presets chapter (see the manual ([basics](../basics.md))) there is a whole
chapter about presets. You find examples and more hints there.

## Using a start value

A trigger to `clear` will set the virtual position of the pot to a defined start
value (which you can adapt with `startvalue`). This means that now the physical
position of the pot is not anymore identical with the virtual position. For that
reason the pot runs in virtual mode as soon as you connect the `clear` input.

In virtual mode the state of the virtual pot is saved to the SD card, the pickup
procedure (as described below) is applied and the LED gauge is active per default
(MASTER only).

## Picking up the pots

When you use overlaying, presets or a start value, your pots run in virtual mode.
It means that the physical value of the pot might not be identical with its output
value.

As an example let's assume that – using the upper example with overlaying – you
first press `B1.1` and set decay fully CW `1.0`. Now you select `B1.2`. Because
0.5 is the start position of every virtual pot that is the current value of the
second virtual pot. But the physical pot is at 1.0.

This is solved in the following way:

- If you turn the physical pot *right*, the value of the virtual pot is always
  increased until both reach `1.0` at the same time.
- If the physical pot is already at `1.0` when you select a virtual pot, it cannot
  be increased further. You first have to turn the pot left a bit and then right
  again.
- If you turn the physical pot *left*, then the value of the virtual pot is always
  decreased until both reach `0.0` at the same time.
- If the physical pot is already at `0.0` when you select a virtual pot, it cannot
  be decreased further. You first have to turn the pot right and then left again.

If you really want even more details – here we go: Let's assume that the virtual
pot is at `0.4` when you select it. And let's further assume that the physical pot
is at position `0.8`. When you turn it *left*, the physical pot has a way of 0.8
to go until 0.0 and the virtual just 0.4. So the virtual pot is moving with half
of the speed, for both to reach 0.0 at the same time. When you turn the pot
*right*, the virtual pot has 0.6 to go until maximum, while the physical pot has
just 0.2 left until it reaches its maximum. So now the virtual pot moves three
times faster than the physical.

This algorithm is different than the common "picking up" of pots that you see in
Eurorack land quite a lot in such situations. I preferred my solution because it
seems to be more convenient – especially if you want to change a value *a little
bit*. Also it allows to have multiple virtual pots to be selected at the same time
without having their values immediately snap to the same value.

By the way: it is also possible to select *none* of the pots. Which is a
convenient way to reset the physical pot to the middle position so that you always
have headroom for movement left *and* right, before selecting one of the virtual
pots.

## Pot circuits doing math

Here is an important caveat for all you hardcore hackers out there: The `pot`
circuit is designed to interface with real pots that real users turn. While you
*can* misuse a `pot` circuit for doing some basic math, don't do this. Rather have
a look at [`math`](math.md) and [`compare`](compare.md).

Why? In order to optimize the execution speed of your patch, several user
interface circuits are executed at just 12.5% of the normal speed. This saves
valuable time for the execution of more time critical circuits. So instead of
checking pots at sub-millisecond intervals, your master rather spends its time in
executing your sequencers with a timing as precise as possible.

This means, that `pot`, `button`, `buttongroup` and similar circuits are executed
just every 8th loop cycle.

If you experience any trouble with this "UI slowdown", you can disable it by using
a [`droid`](droid.md) circuit:

```droid
[droid]
    uislowdown = 0
```

## Display

If you have a DB8E display controller (see the manual ([hardware](../hardware.md))),
the `pot` circuit automatically updates the display whenever the input pot value
has changed. It outputs the updated value of `output`.

If you use other outputs like `lefthalfinv` etc., these outputs are not being
displayed. If these values are important for you, use the [`display`](display.md)
for that purpose.

If you use the `pot` circuit for non-interactive purposes like computing a
modified slope from some automated input, you might want to disable the display
with `display = 0`. Otherwise ever changing values will thrash your display.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `pot` (`p`) | `0..1` | `0.0` | Wire your pot here, e.g. `P1.1` |
| `outputscale` (`os`) | CV | `1.0` | The final output is multiplied with this value. It's a convenient method for scaling up and down the pot range. |
| `outputoffset` (`oo`) | CV | `0.0` | After scaling the virtual pot value with `outputscale`, this value is being added before sending the result to the output. This parameter is only used for the "normal" `output`, not for the other special outputs like `lefthalf` etc. |
| `notch` (`no`) | CV | `0.0` | By setting this parameter to a positive number you create an artificial "notch" of that size. We suggest using `0.1` (or `10%`). The maximum allowed value is `0.5`. Greater values will be reduced to that. Note: Using this in combination with `outputscale` also moves the notching point. E.g. with `outputscale = 2` the notch will be at `1.0`. |
| `discrete` (`d`) | integer | | Setting this value to 1 or larger switches the pot over to select a discrete integer number, rather than a continous value. For example `discrete = 5` makes the pot output one of the *exact* values `0`, `1`, `2`, `3` or `4`. This is ideal for selecting presets and similar. If you enable `ledgauge` (highly recommended), it shows you the value by using the LEDs of the master in an adapted way. The maximum allowed number is `16`. When using discrete, the `startvalue` input is interpreted as a discrete number. So for example if you have `discrete = 5`, you can use `startvalue = 3` to set the selected value to the number `3` after a trigger to `clear`. A potential `outputscale` is applied *afterwards*. Notes: The options `notch` and `slope` do not work in discrete mode. `outputscale` and `outputoffset` are still applied, though. All outputs other than `output` are dead and output 0.0. `discrete = 1` does not really make sense, since there is just one value to select from and the output will always be `0.0`. |
| `slope` (`sl`) | CV | `1.0` | Changes the resolution of the pot in lower or higher ranges. Set `slope` to 2 or more, if you want small values near 0.0 to be "zoomed in". Set slope to `0.5` or `0.3` if you want to zoom in value nears 1.0. |
| `ledgauge` (`g`) | CV | ☞ smart | The "LED gauge" uses the 16 LEDs of the MASTER in order to indicate the current value of the pot (not available on the MASTER18). This is especially useful for "virtual" pots – i.e. those pots that you get when you use `select` in order to layer several different functions onto one pot. In that situation the position of the physical pot can be different than that of the virtual one, so the gauge shows you the effective virtual value. Furthermore, by illuminating the inner four LEDs, the gauge shows when the pot hits *exactly* 0.5. This can only happen if you use the `notch` parameter. Otherwise it's practically impossible to hit exactly. If you have a MASTER18, the gauge is displayed in the upper 16 LEDs of your first B32, if you have one. The LED gauge is automatically activated if you use `select`. If you don't like the LED gauge, you can turn it off with `ledgauge = off`. Otherwise `ledgauge` set's the color of the indicator in the same way as the R-registers do and at the same time *enables* the gauge even if you don't use `select`. Color examples: `0.2` cyan, `0.4` green, `0.6` yellow, `0.73` orange, `0.8` red, `1.0` magenta, `1.1` violet, `1.2` blue. The colors repeat over in a kind of wheel at 1.2, so e.g. `1.4` creates the same color as `0.2`. Note: If you have a DB8E controller, this parameter does nothing. No LED gauge is shown. |
| `startvalue` (`sv`) | `0..1` | `0.5` | This parameter defines the value your pot will get when there is a trigger to `clear`. This is the value *before* `outputscale` and `outputoffset` is applied. If you use `discrete`, the parameter does not expect a fraction but a discrete number in the range of the discrete values (`0`, `1`, `2`, etc.). |
| `display` (`dy`) | integer | `1` | This parameter selects the number of the DB8E display controller, where any change of this circuit's state will show up. Per default, the first DB8E available is used. Set this to `2` if you have a second DB8E and want to use that. Note: any other controller is not being counted here, it's really just the number of the DB8E as counted from the master module. Set this to `0` to suppress any display output from this circuit. |
| `header` (`hr`) | text | ☞ smart | This text is displayed at the top of the DB8E display whenever this circuit shows up in the display. If you omit this, an automatic title is used. For example if the output of this circuit is fed into an internal patch cable, the name of this cable is displayed. Setting `header = ""` or `header = 0` removes the header and just displays the actual value. |
| `select` (`s`) | integer | ☞ smart | The `select` input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to `1`). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | ☞ smart | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is `0`, the circuit will be active if `select` is exactly `0` instead of a positive gate signal. In some cases this is more convenient. |
| `preset` (`pr`) | integer | ☞ smart | This is the preset number to save or to load. Note: the first preset has the number `0`, not `1`! For the whole story on presets please refer to the presets chapter (see the manual ([basics](../basics.md))). This circuit has 16 presets, so this number ranges from `0` to `15`. |
| `loadpreset` (`lp`) | trigger | | A trigger here loads a preset. As a speciality you can use the trigger for selecting a preset at the same time. |
| `savepreset` (`sp`) | trigger | | A trigger here saves a preset. |
| `clear` (`cl`) | trigger | | A trigger here loads the default start state into the circuit. The presets are not affected, unless you use direct preset switching with the `preset` input and without triggers. And in that case the current preset is also cleared. |
| `clearall` (`ca`) | trigger | | A trigger here loads the default start state into the circuit and into all of its presets. |
| `dontsave` (`dos`) | gate | `0` | If you set this to `1`, the state of the circuit will not be saved to the SD card and not loaded from the SD card when the Droid starts. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | `0..1` | Your pot output comes here. |
| `bipolar` (`b`) | CV | Optional output with a range from -1.0 to 1.0, where the center notch is at 0.0 (or from -`outputscale` to +`outputscale` if that is used). |
| `absbipolar` (`ab`) | CV | A variation of `bipolar` that always outputs a positive value, i.e. the pot will go 1 … 0.5 … 0 … 0.5 … 1 (if `outputscale` is not used). |
| `lefthalf` (`l`) | CV | This output allows you to split the pot into two hemispheres. Here you get `outputscale` … 0.0 while the pot is in the left half. In the middle and right of it you always get 0. |
| `righthalf` (`r`) | CV | This is the same but for the right half. It outputs 0 while the pot is in the left half and 0.0 … `outputscale` from the middle to the fully right position. |
| `lefthalfinv` (`li`) | CV | This outputs 1.0 - `lefthalf`, i.e. the value range 0.0 … 1.0 … 1.0 when the pot moves left → mid → right (and then scaled by `outputscale`). |
| `righthalfinv` (`ri`) | CV | This outputs 1.0 - `righthalf`, i.e. the value range 1.0 … 1.0 … 0.0 when the pot moves left → mid → right (and then scaled by `outputscale`). |
| `onchange` (`c`) | trigger | This output emits a trigger whenever the pot is turned in either direction. |

## See also

- [`notchedpot`](notchedpot.md) — obsolete circuit replaced by `pot`.
- [`switchedpot`](switchedpot.md) — obsolete circuit replaced by `pot`.
- [`buttongroup`](buttongroup.md) — for switching between overlaid virtual pots.
- [`math`](math.md), [`compare`](compare.md) — use these instead of `pot` for
  actual math.
- [`droid`](droid.md) — disable the UI slowdown with `uislowdown = 0`.
- [`display`](display.md) — display values that `pot` does not display itself.
