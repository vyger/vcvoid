---
circuit: algoquencer
title: Algorithmic sequencer
obsolete: false
ram_bytes: 888
manual_pages: [127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139]
category: sequencer
tags: [sequencer, drum, trigger, turing-machine, random, algorithmic, accents, fills, rolls, ratchets, morphs, dejavu, variation, activity, presets, alternate, branches, fractal, pattern]
see_also: [minifonion, button, buttongroup, mixer, motoquencer, encoquencer, sequencer]
impl_difficulty: hard
controller_binding: controller-enhanced
verification: requires-human
spec_gap: true
difficulty_note: "Large stateful performance sequencer (888 bytes) combining a Turing-machine PRNG with fills, morphs, branches, rolls, variation and activity algorithms."
verification_note: "Requires-human: runs master-only from gate I/O but step buttons/LEDs, accents and presets need a controller. The internal PRNG and the morph/fill/variation placement algorithms are unspecified, so faithful output can only be judged against real hardware."
---

# algoquencer — Algorithmic sequencer

The Algoquencer is a versatile performance sequencer, which implements a
completely new approach: it combines a classical trigger sequencer with a
turing machine and other algorithms in order to create a very hands on pattern
generator for live improvisation. Its main tasks are:

- manual + algorithmic trigger sequencer for drum voices
- generator of (possibly) repeating random CVs

The random CVs can either be completely chaotic or repeating over and over and
also anything inbetween. In this way it is similar to the so called "Turing
Machine" while using a different algorithm internally.

There are lots of interesting high-level parameters that you can easily map to
pots on your controllers or encoders on your E4. Among these are Activity,
Variation, Déjà-vu and many more. With a turn of a knob you can instantly
increase or decrease the density or complexity or your patterns in various ways.

Here are some of the features:

- Up to 16 step buttons
- change the pattern length on the fly
- manually editable accents for each step
- ratchets and drum rolls
- fills
- deterministic and chaotic randomization
- simple muting
- fractal sequencing

If you use the Algoquencer for drumming, each `algoquencer` circuit plays just
one voice – e.g. a snare drum. For orchestrating a whole drum kit simply use
more Algoquencers with possibly different parameters. It totally makes sense to
use some of the pots and buttons with all drum instruments – e.g. a pot for
Déjà-vu – and others on a per-instrument base, like Activity.

Here are some examples of how to use the Algoquencer circuit.

## Pseudo random voltages / Turing machine

Without any inputs other than `clock` the algorithmic sequencer creates a
sequence of random numbers that repeat over and over every 16 steps. This is
much like the "Turing Machine". The voltage range of the `pitch` output defaults
to 0 V … 3 V:

```droid
[algoquencer]
    clock = G1 # Some clock signal
    pitch = O1
```

You can change the length to any other value up to 64 by using the `length`
parameter:

```droid
[algoquencer]
    clock = G1 # Some clock signal
    pitch = O1
    length = 12
```

If you do not like the default output voltage range you can adjust that with the
inputs `pitchlow` and `pitchhigh`:

```droid
[algoquencer]
    clock     = G1 # Some clock signal
    pitchlow = 1V
    pitchhigh = 4V
    pitch     = O1
```

Note: the CV output is named `pitch` which suggests using it as a 1V/octave CV.
This is just one application. You can use the CV for anything you like. If you
use it as pitch, you might want to quantize it to a musical scale. Otherwise the
pitches sound very odd. You can do the quantization with the
[`minifonion`](minifonion.md):

```droid
[algoquencer]
    clock     = G1 # Some clock signal
    pitchlow = 1V
    pitchhigh = 4V
    pitch     = _PITCH

[minifonion]
    input    = _PITCH
    output   = O1
```

The `dejavu` parameter sets the type of randomness: repeating or always
different. It has a default of `1.0`. This means that once a random decision has
been made for a certain step of the pattern, it won't change ever. The same
random pattern will repeat again and again. Making `dejavu` smaller will convert
some of the decisions to be random while others still repeat unchanged over and
over again.

Try mapping `dejavu` to a pot and experiment to get a feel of its impact.

```droid
[algoquencer]
    clock        = G1 # Some clock signal
    pitchlow     = 1V
    pitchhigh    = 4V
    pitch        = _PITCH
    dejavu       = P1.1 # Some potentiometer

[minifonion]
    input    = _PITCH
    output   = O1
```

You want to change the entire pattern? You can choose another one by setting
`pattern` to an arbitrary integer number:

```droid
[algoquencer]
    clock   = G1
    pitch   = O1
    length = 12
    pattern = 5
```

When you leave `dejavu` at `1.0`, the same pattern number will always sound
exactly the same (the pattern number is used as the seed for a pseudo random
generator).

Another way to change the pattern is to send a trigger to `nextpattern`, for
example with a button:

```droid
[algoquencer]
    clock       = G1
    pitch       = O1
    length      = 12
    dejavu      = 1
    nextpattern = B1.1
```

Do you like slowly evolving patterns (which is a feature from the "Turing
Machine")? The `morphs` parameter – which is usually `0.0` – will introduce
random changes to the repeating pattern in a very controlled way:

- Changes (aka morphs) are introduced each time the pattern starts (again) –
  never in-between
- The exact number of changes is controlled with the `morphs` parameter and is
  not random.
- The steps where these changes happen and the changes itself are random.

`morphs` takes a number between `0.0` and `1.0`. At `0.0` no morphs happen. At
`1.0` every step will be morphed – thus completely changing the pattern every
time it would repeat. Here is a table of how exactly the parameter affects the
number of morphs per 64 steps. It is done in a way that is very suitable for
mapping it directly to a pot and gives a very fine resolution at the left half
of the pot:

| morphs | morphs per 100 steps |
|--------|----------------------|
| 0.0 | no morphs |
| 0.1 | 1 |
| 0.2 | 4 |
| 0.3 | 9 |
| 0.4 | 16 |
| 0.5 | 25 |
| 0.6 | 36 |
| 0.7 | 49 |
| 0.8 | 64 |
| 0.9 | 81 |
| 1.0 | 100 |

As you can see the smallest number of morphs – if you set `morphs` just a little
above 0 – is one per 64 steps.

Note: If you are curious whether morphs are happening you can wire the output
`morphled` to some LED. It will flash whenever morphs happen.

## Dejavu or morphs?

Did you get the difference between `dejavu` and `morphs`? Here once again:

- `dejavu` controls, whether to use just complete random values (`dejavu = 0`)
  or repeating pseudo-random sequences (`dejavu = 1`).
- `morphs` comes into play, when `dejavu` is > 0 and modifies the pseudo-random
  sequences from time to time a bit so they won't get boring.

## True random voltages

If you do not want the random pitches to repeat you can set the `dejavu`
parameter to 0. This transforms the `algoquencer` into a simple random number
generator:

```droid
[algoquencer]
    clock = G1
    pitch = O1
    dejavu = 0
```

It can be very interesting to map `dejavu` to one of the pots of your
controllers. That way you can change on-the-fly between structured melodies and
complete randomness – or anything between!

## Using the Algoquencer as drum sequencer

This is how you setup the Algoquencer for use as a drum sequencer. Like in the
previous examples you need a clock signal. Also using a reset input helps you to
sync your drums with some external stuff. A trigger here resets the pattern to
the first step:

```droid
[algoquencer]
    clock = G1
    reset = G2
```

A trigger into `clock` will move to the next step of the pattern. One into
`reset` resets back to the first step.

Algoquencer supports up to 16 buttons (aka step buttons) for manually setting up
a trigger pattern. If you assign less than 16 buttons then your patterns will be
shorter. You probably want to assign these to buttons of your controllers, e.g.

```droid
    button1 = B1.1
    button2 = B1.2
    button3 = B1.3
    button4 = B1.4
```

In order for the LEDs in these buttons to work you also need to assign the
`led...` outputs:

```droid
    led1 = L1.1
    led2 = L1.2
    led3 = L1.3
    led4 = L1.4
```

Please make sure that there is no "hole" in your definitions. You cannot use
`button8` if you not also use `button1` through `button7`.

Note: You can use Algoquencer even without step buttons. This is like having an
empty pattern, but `activity` will still work and create artifical beats if it
is not zero.

Last but not least wire the output `trigger` to the trigger input of some drum
voice.

```droid
    trigger = O1
```

For a simple "normal" trigger sequencer this is enough. I'd suggest you setup
this small example first and once it is up and running you investigate further
features of Algoquencer. Here is the example once again complete for usage while
we assume that you have an P2B8 controller:

```droid
[p2b8]

[algoquencer]
    clock   = I1
    reset   = I2
    button1 = B1.1
    button2 = B1.2
    button3 = B1.3
    button4 = B1.4
    led1    = L1.1
    led2    = L1.2
    led3    = L1.3
    led4    = L1.4
    trigger = O1
```

## Accents

Algoquencer supports setting or not setting an accent for each of the steps. For
this there is a "second page" of the buttons where you can edit these accents.
In order to access that accent page you need to wire the input `accentbutton` to
one of your buttons (e.g. `B1.5`). Also wire the output `accent` to some external
output jack and patch that to the accent input of your drum voice:

```droid
    accentbutton = B1.5
    accent       = O3
```

Now while you hold the accent button the step buttons will switch over to showing
the accents intead of the normal beats. And you can set and remove accents now.

Note: if you do not want to be forced to hold the button while editing accents
you can convert it into toggle button using the [`button`](button.md) circuit:

```droid
[button]
    button             = B1.5
    led                = L1.5
    output             = _ACCENTS

[algoquencer]
    # ... the other stuff
    accentbutton = _ACCENTS
    accent       = O3
```

## Alternate steps

The Algoquencer just supports 16 steps, but there is a great way to extend your
pattern to 32 or more steps. The concept for this is a bit unusual, but all the
more musical and hands on. It goes like this:

There is an *alternate page* of another 16 buttons. These are like a third layer
of buttons (if you account the accents for the second layer). Just like with the
accents you define a button for bringing up that layer, for example:

```droid
    alternatebutton = B1.7
```

While you hold that button you edit the alternate page instead of the normal
steps.

Now: every active step in the alternate page will *flip* the according step in
the normal page *for every second bar*. That way you can have a variation of the
pattern every second bar but you just edit the *differences* to the normal
pattern. So adding or removing one beat every second bar can be done by
activating exactly one step in the alternate page.

You are not limited to a pattern of two bars. By setting `alternatebars` to
another value you can change the frequency of the alternate bar:

```droid
    alternatebutton = B1.7
    alternatebars = 4
```

Now bars 1 - 3 are played normally and every forth bar the alternate page is
applied. That basically forms a pattern of 64 steps.

## Pattern length and bars

As you have at most 16 buttons one pattern can have a length of at most 16 steps.
The length of the pattern can be set in various ways:

- If you wire at least one `button1` then the length defaults to the number of
  wired buttons.
- This can be overridden by setting `length` to any value (e.g. `length = 7`).
- If you use the `lengthbutton` then you can interactively change the pattern
  length during your performance. This will always override the `length` input.

Add the button for changing the length is easy:

```droid
    lengthbutton = B1.6
```

One bar usually has the same number of steps as your pattern. But if you set
`repeats = 2`, one bar will consist of two times the pattern (and thus lasts
twice as long). Bars are useful when you use fills or branches.

## Playing fills

Fills are additional beats the Algoquencer adds at the end of certain bars in
order to play a musically interesting fill. In order to use this first wire
`fills` to some CV or most likely to a pot:

```droid
    fills = P1.1
```

Now if you crank up that pot clockwise then more and more beats will be added –
with a tendency to the end of the bar. In music – however – playing a fill each
bar is not very interesting. By setting `fillorder` to 1, 2 or 3 (or even a
higher number) will make the fills assume a cycle of 2, 4 or 8 or move bars.
Please see below for details.

## Activity and random

Four inputs are key features of Algoquencer, since they extend it from a plain
old trigger sequencer to an algorithmic drummer. These are `variation`,
`activity`, `dejavu` and `morphs`. The latter two already have been discussed
when using Algoquencer as random generator. They have the same effect here.

The default value of `variation` is `0.0`. That means that Algoquencer will
exactly play the pattern as you have dialled it in with your step buttons. If you
increase this value (a pot is handy for doing this, of course), randomly some of
the beats will move to other steps. Setting `variation` to `1.0` will completely
alter your pattern. The number of beats will stay the same!

`activity` will change exactly that: the number of triggered beats in one bar.
The default value is `0.5` – which is the center position if assigned to a pot.
Here the number of played beats is exactly the same as you have set in your
pattern. Turn it left to remove (randomly) some of the beats. Turn it right to
add some. At `0.0` no beats are triggered, at `1.0` there is a beat for every
clock cycle.

The `activity` also has an effect when you create random voltages. Here the
voltage only changes when a "beat" happens at that step, even if you are not
using the `trigger` output.

## Further nifty parameters

There are some more interesting parameters like `rolls`, `offbeats`,
`distribution` and `branches`. Please look at the table of inputs for more
details.

## Presets

The algoquencer supports up to 16 presets. Each preset comprises all settings
that can be interactively changed, i.e. the activated steps, accents, alternate
steps, the manually changed length, the state of the mute button and also the
current random seed (which was modified by `nextpattern`, `prevpattern` or
`reroll`).

There are three ways of switching between presets. The first way is easy to
implement. Simply send the number of the current preset to the input `preset`.
It has to be a number from 0 to 15. You can for example use a pot if you multiply
it with 15:

```droid
[algoquencer]
    preset = P1.1 * 15
    ...
```

Now any change you make will immediately be saved to that current preset. If you
change the preset number by turning the pot, another preset will immediately be
loaded and activated.

The second – more sophisticated – way is to use triggers for loading and saving.
These could be buttons, e.g.:

```droid
[algoquencer]
    preset = P1.1 * 15
    loadpreset = B1.1
    savepreset = B1.2
    ...
```

Now turning the knob does not load or save any preset. The input `preset` is just
evaluated when you press `B1.1` or `B1.2`:

- A trigger to `savepreset` will save the current settings into the preset that
  is selected with the `preset` input.
- A trigger to `loadpreset` will copy the contents of the preset selected by
  `preset` into the current settings.

Note: In the second mode you effectively have 17 presets, since the "current
settings" could also be considered to be a preset. The advantage of this mode is
that playing around with the settings of the algoquencer does not immediately
effect any of the presets.

Hint: In order to avoid saving or loading presets by mistake, have a look at the
[`button`](button.md) circuit and the `longpress` output. It sends a trigger when
a button is pressed and hold for a certain time.

The third way is a combination of the first two ways. Here you work with
triggers, as well. But these triggers at the same time hold the number of the
preset to load or to save. This makes situations easier where you have one button
per preset:

```droid
[mixer]
    input1 = B1.1 * 1
    input2 = B1.2 * 2
    input2 = B1.3 * 3
    output = _LOAD_PRESET

[mixer]
    input1 = B1.4 * 1
    input2 = B1.5 * 2
    input2 = B1.6 * 3
    output = _SAVE_PRESET

[algoquencer]
    loadpreset = _LOAD_PRESET
    savepreset = _SAVE_PRESET
```

This means that if the trigger CV has the value 2 when it is non-zero, it load
preset number 2. This mode is automatically active, if you don't patch the
`preset` input.

There is one drawback of this method: you cannot easily access preset number 0
that way, since the CV 0 is not sufficient for triggering the input. The trick is
sending a value larger than 0.1 (which is the threshold for boolean "true"
values) and less than 0.5 (which would be rounded to 1). So for example send a
trigger with the value 0.3 to load or save preset number 0.

## Sharing buttons between multiple algoquencers

The buttons on your controllers are a valuable ressources and not to be wasted
lightheartedly. And especially the algoquencer uses quite a lot of buttons. But
the good news is: you can share most of these buttons with other instances of
algoquencer, to create a multi-track sequencer with just one set of buttons. You
can even share the buttons with completely other circuits.

The key to this is the `select` input. If you patch it, all buttons and LEDs will
just be used by this instance of algoquencer as long as `select` gets a high gate
signal. Here is an example (which is just a sketch and not complete):

```droid
[algoquencer]
    select = _SELECT_1
    button1 = B1.1
    button2 = B1.2
    ...
    led1    = L1.1
    led2    = L1.2
    ...

[algoquencer]
    select = _SELECT_2
    button1 = B1.1
    button2 = B1.2
    ...
    led1    = L1.1
    led2    = L1.2
    ...
```

Now you need to make sure that at any given time either `_SELECT_1` or
`_SELECT_2` is active. The easiest way is with a [`buttongroup`](buttongroup.md),
because here you can add more and more tracks if you like. Let's assume that for
switching between tracks you use the buttons `B2.7` (track 1) and `B2.8`
(track 2). This would look like this:

```droid
[buttongroup]
    button1 = B2.7 # select track 1
    button2 = B2.8 # select track 2
    led1 = L2.7
    led2 = L2.8

[algoquencer]
    select = L2.7 # becomes 1 if B2.7 is selected
    button1 = B1.1
    button2 = B1.2
    ...
    led1      = L1.1
    led2      = L1.2
    ...

[algoquencer]
    select = L2.8 # becomes 1 if B2.8 is selected
    button1 = B1.1
    button2 = B1.2
    ...
    led1      = L1.1
    led2      = L1.2
    ...
```

Please note: the buttons `mutebutton` and `unmutebutton` and their according LEDs
are not handled by the `select` jack. The idea is that they always get their own
dedicated buttons. This allows you to quickly mute or unmute several tracks at
once.

## How the LEDs work at a reset

The LEDs in the buttons do not only show the enabled steps, but also – with 50 %
brightness – the current position of the step counter. To be precise, the LED
always shows the step that has been played most recently. If the counter
highlights the first step, that step has already been played and the next clock
tick will trigger step number two.

This is quite natural and seems easy to understand. Unless you think of what
happens after a *reset*. If you send a trigger to the `reset` input, the sequence
is reset to its first step. But of course you expect the *next* step to be played
to be the *first* step.

This means that the first step cannot be the one indicated by the step counter
LED – because that always shows a step that has been played already. For that
reason, after a reset, the step LED is turned off until the next clock cycle.

## Display

If you have a DB8E controller attached and configured (see the manual
([hardware](../hardware.md))), the `algoquencer` will show a manual change of the
pattern length with the `lengthbutton` in the display. Currently no further
information is displayed. This might change in future versions.

Using the `header` input allows you to show a name in the display header.
Example:

```droid
[algoquencer]
    header = "Kickdrum"
    ...
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `clock` (`c`) | trigger | | Clock input. This is mandatory. For each clock pulse the sequencer is advanced by one step. |
| `reset` (`r`) | trigger | | Reset input. A trigger here switches back to step 1. |
| `button1 … button16` (`b`) | gate | | 1st … 16th step button. Assign these buttons to buttons on your controllers. |
| `length` (`l`) | integer | ☞ smart | Sets the length of the pattern. Note: if you use `lengthbutton`, this input is ignored as soon as the length button has been used for the first time. If you have assigned at least one button, the default value of `length` is the number of buttons you have assigned. Otherwise it defaults to `16`. The maximum length is 64. Any larger number will be truncated to 64. |
| `pattern` (`pt`) | integer | `0` | Selects a pattern of pseudo random values. If you set `dejavu` to 1, all "random" decision are deterministic and repeat again and again. If you do not like these choices, you can choose a different pattern, just by setting this input to any integer number you like. The default pattern is 0. If you patch a pot here, simply multiply it by the number of different patterns you want to select, e.g. `pattern = P1.1 * 10`. This will allow you to select one of the pattern 0, 1, … 10. You can use `pattern` in combination with `nextpattern`, `prevpattern` and `reroll`. These three inputs create an offset to the chosen pattern. E.g. if you set `pattern = 5` and send one trigger to `nextpattern`, the actually used pattern is 6. |
| `nextpattern` (`np`) | trigger | | Switches forward to the next pseudo random pattern. |
| `prevpattern` (`pp`) | trigger | | Switches back to the previous pseudo random pattern. |
| `reroll` (`rr`) | trigger | | Select one of the pseudo random patterns completely by random. |
| `clearpage` (`cp`) | trigger | | A trigger here unselects all step buttons in the currently active page (normal, alternate, accent). |
| `pitchlow` (`pl`) | CV | `0.0` | This set a lower voltage boundary for the `pitch` output for notes that are randomized. Negative values are allowed. |
| `pitchhigh` (`ph`) | CV | `0.3` | This set an upper voltage boundary for the `pitch` output for notes that are randomized. |
| `pitchresolution` (`pre`) | integer | `0` | If this is non-zero, it make the `pitch` output adopt that number of possible discrete values. E.g. if you set it to 2, only the values set by `pitchlow` and `pitchhigh` are possible. A value of 3 will allow an additional value in the middle, and so on. |
| `gatelength` (`gl`) | CV | `0.1` | The gate length in input clock cycles. A value of `0.5` (5 V) thus means half a clock cycle. A steady input clock is needed for this to work. Please note that if the gate length is >= 1.0, two succeeding notes will get a steady gate, which essentially means legato. When playing rolls, i.e. more than one beat per step, the gate length is divided by the number of rolls. That way the gates get shorter and even at a gatelength close to 1.0 the gates are still audible and do not merge together. |
| `lengthbutton` (`lb`) | gate | | Map this to a button like `B1.1`. While you press and hold this button the sequencer switches to *change length mode*. While in this mode a press of one of the step buttons will change the length of the pattern. Also while in this mode the LEDs of the step buttons will show the current length. If you do not like to hold the button but switch it on and off, you can create a toggle button with [`button`](button.md) and send its output here. |
| `repeats` (`rp`) | integer | `1` | Usually one bar has the length of one pattern. Setting this to 2 will consider one bar as a run of two times through the pattern. So if you have 8 buttons and `bars = 2`, one bar will be 16 steps, where the 1st and 9th step are set by `button1`, 2nd and 10th by `button2` and so on. Why should that be useful? Well – the difference shows up when you use fills, or branches or work with the alternate pattern. These three algorithms work based on bars. And `repeats = 2` makes one bar have 16 steps, even if you just have eight buttons. |
| `alternaterepeats` (`arp`) | integer | ☞ smart | If you are use using `repeats` and `alternatebars` / `alternatebutton` at the same time, with this input you can specify a different value for `repeats` when it comes to selecting the alternate button page. Assume you have eight buttons and `repeats = 2` and `alternatebars = 2`. Then Algoquencer will play two times your 8-step pattern normally and two times alternated (since two times the 8 steps form one bar). This results in a form of A A B B. If you want your form rather to be A B A B, set `alternaterepeats = 1`. This way, when it comes to alteration, the length of one bar is just normal length (8 steps here). |
| `branches` (`bs`) | integer | `0` | Enables the branching feature (sometimes also called fractal sequencing). When `branches = 1`, then every second bar will be using other random values – giving a sequence of the bars A B. With `branches = 2` you get a sequence of the form A B A C. A value of 3 creates an even longer sequence that repeats itself after eight bars: A B A C A B A D. Note: this only takes effect when you set `dejavu > 0`. The largest effect is when it is set to 1. And the you need to use either `variation` or set `activity` to a value greater than 0.5. Because otherwise Algoquencer will strictly play the gates that you've set with your buttons and then every bar will be the same, of course. |
| `mutebutton` (`mb`) | trigger | | Wire this to a button like `B1.2`. When you press the button once, all triggers are muted. Pressing again unmutes them. So this behaves like a toggle [`button`](button.md) in itself. You probably want to wire `muteled` to the LED in that button, e.g. `L1.2`. It show the mute state. The mute button works together with the unmute button (see below). Note: even if you use the `select` jack in order to overlay your buttons with several algoquencers, the `mutebutton` will always be active. The idea is to always have direct access to this button. |
| `unmutebutton` (`ub`) | trigger | | A trigger to this jack resets the mute button exactly at the beginning of the next bar. While waiting for that to happen, the output `unmuteled` will blink. Wire this to the LED in the button. Note: even if you use the `select` jack in order to overlay your buttons with several algoquencers, the `mutebutton` will always be active. The idea is to always have direct access to this button. |
| `accentbutton` (`ab`) | gate | | While this input is high you are in *accent editing mode*. This is very similar to the mode where you set the length. But now for each step you edit whether this step is outputting an accent when triggered. You might want to use a toggle button for this function, so you can operate without holding down the button all the time. |
| `alternatebutton` (`alb`) | gate | | If this input is high, you are in *alternate editing mode*. Every Algoquencer has an alternate set of steps. Each step that is currenty activated *toggles* the state of the normal step, but only for each even bar. This allows to introduce variations of the pattern that occur every second bar. |
| `alternatebars` (`aba`) | integer | `2` | With this input you can change the influence of the `alternatebutton`. Per default the pattern alternation is done every second bar. You can change this to any number you like with this input. Values less than 1 will be considered as one – which means that every bar is alternated. |
| `accentlow` (`al`) | CV | `0.0` | This value is output at `accent` when a note without an accent is being triggered or when no note is triggered at all. |
| `accenthigh` (`ah`) | CV | `1.0` | This value is output at `accent` while a note with an accent is triggered. The value will be kept for the full time of the clock cycle. |
| `startaccents` (`sc`) | integer | `2` | When the circuits starts its new life for the first time or if you do a `clear` or `clearall`, this input handles how the accents on the accent page should be preset. There are three possible settings: `0` all accents are cleared; `1` all accents are set; `2` accents are set on odd steps (downbeats) and cleared on even steps (off beats). |
| `activity` (`a`) | 0..1 (mid 0.5) | ☞ smart | This is the most important parameter and you will probably wire it to a pot like `P1.1`. The activity controls, how "busy" the sequencer is playing, or in other words how often a step gets an active gate (and thus a changing output pitch). Let's first assume that `variation` is set to `0.0` (which is the default). Then at a value of `0.5` (or pot at 12'clock) Algoquencer will exactly play that pattern that you have set with the step buttons. Turning the knob CCW will remove more and more beats from the pattern until it is completely silent at a value of `0.0` (or pot fully CCW). But if you turn up the knob above the middle position then more and more additional beats will be placed into you pattern in a random way until – at `1.0` – a trigger will happen at every beat. Note: If you do not use step buttons, this parameter behaves slightly different: A value of `0.5` then means an activity of 50%, which means that exactly the half of the steps will get an event. This is different from a situation where you *have* defined buttons but all are deselected. In that case `0.5` means that exactly the number of beats of your pattern are being played, which is zero in that case. |
| `variation` (`v`) | 0..1 | `0.0` | The variation controls how strictly Algoquencer will stick to the pattern that you have set with your step buttons. You probably want to wire this to a knob. A value of `0.0` (or the knob fully CCW) will allow no variations. Your pattern will be played exactly as it is. If the `activity` goes beyond `0.5`, additional beats will be placed, of course. And these are random. If you increase the variation, more and more beats of your pattern are being replaced with other beats – while keeping the total number of beats the same. If you set `variation` to `1.0` (or the pot fully CW) then your pattern is completely ignored except for the actual number of beats it contains. |
| `dejavu` (`d`) | 0..1 | `1.0` | The dejavu parameter controls what *random* should mean. If `dejavu = 0.0`, then all random decisions are completely chaotic – and every time a decision is taken the dice are being rolled again. At `dejavu = 1.0` on the other hand – once a random decision has been taken for a certain step in a certain bar, it will stay always the same from now on. This will lead to repeating exactly the pattern bars over and over again. We sometimes call this random to be "deterministic". Any position in between will choose some of the steps as chaotic random and some of the steps as deterministic. |
| `morphs` (`m`) | 0..1 | `0.0` | This parameter will introduce changes in formerly taken random decisions from time to time. If you set it above zero, at every start of a bar some of the deterministic random decisions will be remade. Setting `morphs = 1` will essentially disable `dejavu`, since all decisions are redone every bar anyway then. If you know the Turing Machine: In principle that has the same idea, but Algoquencer has a few improvements: The number of random changes is exactly controlled by the setting. At each specific setting of `morphs` the same number of changes will be done at each bar. Changes only appear at the beginning of each bar. If you use `branches`, they will appear whenever you sequence is over. Small settings will introduce just one morph each 64th step. |
| `offbeats` (`ob`) | 0..1 (mid 0.5) | `0.5` | Whenever random beats are being placed then this setting controlls whether downbeats or offbeats should be preferred. At at setting of `0.5` there will be no difference. If you increase the value then more and more offbeats will appear. Offbeats are steps with an even number, like 2, 4, 6 and so on. Value smaller than `0.5` will prefer downbeats. Offbeats sound more "complex" and downbeats more simple or "down to earth". |
| `distribution` (`di`) | 0..1 (mid 0.5) | `0.5` | This is very similar to `offbeats`, but this time you decide whether beats should be placed rather in the first half of the bar or in the second half. |
| `fills` (`f`) | 0..1 | `0.0` | When this parameter is set above `0.0`, additional beats will be placed in order to make the beat more "active". This happens at musically useful times controlled by `fillorder` (see below). The additional beats within the bar are placed in a way that prefers the end of the bar. If there are already too many beats in the bar then the fill will *remove* or change some instead. |
| `fillorder` (`fo`) | integer | `0` | This integer number controls how fills are being placed: `0` every bar; `1` every second bar; `2` small fill in bar 2, big fill in bar 4; `3` tiny fill in bar 2 and 6, medium fill in bar 4, big fill in bar 8. |
| `rolls` (`rl`) | 0..1 | `0.0` | This parameter controls if drum rolls (or ratchets as you might call it) are being created. At `0.0` no rolls are being created. At `1.0` every beat will be converted into a roll. Rolls always happen before the actual beat, they lead to it. If you using this feature for snare rolls you might want to use the output `rollvelocity` for controlling the snare volume. |
| `rollcount` (`rc`) | integer | `1` | Number of additional beats for playing the roll. Setting `rollcount = 0` would disable rolls. All these beats are distributed in the clock tick before the beat the roll is leading to. The first beat of the roll is exactly one tick before that beat – or more if you increase `rollsteps`. |
| `rollsteps` (`rs`) | integer | `1` | Length of the roll in clock ticks (steps). The total number of additional beats is thus `rollcount × rollsteps`. |
| `rollstartvelo` (`rsv`) | CV | `0.5` | Rolls can be played with an increasing velocity. This first beat starts with the velocity set with this parameter. Then every beat gets a bit louder until the last beat is played with velocity `1.0`. The velocity for rolls is output at the jack `rollvelocity`. |
| `pitch1 … pitch16` (`p`) | CV | ☞ smart | You can use these inputs, if you want the pitches of the `pitch` output play a certain melody. That way the Algoquencer behaves like a normal melody sequencer – but all the algorithmic parameters will be applied. For example `variation` will also be applied to these notes. Note: If `length` is larger than 16, these pitch inputs will be cycled through, so step 17 uses `pitch1`, step 18 uses `pitch2` and so on. |
| `display` (`dy`) | integer | `1` | This parameter selects the number of the DB8E display controller, where any change of this circuit's state will show up. Per default, the first DB8E available is used. Set this to `2` if you have a second DB8E and want to use that. Note: any other controller is not being counted here, it's really just the number of the DB8E as counted from the master module. Set this to `0` to suppress any display output from this circuit. |
| `header` (`hr`) | text | ☞ smart | This text is displayed at the top of the DB8E display whenever this circuit shows up in the display. If you omit this, an automatic title is used. For example if the output of this circuit is fed into an internal patch cable, the name of this cable is displayed. Setting `header = ""` or `header = 0` removes the header and just displays the actual value. |
| `select` (`s`) | integer | ☞ smart | The select input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to 1). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
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
| `trigger` (`t`) | trigger | Here comes the trigger output. Patch this to the trigger input of your drum or synth voice. |
| `gate` (`g`) | gate | The gate output is alternative to the trigger and has a variable length. It is useful when Algoquencer is used for creating melodies. Patch the gate input of an envelope or something similar here. |
| `pitch` (`p`) | stepped | Outputs the (pseudo-)random voltage (unquantized) at each step with an active gate. This honors all the settings that control the randomness and variation, like `dejavu`, `variation`, `fills` and `branches`. |
| `accent` (`ac`) | CV | Whenever a beat with an accent is being played, the value set by `accenthigh` is sent here, otherwise `accentlow`. If you are wiring this to one of the jacks of the G8 expander then that will output just 0V and 5V of course. |
| `led1 … led16` (`l`) | stepped | 1st … 16th LEDs of the step buttons. Assign these to the LEDs in the step buttons. |
| `barled1 … barled4` (`bl`) | gate | Patch these output to some LEDs in order to show you the current bar in the sequence. |
| `rollvelocity` (`rv`) | CV | If you enable rolls, then the velocity of the roll beats will be output here. For normal beats this will always be `1.0`. |
| `startofbar` (`sb`) | trigger | At the beginning of every bar a trigger is output here. |
| `muteled` (`ml`) | gate | Wire this to the LED in your mute button. It will then be lit while the voice is muted. |
| `unmuteled` (`ul`) | gate | Wire this to the LED in your unmute button (if used). It will blink while the unmute is waiting for the start of the next bar. |
| `morphled` (`mol`) | gate | This output will get a trigger every time a morph happens. It is intended to be wired to an LED. |
| `fillsled` (`fl`) | gate | This output will get a trigger every time a fill beat is being played. Wire this to some LED if you like. |
| `branch` (`br`) | integer | This output will output the current branch number, e.g. 1, 2, 3 and so on. If you do not use `branches` then it is always `1`. |
| `lengthoutput` (`lo`) | integer | Outputs the currently selected length. This is useful if you are using the `lengthbutton` for interactively changing the length of the pattern and want to share that setting with other circuits. |

## See also

- [`minifonion`](minifonion.md) — quantize the `pitch` output to a musical scale.
- [`button`](button.md) — make toggle buttons or `longpress` triggers for editing modes and preset load/save.
- [`buttongroup`](buttongroup.md) — select between several algoquencer tracks that share buttons.
- [`mixer`](mixer.md) — combine several buttons into a single preset-select trigger.
