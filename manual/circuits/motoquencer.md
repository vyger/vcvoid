---
circuit: motoquencer
title: Motor fader sequencer
obsolete: false
ram_bytes: 1184
manual_pages: [315, 316, 317, 318, 319, 320, 321, 322, 323, 324, 325, 326, 327, 328, 329, 330, 331, 332, 333, 334, 335, 336, 337, 338, 339, 340, 341, 342]
category: sequencer
tags: [sequencer, motor faders, m4, motorfader, steps, pages, gate, cv, pitch, ratchets, repeats, accumulator, quantize, scale, form, pattern, pingpong, lucky, randomize, copy, paste, keyboard, recording, linked, performance]
see_also: [motorfader, buttongroup, minifonion, chord, arpeggio, midiin, button, sinfonionlink]
impl_difficulty: hard
controller_binding: controller-required
verification: requires-human
spec_gap: false
difficulty_note: Huge feature surface (32 steps/pages, 8 fader+button modes, forms, patterns, lucky randomization, accumulator, recording, linking) driven by M4 motor faders with per-notch force feedback that has no VCV analog.
verification_note: "Step-sequencing logic can be checked headless for CV/gate given scripted inputs, but the defining motor-fader haptics (notch feedback, faders auto-moving on button touch or lucky rerolls) require a human on real M4 hardware; randomized lucky/probability paths are non-deterministic."
---

# motoquencer — Motor fader sequencer

This circuit allows you to build simple but also very complex performance
sequencers based on motorized faders. It supports up to 32 steps and up to eight
M4 controllers with up to 32 faders. The list of features is long and diverse
and aims at supporting creative live performances.

You probably will fail to map all existing inputs to controls, so better don't
try and rather experiment with just a fraction of those at a time.

## Basic minimal example

Despite all the features, this sequencer is easy to get started with. Here is the
smallest possible example. You always need a clock input. Here I get it from
input `I1`. You need to have at least one M4 unit attached to your DROID (and
declared with `[m4]` in your patch). The motor sequencer automatically configures
all your available faders (up to 32) for the sequencer (you can change that with
`firstfader` and `numfaders`):

```droid
[m4]

[motoquencer]
    clock = I1
    cv = O1
    gate = O2
```

As soon as your clock starts, you get a sequence with one step per available
fader (which is four if you have just one `[m4]` declared). The faders select
notes from a C lydian scale in two octaves. You will feel 15 notches. They
correspond to the 15 notes in this range. The touch buttons below the faders
switch on/off the gates. The gate is automatically switched on when you change
the note value of a step (move the fader to another notch).

The pitch is output at `O1` and the gate at `O2`. Well – this wouldn't have
needed expensive motor faders, but it works and shows a minimal application of
motoquencer.

## Switching pages

Your sequence can have more steps than you have faders. This is done by switching
pages. In the following example we assume that you have just one M4 but want a
sequencer with 16 steps. Use the `page` input in order to set the current page
(group of 4 steps) that you want to see and edit with your faders. These pages
have the numbers 0, 1, 2 and 3. That number can nicely be output by a
[`buttongroup`](buttongroup.md) on a P2B8. Here is a fully functional example of a
16 step sequencer with just four faders:

```droid
[p2b8]
[m4]

[buttongroup]
    button1 = B1.1
    button2 = B1.2
    button3 = B1.3
    button4 = B1.4
    led1 = L1.1
    led2 = L1.2
    led3 = L1.3
    led4 = L1.4
    output = _PAGE

[lfo]
    hz = 20 * P1.1
    square = _CLOCK

[motoquencer]
    clock = _CLOCK
    page = _PAGE
    numsteps = 16
    cv = O1
    gate = O5
```

## Repeats, ratchets and randomize

In the upper examples we just had two parameters per step of the sequence: the
pitch / CV and the gate. There are some more. Altogether every step has the
following eight parameters:

| # | Parameter |
|---|-----------|
| 0 | pitch / CV |
| 1 | randomize CV |
| 2 | gate propability |
| 3 | repeats (up to 16) |
| 4 | gate pattern |
| 5 | ratchets (up to 8) |
| 6 | gate |
| 7 | skip |

Each of these parameters has a number from 0 to 7 and you can set the input
`fadermode` to one of these in order to switch the faders to control that
parameter. Here are some details about the various parameters:

**Pitch / CV** is the output pitch of each step. With the inputs `cvbase` and
`cvrange` you can define a voltage range for those CVs. Per default, the CV is
quantized to a musical scale, but you can change that with `quantize` (see below).

**Randomize CV** is a number from 0 (fader at the bottom) to 7 (fader at the top).
0 means randomization is off. The other 7 steps will increasingly modify the
step's CV by adding a different random offset each time the step is played. At
position 7 (the maximum), the offset is up to `cvrange`, so if your CV is at
maximum, this could double up your CV range.

**Gate propability** also has 8 settings. Here the maximum (fader at top position)
is the default and means: this step is always played, if the gate is on. The
other seven settings will reduce the propability of this step being played. The
lowest setting still leaves a small chance. Turn off the gate to silence a step
completely.

But this propability is not simply a random chance. It has several very musical
settings as you can see from the following table. Here you see the eight fader
positions and their meaning – 8 being the top position and 1 the bottom position:

| Pos. | Meaning | |
|------|---------|---|
| 8 (top) | played always | 100% |
| 7 | random chance of 50% | 50% |
| 6 | played every *even* turn | 50% |
| 5 | played every *odd* turn | 50% |
| 4 | random chance of 25% | 25% |
| 3 | played every 4th turn | 25% |
| 2 | random chance of 12% | 12% |
| 1 | played if previous random was positive | – |

The LEDs below the faders indicate the current setting with different color and
blink codes:

- Gates that are played always are blue with a constant light.
- Random gates for 50%, 25% and 12% are in the same blue but blink in various speeds.
- Gates of setting 1 (conditional random) are blinking fast.
- Gates depending on the turn (3, 5 and 6) are in cyan color and light steadily
  in the bars (turns) where they are on and blink in the other bars.

The position 6 and 5 are very musical and can transform a pattern of length 8
into an effective melody of 16 steps. A step in position 6 is just played every
second run of the whole sequence. Position 5 is just the same but starts with the
first run and will then be played on run 3, 5, and so on.

Position 4 is similar, but these steps will just be played every fourth sequence
run, so you can use it for playing things like a pickup or break or the like.
These "run counters" are reset by the `reset` input.

The bottom position of 1 is an addition for the true random positions 7, 4 and 2:
A step in position 1 is played, whenever the *most recent* random decision of
positions 7, 4 and 2 was *positive*. It allows you to create groups of notes that
are either played completely or not at all: Set the first step of these to a
random propability of 50, 25 or 12%. And the remaining notes to position 1. Now
whenever fate decides that the first note is being played, so will all remaining
ones. These steps do not need to be subsequent. You can have wholes.

**Repeats** changes the number of clock cycles one step will last. It is a number
from 1 (fader at the bottom) to 16 (fader at the top). This setting changes the
total duration of one sequence cycle. If you set repeats to 2 for one of 16 steps,
your sequence will last 17 clock cycles.

Hints:

- While you move the fader in this mode, the LED below the fader helps you
  dialing in a specific number of repeats. It uses the following color scheme:
  The numbers 4, 8, 12 and 16 are displayed *red*. The numbers 2, 6, 10 and 14
  are displayed *yellow*. The remaining (odd) numbers are black (LED is off).
- When you change the number of repeats of a step, the skip setting of that step
  is automatically cleared (see below).

The **Gate pattern** decides how gates are played when *repeats* is 2 or larger.
There are four gate patterns, which you can feel in the fader. In the first
setting (fader down) just the first repetition of the step is "played" (i.e. a
gate signal sent). Setting 2 will play one gate per repetition. Setting 3 plays
one long gate. And setting 4 is like 3 but lets the gate open when the step ends.
This ties this step to the next one. And this setting also has an effect when
`repeats` is just 1.

| Pos. | Gate pattern |
|------|--------------|
| 4 (top) | Tie this step to the next one |
| 3 | play one long gate |
| 2 | play all repetitions individually |
| 1 | Just the first repetition |

**Ratchets** can be set from 1 (normal) to 8. It divides the clock cycle of the
step into equal time intervals in which the step is repeated. If you set ratchets
to 2, for example, you will get two notes played at double time. Ratchets do
*not* change the duration of the sequence.

The remaining two settings are usually set with the touch buttons, but you can
also use the faders.

**Gate** decides wether the step is "played". If it is played, its CV will be sent
to the `cv` output and the `gate` signal is set to high for half a clock cycle
(you can change all this, no worries).

Steps with **Skip** enabled will be skipped. This shortens the duration of the
sequence. Note: if *all* steps are set to skip, the sequencer repeats playing the
most recent step over and over. If you change the repeats of a step, the *skip* is
automatically removed.

So let's now make an example where we use a button group for setting `fadermode`:

```droid
[p2b8]
[m4]

[buttongroup]
    button1 = B1.1
    button2 = B1.2
    button3 = B1.3
    button4 = B1.4
    button5 = B1.5
    button6 = B1.6
    button7 = B1.7
    button8 = B1.8
    led1 = L1.1
    led2 = L1.2
    led3 = L1.3
    led4 = L1.4
    led5 = L1.5
    led6 = L1.6
    led7 = L1.7
    led8 = L1.8
    output = _FADERMODE

[lfo]
    hz = 20 * P1.1
    square = _CLOCK

[motoquencer]
    clock = _CLOCK
    fadermode = _FADERMODE
    cv = O1
    gate = O5
```

## Button mode

Very similar to the faders, also the touch buttons have modes. These can be
switched with `buttonmode` and here are the possible settings:

| # | Mode |
|---|------|
| 0 | gates |
| 1 | start / end |
| 2 | gate pattern |
| 3 | skip |

Three of these settings you already know from the `fadermode`. When the buttons
are set to gate pattern, you cycle through the four steps each time you touch the
button (and the LED cycles through four colors).

Fun fact: You can set `fadermode = 6` and `buttonmode = 0`. That way, both the
button and the fader control the gates. Try this out and touch the buttons: the
fader will move automatically.

The mode "start / end" cannot be set with the faders. They set a sub range of the
sequence to be played. Here is what it means:

### Start and end

Usually your sequence is played from the first to the last step. But you can
change this by setting a start step and an end step. You can do this in various
ways.

The first one is a manual interactive change. Set `buttonmode = 1`. This activates
the start/end configuration of the sequencer. The current start step has a green
LED and the end step a red one. Touching a button changes the *end* step. You can
set the start step by first setting an end step and *holding* that button and then
– with a second finger – press another step. This will set the start step.

The manual change of start/end can be reset with a trigger to `clearstartend`.

You can automate the manual setting of the end step (not the start step) with the
input `setendstep`. This input expects an integer number in the range 0 …
`numsteps`. Whenever this number *changes to a new non-zero value* the end step is
set to this new value. Example: the input changes from 3 to 5 → the end step is
set to step 5. If the input changes to 0 nothing happens. `clearstartend` resets
the end step to its default, which is `numsteps` unless you changed this.

The third method of setting start and end is with the inputs `startstep` and
`endstep`. While the methods just mentioned temporarily modified start and end,
these inputs set the *default* values for the start and end step. Example:

```droid
[motoquencer]
    ...
    startstep = 5
    endstep = 8
```

You can combine this with a manual modification of start and end. As soon as you
change the start/end with the step buttons in buttonmode 1 or by using setstartend,
the manual settings override the inputs `startstep` and `endstep` until you do a
clear or `clearstartend`.

## Quantization, root and scale

Per default, the CVs are quantized to the notes of a lydian C major scale, as is
the default for many other circuits, as well. This means that the faders have one
artifical notch for each scale note. You can feel the notes. This makes it easy to
change the note in exact steps without any display.

As with many other pitch-aware circuits, like for example
[`minifonion`](minifonion.md) or [`chord`](chord.md), you can use `root` and
`degree` for changing the scale. See in the table of inputs below for the
different possible scales. Note: `root` has no effect on the lower CV boundary.
It's just for the selection of the allowed notes. Use `cvbase` for setting that.

Furthermore, there are the inputs `select1`, `select3`, … You can use them to
further restrict the possible notes – or even add notes that are not contained in
the scale. Refer to the [`minifonion`](minifonion.md) circuit for a broader
discussion of these inputs.

Note: If you have set a melody with the faders and reduce the number of allowed
notes afterwards, the faders will possibly move to new positions. But as long as
you don't touch them, they will internally "remember" their original note. If you
later re-add the missing notes, the faders will move back and your original melody
is restored.

With the input `quantize` you can switch off the musical mode. `quantize = 0`
disables quantiziation and the faders create a continous CV (the internal
resolution is 127 steps, just like in a MIDI CC). And `quantize = 1` will quantize
to semitones (1⁄12 V steps).

Note: The maximum number of notches is 201. But if you select more than 25
notches, the force feedback is turned off as the notches would get too small to
work. This number of 25 "real" notches nicely matches the 25 possible semitones
of two octaves. If you increase that range, the notches are switched off.

## Direction, ping pong, movement patterns

The Motoquencer has quite a bunch of interesting features for changing the order
in which steps are being played. Some of them, like the playing direction or "ping
pong", are the usual suspects and common among sequencers. The "playing patterns"
and "forms" go beyond this and create interesting creative possibilities.

`direction` defaults to 0, which means "forwards". Set this to 1 (e.g. with a
toggle button) to run the sequence backwards.

```droid
    direction = 1 # backwards
```

`pingpong` is another switch. Setting it to 1 enables "ping pong mode". Here the
direction switches back and forth. Depending on `direction`, the sequence starts
at the start step or the end step, moves towards the other end and then turns
around in order to come back. Note: Since the steps at the turning points are
played just once, a sequence of 8 steps in ping pong mode has a duration of 14,
not 16.

```droid
    pingpong = 1 # enable ping pong
```

`pattern` changes the way how the sequencer steps through the sequence. Pattern 1
for example goes always two steps forwards (according to `direction` and
`pingpong`) and then one step backwards. Assuming `direction = 0` and
`pingpong = 0`, the step order would be 1, 2, 3, 2, 3, 4, 3, 4, 5, 4, 5, 6 and so
on. The available patterns are much the same as in the [`arpeggio`](arpeggio.md)
circuit with the addition of pattern 6, which goes forwards in small random steps.

```droid
    pattern = 3 # set pattern 3
```

## Forms like AAAB

Already confused? Then you probably won't like the "Forms" feature! Here we create
longer sequences by first dividing the steps into two (or three parts), and then
playing these parts in certain orders.

The most useful form (except the trivial 0) is probably 1, which is AAAB. Here the
steps are divided into a first half, which is called A, and a second half, which is
called B. The A part is always played thrice and then once the B part. Assuming you
have 8 steps (and all the other fancy stuff is off), the step order would be 1, 2,
3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 5, 6, 7, 8.

The patterns with the three parts A, B, and C divide the steps into three equal
sized parts. You better make sure that you have 6 or 12 or 24 steps in that case,
or else your parts won't have equal size (which on the other hand could be funny
anyway).

The forms can be combined with `direction`, `pingpong` and `pattern`. Here
stepping modifications are always applied within each individual part.

The forms can also be combined with the start and end point. Here just the steps
between start and end are divided into parts.

## Autoreset

In contrast to all the upper modifications of the step order, `autoreset` is super
simple. It resets the whole sequence (including parts) to the very beginning after
a specified number of clock ticks.

There are two typical applications: First, if you want to make sure that the
pattern repeats in some regular way despite crazy modifications, set
`autoreset = 16` and the sequence will restart exactly every 16th clock tick. If it
is longer, it will be truncated. If it is shorter, it first repeats, but then the
repetition is truncated.

On the other hand you can make a regular sequence irregular, if you set e.g.
`autoreset = 7` in a sequence with usually 16 steps, thus forcing polymetric shifts
with other parallel rhythms.

When you use the special gate "propabilities" odd and even in combination with
autoreset, please note that after a reset the odd / even count always starts with
odd.

There is one subtlety: The pitch accumulator is *not* reset by the autoreset but
rather advanced. When jamming short sequences with autoreset and at the same time
using the pitch accumulator, this is what you expect and what you want. If it would
be reset to 0, the pitch accu would not work together with auto autoreset and that
would be a shame.

## The Metric Saver

The Metric Saver™ is a very musical feature that allows you to go bonkers with all
start, end, direction, ping pong, pattern, form, repeats, autoreset and skips
without loosing the sync to the rest of your music.

If The Metric Saver™ is turned on (which is the default), the motoquencer
automatically keeps track of the original incoming clock count. As soon as – after
a polymetric journey – you come back to "normal", it jumps to the step that
*would* have been the current one without those alterations.

An example: You set autoreset to 7 in order to create polymetric tension. Later you
set it back to 0. Now the sequence immediately jumps to the step where it would
have been without autoreset (this requires that none of the other step changing
features are in use). You snap back to your original groove and are in sync again
with the rest of your modular "band".

Note: The Metric Saver™ is only activated when really all modifications to the
normal step order are turned off. That also includes steps where "repeats" or
"skip" is used, since they also introduce time shifts.

## I Feel Lucky

The Motoquencer has a powerful system of *one time randomization*, which is called
I Feel Lucky™. While setting random CVs or gate propabilities is quite common
amongst sequencers, here we talk of something different. By sending a trigger to a
certain input, some of your steps are randomly modified – and stay that way. If
your faders currently show these steps, you will immediately see them moving
around. And they stay there, so that you can manually modify the random decision
if you like.

Those triggers are most times sent by buttons, but also slowly running LFOs or
using the `startofsequence` as a trigger are fine.

Let's make a simplified example:

```droid
[motoquencer]
    ... usual stuff goes here ...
    luckychance = P1.1
    luckyamount = P1.2
    luckycvs = B1.1 # press to reroll CVs
```

All *lucky* operations honor the `luckychance` input. This sets the relative
number of steps that is affected by the randomization. Setting it to 1 will affect
all steps. At 0, no step is affected. At 0.5 exactly half of the steps is affected,
randomly chosen from all steps between start and end.

A trigger to `luckycvs` sets a new random CV value for each affected step. And with
the pot `luckyamount` you control the maximum CV that's possible here.

You can use this mechanism also to reset things. A trigger at `luckycvs` whith
`luckyamount = 0` and `luckychance = 1` will bring all steps back to the CV set by
`cvbase`.

Please have a look at the table of inputs for all the other `lucky...` triggers
and … feel lucky!

## Pitch accumulator

The pitch accumulator – or just short accumulator – is a way to alter the notes of
certain steps in a predictable way as the sequence goes on. It works like this:

First you turn it on by setting `accumulatorrange` to a non-zero number. Let's
assume you set it to 4.

```droid
[motoquencer]
    accumulatorrange = 4
    ...
```

If you do this, the fader in the fader mode "pitch randomization" changes its
meaning slightly. The upper four settings are now dedicated to the accumulator
while the lower three settings still do pitch randomization:

| Pos. | Meaning |
|------|---------|
| 7 | accumulator: shift up twice each turn |
| 6 | accumulator: shift up each turn |
| 5 | accumulator: shift down each turn |
| 4 | accumulator: shift down twice each turn |
| 3 | strong pitch randomization |
| 2 | medium pitch randomization |
| 1 | slight pitch randomization |
| 0 | randomization + accumulator off |

Let's assume you set a step to 6 (shift up each turn). Now the note of this step is
increased by one note every repetition of the sequence. Every time it restarts
from step 1, the internal accumulator is increased by one and the note is moved up
one one within the selected scale notes.

If `accumulatorrange = 4`, the accumulator is reset after the sequence has played
four times and all notes are restored to their original values. The same does an
extern reset signal.

With the four fader positions 4, 5, 6 and 7 of a step, you can have the note moved
up once or twice or on the contrary moved downwards once or twice per repetition.

If you enable a form like AAAB, the accumulator is increased at the end of the
complete form. So even if the A part repeated three times, the accumulator-
sensitive steps change their note for each repetition of A but just at the end of
the whole sequence.

If you use `autoreset`, the pitch accumulator is increased every time the auto
reset hits.

## Multiple tracks

Each motoquencer circuit has just one CV and one gate output. In many cases it is
desirable to have several CVs and maybe also additional gate outputs as part of a
sequence. Also you probably want more sequencers using the same faders, of course.

This is done by adding more instances of motoquencer to your patch. The easiest way
is to use the `select` input of each of these, in order to make sure that at every
time exactly *one* motoquencer is selected and gets access to the motor faders. You
really shouldn't try selecting more than one at the same time, or your faders will
get crazy!

Here is an example with the two buttons `B1.7` and `B1.8` selecting one of two
sequencers:

```droid
[p2b8]
[m4]

[buttongroup]
    button1 = B1.7
    button2 = B1.8
    led1 = L1.7
    led2 = L1.8

[lfo]
    hz = 20 * P1.1
    square = _CLOCK

[motoquencer]
    clock = _CLOCK
    select = L1.7
    cv = O1
    gate = O5

[motoquencer]
    clock = _CLOCK
    select = L1.8
    cv = O2
    gate = O6
```

This simple patch is a fully functional two-track four-step sequencer. And as long
as you don't run out of RAM, you can add as many tracks as you like.

One thing you have to have in mind: These sequencers can easily go out of sync.
Just play around with the start or end step or skip or repeats. While that can be
interesting, sometimes it is not desirable. Maybe you just want every step to have
additional CV or gate values.

This can be done by *linking* two or more instances of motoquencer together. To do
that, add the following line to the first instance:

```droid
    linktonext = 1
```

At the next motoquencer in the patch, don't wire `clock` or `reset` or anything
else that deals with stepping or direction or faders. Just connect the outputs. The
linked sequencer is *remote controlled*.

Some inputs still apply for the linked sequencer. One example is `cvbase` and
`cvrange`. Any parameter that has an influence on which step is played when,
however, is ignored. That task is done by the main sequencer.

Here is a complete example that adds one additional CV and one gate to a sequencer.
Note: The fader modes 10 and 16 give you access to the modes 0 and 6 of the linked
sequencer. Simply add 10 for each sequencer in the chain.

```droid
[p2b8]
[m4]

[buttongroup]
    button1 = B1.1
    button2 = B1.2
    button3 = B1.3
    button4 = B1.4
    button5 = B1.5
    button6 = B1.6
    button7 = B1.7
    button8 = B1.8
    led1 = L1.1
    led2 = L1.2
    led3 = L1.3
    led4 = L1.4
    led5 = L1.5
    led6 = L1.6
    led7 = L1.7
    led8 = L1.8
    output = _FADERMODE
    value7 = 10 # CV of sequencer 2
    value8 = 16 # gate of sequencer 2

[lfo]
    hz = 20 * P1.1
    square = _CLOCK

[motoquencer]
    clock = _CLOCK
    fadermode = _FADERMODE
    linktonext = 1
    cv = O1
    gate = O5

[motoquencer]
    cv = O2
    gate = O6
```

If you need more than two CVs, you can create even longer chains, for example:

```droid
[motoquencer]
    clock = _CLOCK
    fadermode = _FADERMODE
    linktonext = 1
    cv = O1
    gate = O5

[motoquencer]
    linktonext = 1
    cv = O2
    gate = O6

[motoquencer]
    cv = O3
    gate = O7
```

Simply add a `linktonext` at every instance except the last. And add 10 to
`fadermode` for every sequencer. For example `fadermode = 25` selects fader mode 5
on the third sequencer in the chain.

Here are some details, what linking exactly means for the linked sequencer:

- The linked sequencer does not react to `clock`, `reset`, `startstep`, `endstep`,
  `form`, `direction`, `pingpong`, `pattern`, `autoreset`, `shiftsteps` or any
  other potential means of influencing the play order of the steps. Instead the
  current step number of the linked sequencer will always be the same as the step
  number of the main sequencer.
- If you use `shiftsteps`, `luckyshuffle` or `luckyreverse` on the main sequencer,
  the exact same rearrangement of steps will happen at the linked sequencers.
- If the main sequencer plays repeats, so does the linked one. The "repeats"
  setting of the linked sequencer's steps are ignored.
- If the main sequencer skips a step, so does the linked one. The "skip" property
  of steps in the linked sequencer are ignored, as well.
- Ratches still work independently, since they don't change the step sequence.
- Also the gate pattern of the linked sequencer will be applied.
- In the linked sequencer, `holdcv` has one additional value: 2. If you set it to
  2, the CV output of the linked sequencer is synchronized to the gate of the
  linked sequencer, not to that of the main sequencer.
- Don't use `select`, `fadermode` and `buttonmode` on the linked sequencer. They
  are ignored. Instead, for accessing the parameters of the steps of the linked
  sequencer, add 10 to `fadermode` or `buttonmode`. So while `fadermode = 1` sets
  the fader to the CV randomization of the main sequencer, so does `fadermode = 11`
  for the linked sequencer.

The following parameters are still valid for the linked sequencer:

- `cvbase`, `cvrange` and `quantize`
- `gatelength`
- `holdcv` (with the extra value 2)
- `luckychance`, `luckyamount` and all of the other `lucky...` paramters, with the
  exception of `luckyskips`, `luckyrepeats`, `luckyshuffle` and `luckyreverse`.

## Recording with a keyboard

You can use a keyboard to record sequences into your motoquencer. More precisely,
you can attach a CV / gate input for that purpose. That might very well come from a
keyboard attached to the X7, via the circuit [`midiin`](midiin.md). But any other
source is possible, as well.

The first step is attaching your recording source to `keyboardcv` and
`keyboardgate`. Here is an example:

```droid
[midiin]
    cv = _CV
    gate = _GATE

[motoquencer]
    keyboardcv = _CV
    keyboardgate = _GATE
    ...
```

After doing this, you should already be able to play with your keyboard directly to
the voice that's attached to the motoquencer. While a key is pressed (`keyboardgate`
is high), the `keyboardcv` has precedence over the sequence. But you can change that
with the setting `keyboardmode`.

To record your keyboard into a sequence, you need to connect `recordmode`, maybe to
a [`button`](button.md). While recording is active and the keyboard gate is high,
the current sequencer step will be replaced with your keyboard note. Otherwise the
steps are untouched. That way you play more and more notes into the sequence.

In order to get rid of existing notes, either clear the sequence before recording
(using the `clear` trigger), or make use of the input `recordsilence`. Setting that
to 1 will silence all steps when no key is pressed.

You also can route `recordsilence` to one key on your keyboard using the `notegate`
outputs of [`midiin`](midiin.md). That way you can actively "erase" notes by
pressing that key.

While recording key presses the motoquencer tries to be tolerant with respect to
your timing. So keys pressed slightly before or after the current clock tick are
just fine.

Note: The sequencer can just record into its grid of steps and quantized notes. So
it's not a free style MIDI recorder. You cannot record notes that are faster than
your input clock. If you have enabled quantiziation, you can just play notes from
the current scale. So it needs some time to get familiar with this way of
recording. Nevertheless it's a great tool for rapid composition. Especially because
it's easy to modify your melodies with the faders after you have recorded them.

### Recording & linked sequencers

When you have combined several motoquencers with `linktonext = 1`, recording also
works in the linked sequencers. Here are some hints:

- `recordmode` can be (and must be) set individually on each of the motoquencers.
  If you want to record into a linked sequencer, make sure that you set `recordmode`
  there.
- Using the same value for `recordmode` for all sequencers means that they always
  record simultanously.
- Also `keyboardcv` and `keyboardgate` are settings that each sequencer instance
  has on its own. That means that you can record different CVs with different gates
  on each sequencer at the same time.
- Using the same gate signal for the `keyboardgate` of all sequencers can make
  sense. E.g. if you want to record paraphonic chords or pitches together with
  modulation CVs.

## Copy & paste pages

The copy & paste feature allows you to copy a part of your sequence from one page
to another or from one preset to another. To do this, map the inputs `copy` and
`paste` to two buttons (you don't need toggle buttons here, so no button circuit is
needed).

A trigger to `copy` copies the current page of the current sequence into an
internal clipboard. And `paste` copies the clipboard to the current page of the
current sequence.

By changing pages or presets between copy and paste, you can copy parts of your
sequence elsewhere.

There are also two alternative triggers for pasting. `pastefaders` just pastes the
faders of the currently selected mode. `pastebuttons` is likewise for the buttons.
With that you can for example just copy the gate propabilites from one page to
another while leaving the rest of the parameters as they are.

If you have linked sequencers, those will automatically be handled as well. Don't
connect the copy and paste triggers there.

## Doubling the range

The by far most widely used application of copy & paste is to copy the steps from
the first page (for example steps 1 … 8) and paste these into the second half
(steps 9 … 16) in order to create a sequence where that second half is musically
similar than the first half. If that's your main (maybe only) application, you can
use the `doublerange` input instead of copy and paste.

You just need a single button or trigger. Map it to `doublerange`:

```droid
[motoquencer]
    ...
    doublerange = B2.23
```

Here is how it works:

1. Start with a reduced range of your sequence by setting the end step to something
   less than the number of steps (see above for you to change the end step). For
   example step the end step to step 8 of 16.
2. Press the `doublerange` button.
3. Now two things happen:
   - Steps 1 … 8 will be copied into 9 … 16.
   - The end step is set to step 16 (and counts as manually modified).

So the sequence is doubled in length, but since the contents of the second half is
a copy of the first half, there is no *audible* difference. Now you can edit the
second half to create some variation and maybe use the `form` feature for using an
AAAB or AABB form.

You even can start with a single step by setting the end to the first step. Pressing
`doublerange` once doubles the length from 1 to 2. Pressing again makes a sequence
of 4 identical steps. And so on…

## Copy & paste single steps

You can also copy individual steps. For this you need just one button. Map this to
the input `stepcopy`:

```droid
[motoquencer]
    ...
    stepcopy = B2.24
```

And this is how you copy steps:

1. Press and *hold* the step copy button.
2. Touch the plate below any sequencer step. This step is then copied into an
   internal clipboard.
3. Touch the plate of another step. The copied step is pasted here.
4. Touch more other steps to copy there, as well.
5. Release the step copy button.

Notes:

- Always *all* aspects of the step are copyied.
- If you work with linked sequencers, settings of linked sequencers are copied, as
  well. Do not use the `stepcopy` input there.
- If your dexterity allows you can switch to another page of steps while still in
  the copy mode and thus paste the copied step to another page.
- It might be more convenient if you use a toggle button (see
  [`button`](button.md)), so that you start and end the copying by pressing the
  button and do not need to hold it all the time.

Hint: It is allowed to map both `stepcopy` and `doublerange` to the same
(non-toggling) button. The MFPS (see the manual, [basics](../basics.md)) does this
to save one button. In this case the `doublerange` feature is activated if you
press and release the button without touching any step button inbetween. Otherwise
a step copy is done and no range doubling happens.

## LED colors

Depending on the `buttonmode`, the LEDs below the faders have different colors.
Here is an overview over all possible colors:

| color | meaning | buttonmode |
|-------|---------|------------|
| white | currently played step | always |
| blue | enabled gate | 0 |
| green | start step | 1 |
| red | end step | 1 |
| cyan | gate on the first repetition | 2 |
| pink | gate on each repetition | 2 |
| orange | hold gate over duration | 2 |
| yellow | tie the gate to the next step | 2 |
| violet | skip | 3 |

## Display

If you have a DB8E display controller (see the manual, [hardware](../hardware.md)),
the motoquencer circuit automatically uses the display whenever you edit something
in the sequencer. For example the note value of a display is displayed when you
change it (e.g. showing "Pitch" and the note "E♭1").

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `firstfader` (`f`) | integer | `1` | First M4 fader of the sequencer (starting with 1). If you omit this, it starts at the first fader of your first M4. |
| `numfaders` (`n`) | integer | | Number of faders to use for your sequencer. The typical numbers are 4, 8, 16 and 32. 32 is the maximum (eight M4 units). If you omit this, all of your M4 faders will be used. |
| `numsteps` (`ns`) | integer | | Number of steps your sequence consists of (at maximum). The number of steps can be greater than the number of faders. In that case use `page` for paging your faders so that you can edit all of the steps. Having the number of steps less than the faders, makes no sense – it's just a waste of faders. The maximum number of steps is 32. If you don't set this parameter, the number of steps will be set to the number of faders. Note: changing this setting dynamically can provoke various surprising behaviours. For example the number of pages (see parameter `page`) might be reduced. Or the end marker is forcibly moved around. If you want to change the length of the sequence via CV, better use `endstep` or `autoreset`. Another note: Setting `numsteps` will *not* restrict the number of faders. If you set `numsteps = 4` but have eight faders available, the circuit will use all these, even if faders 5, 6, 7 and 8 will be useless. You need to set `numfaders = 4` in this situation. |
| `page` (`p`) | integer | `0` | Use this parameter, if you have less faders than steps. The first page is 0, not 1. For example if you have 4 faders but 16 steps, you can select between the four "pages" of four faders each, by settings bar to 0, 1, 2 or 3. The output of a [`buttongroup`](buttongroup.md) with one button per page is a good match for this parameter. |
| `clock` (`c`) | trigger | | Patch an input clock here. If you want to use ratcheting, that clock needs to be stable and regular, because the sequencer needs to interpolate the clock and create evenly distributed new beats within two clock ticks. If you don't use ratching, you can use any rhythm you like here – may it be shuffled, euklidean, the output from another sequencer or whatever you like. Each clock tick will advance the sequence to the next step (or to the next repition of the current step). |
| `taptempo` (`tt`) | trigger | | If your clock is not steady but might be stopped and restarted, you should patch a steady clock here. This avoids problems with the computation of the gate length right after starting the clock. The tap tempo computation needs a series of at least two clock pulses so the gate length of the first step is wrong after the clock has stopped for a while. |
| `reset` (`r`) | trigger | | A trigger here resets the sequencer to its start step. The next clock tick (or a tick that is roughly at the same time as the reset) will play step 1. Note: If there is a reset *without* a clock tick at the same time, the sequencer will go to "step 0", which is a special state where it waits for the clock to advance to the first step. Without that fancy logic, a reset plus clock would skip step 1 and start with step 2. |
| `run` (`ru`) | gate | `1` | If you set this input to 0, the sequencer will ignore all incoming clock ticks. It stops. The default of 1 is normal operation, where it runs. This input is a better way to temporarily stop the sequencer than to stop the clock. The reason: for computing the gate length and ratchets a steady input clock is needed. If you stop the clock, the next gate length and ratches right after the next start will have the wrong duration since at least two clock ticks are neccessary for computing its speed. Note: This input is not a replacement for `mute`, since a muted sequencer leaves the clock running and advances steps. It just mutes the gate output. |
| `composemode` (`cm`) | gate | | Enabling "compose mode" makes it easier to find the right note in a step, when creating more complex melodies. When `composemode` is set to 1, the sequencer stops clocking. Instead – every time you change the CV of a step, it immediately jumps to that step, outputs the changed CV and opens the gate for a short time, so you can listen to the changed note. |
| `mute` (`m`) | gate | | If you set this to 1, the gate output of the sequencer is muted (will always be 0). Any changes of the CV output still happen. |
| `cvbase` (`cb`) | CV | `0.0` | Here you set the voltage the sequencer will output if the CV fader is at the bottom position. The allowed range is -1 … 1 (which is the same as the range from -10 V to +10 V, if you output the CV directly to an output). |
| `cvrange` (`cr`) | CV | `0.2` | CV range of the faders. So the resulting CV lies somewhere between `cvbase` and `cvbase` + `cvrange`. The CV range cannot be negative and is capped at 1. If you need a greater range, consider multiplying the output value of the `cv` output of the sequencer. |
| `invert` (`iv`) | gate | `0` | Inverts the CV or pitch output. This is like mirroring the fader position. Now if the fader is up, the output is low and vice versa. In scale quantized mode, the melody still stays within the scale. |
| `quantize` (`q`) | integer | `2` | Switches on quantization in two levels. At 0, the faders run freely and output a continous CV. At 1, the output is quantized to semitones, which is 1⁄12 V steps. Also the faders will get artifical notches – one for each semitone. That is, unless your range is too large. The maximum number of notches with force feedback is 25, so if your range exceeds two octaves (0.2), the notches are turned off. At 2, the output is quantized to the scale that `root` and `degree` define. Furthermore the individual scale notes can be switched on or off with the parameters `select1`, `select3` and so on. Note: the `root` input does not select the lowest note of the CV range. That is still set with `cvbase`. It is just used for selecting the scale. Values: `0` = no quantization; `1` = quantize to semitones (1/12V steps); `2` = quantize to the scale set by root and degree. |
| `cvnotches` (`cn`) | integer | `0` | Usually the CVs of the steps are ment to be note pitches (when `quantize` is 1 or 2), or just free CVs (`quantize = 0`). There is an alternative mode, however, that allows you to assign integer values like 0, 1, 2 and so on to each step. To do this set `cvnotches` to a value of 2 or greater. This defines the number of discrete values (and hence notches) for each step and put CVs of the sequence into *notched mode*. 1 makes no sense, of course, since in this "pitch bend mode" the faders would always return to the neutral position. In notched mode the `cv` output does not output a pitch but a notch number starting from 0. `cvbase`, `cvrange` and `quantize` are ignored. The maximum number of notches is 127, but the haptic force feedback of the motor faders is disabled starting at 26. |
| `shiftsteps` (`sh`) | integer | `0` | Shifts all your steps by that number to the left (negative numbers shift to the right). So if `shiftsteps` is 1, right after a reset, the sequencer will not play step 1, but step 2. The shifting wraps around at the end of your sequence, so if you have 24 steps and shift is 1, the sequencer will play step 1 instead of step 24. Note: Other things like `startstep`, `endstep`, `playmode`, `from` and `autoreset` take place *after* shifting. |
| `startstep` (`ss`) | integer | `1` | Sets the first step to be used. This means that after a reset or when the sequencer comes to the end of the sequence, it will begin at this step. There is also a way for setting start and end with buttons (see `buttonmode`). If you use the interactive mode, the `startstep` and `endstep` settings will be overridden. They are reactived if you `clear` everything. Note: `startstep` and `endstep` take place after applying `shiftsteps`. |
| `endstep` (`es`) | integer | | Defines the last of the steps to be played. The default is to play all steps. After playing the end step, the sequencer moves on to the start step at the next clock tick. There is also a way for setting start and end with buttons (see `buttonmode`). If you use the interactive mode, the `startstep` and `endstep` settings will be overridden. They are reactived if you `clear` everything or trigger `clearstartend`. If `startstep` is equal to `endstep`, the sequence just consists of one single step. Setting `startstep` larger then `endstep` is allowed and reverses the playing order. |
| `setendstep` (`ses`) | integer | | This input is similar to `endstep` but avoids the problem of `endstep` that its value is overridden as soon as you manually change the end step with the button (see `buttonmode`). This input acts as a number input and trigger at the same time. As soon as you send a different number than 0, the end step is set to that value *as if you had manually changed it* with the step buttons in button mode 1. The effect always happens when you change the input number. E.g. when it changes from 3 to 5, the end step is set to five *once*. This allows you to directly patch the output of a [`buttongroup`](buttongroup.md) to this input. The input value 0 does not change the end step. |
| `form` (`fo`) | integer | `0` | This is an advanced feature that allows you to slice your steps into two or three parts and create musical song forms like AAAB or ABAC. Each of the parts A, B or C are then played according to the playmode. The form AAAB, for example, creates a 32 step form from just 16 steps, by playing the first 8 steps three times and then the second 8 steps once. Available forms: `0` = A (forms are basically deactivated); `1` = AAAB; `2` = AABB; `3` = ABAC; `4` = AAABAAAC; `5` = AB; `6` = AAB. Notes: The splitting of the steps into parts takes place *after* accounting for `startstep` and `endstep`. Forms with A, B and C split the pattern into three parts. These parts can only be of equal size if the number of steps is dividable by 3, of course. The pattern AB is really not the same as A, e.g when `direction` is set 1 (reverse). In that case each of the parts is played backwards, but the parts themselves move forwards on your steps. |
| `direction` (`d`) | gate | `0` | Sets the general direction in which the sequencer moves through the steps. 0 means forwards and 1 means backwards. |
| `pingpong` (`pp`) | gate | `0` | If set to 1, the sequencer will change the direction every time it reaches the start or end of the sequence. |
| `pattern` (`pt`) | integer | `0` | Selects one of a list of movement patterns. That way, the sequence steps are not played in linear order but in a more sophisticated movement. Available patterns: `0` = go step by step to the sequence (normal) →; `1` = two steps forward, one step backward →→←; `2` = double step forward, one step backward ⇒←; `3` = double step forward, double step backward, single step forward ⇒⇐→; `4` = double step forward, single step forward, double step backward, single step forward ⇒→⇐→; `5` = random single step forward or backward ↔; `6` = go forward by a small random number of steps →×?; `7` = random jump to any allowed (other) note ⇕. |
| `repeatshift` (`rs`) | integer | `0` | This is a number in the range -24 … +24. If you set this to non-zero, each repetition of a step shifts the played note by that many notes within the selected scale notes. This only has effect on steps where the number of repeats is greater than 1. Also it is only applied when the `quantize = 2`. |
| `ratchetshift` (`ras`) | integer | `0` | This is a number in the range -24 … +24. If you set this to non-zero, each ratchet of a step shifts the played note by that many notes within the selected scale notes. This only has effect on steps where the number of ratchets is greater than 1. Also it is only applied when the `quantize = 2`. If you combine `ratchetshift` with `repeatshift`, both shifts are added together. And the ratchet shifting is reset for each repetition of the step. |
| `accumulatorrange` (`ac`) | integer | ☞ smart | Using this parameter enables the pitch accumulator. The value sets the maximum value the accumulator can get. The maximum is 16. If you set this to 0, the fader mode for pitch randomization still is in the special mode where the upper four positions control the impact of the accumulator. See the pitch accumulator section above for details. |
| `constantlength` (`co`) | gate | `0` | This input enables a feature that (tries to) keep the actual length of the sequence constant. There are two levels. If `constantlength = 1`, every change in the *repeats* of a step is compensated by changing the repeats in the following steps. E.g. if you increase the number of repeats from 4 to 5 in step 3 (by moving the fader in the appropriate fader mode), the repeats in step 4 are reduced by 1. If they are already 1, step 5 is tried an so on, until it wrap around to step 1. The second level is `constantlength = 2`. Here also the *skip* setting of steps is honored and modified in order to keep the length constant. A skipped step essentially has the length 0 (or 0 repeats). The componsation is now done not only when the repeats are changed but also when skip is switched on or off on a step. All the compensation is only active with the range that is set with the start and end step. |
| `autoreset` (`ar`) | integer | `0` | If set to non-zero, automatically issues a reset (just like a trigger to `reset`) every N clock ticks. The single difference to a reset is that the pitch accumulator is not reset but advanced. While this sounds like a bug it is musically more useful and is what you expect intuitively (believe me). |
| `metricsaver` (`ms`) | gate | `1` | The Metric Saver™ helps you to reliably come back to your original metric and time after playing around with all sorts of parameters that change the played number of steps in the sequence. These are: `startstep`, `endstep` (also when changed interactively), `form`, `direction`, `pingpong`, `pattern`, `autoreset` and repeats and skips of individual steps. Therefore it counts the actual number of clock cycles since the last external reset (or system start). And when *all* of these features are deactivated, it snaps back the clock to the position it *would* have been by now if you never had played around with all the funny stuff. That way, during a live performance, you can safely play around with all this polymetric and otherwise time disrupting stuff and as soon as you clean up your mess – voila: you are back on track and in sync with the rest of the "band". The metric saver is turned on by default. But you can disable it by setting the parameter to 0. |
| `fadermode` (`fm`) | integer | `0` | Switches the current meaning of the motor faders. You probably want to connect the output of a [`buttongroup`](buttongroup.md) here. Possible modes: `0` = pitch / CV; `1` = randomize CV; `2` = gate propability; `3` = repeats (up to 16); `4` = gate pattern; `5` = ratchets (up to 8); `6` = gate; `7` = skip. (For linked sequencers add 10 per instance.) |
| `buttonmode` (`bm`) | integer | `0` | Switches the current meaning of the touch buttons below the faders. You probably want to connect the output of a [`buttongroup`](buttongroup.md) here. Possible modes: `0` = gates; `1` = start / end; `2` = gate pattern; `3` = skip. |
| `holdcv` (`hc`) | gate | `1` | This setting determines wether the CV output changes every time the sequencer moves to the next step or just when that step is active (a gate is being played). The latter is the default. But if you set this to 0, the CV values of steps without gates will also influence the output CV. Note: regardless of this setting, the CV will never change inbetween. Any change of the CV faders, the `cvbase` and `cvrange` and so on will only take effect when the next step is played. This also ensures that repeats or ratchets are always in the same pitch. (In a linked sequencer the extra value 2 syncs the CV to the linked sequencer's own gate.) |
| `defaultcv` (`dc`) | CV | `0.0` | Set the CV the steps should be set to on a trigger to `clear`. That value must be within the range set by `cvbase` and `cvrange`, or it will be truncated to that range. If you have set `cvnotches`, however, the value is expected to be an integer in the range 0 … `cvnotches` - 1. |
| `defaultgate` (`dfg`) | gate | `1` | Here you set to which state (on / off) the gates should be set on a trigger to `clear`. |
| `clearskips` (`cs`) | trigger | | A trigger here removes the "skip" setting from all steps. |
| `clearrepeats` (`crp`) | trigger | | A trigger here resets the number of repeats to 1 for each step. |
| `clearstartend` (`cse`) | trigger | | A trigger here clears the manual settings of the start and end step. So the sequence will be played in its full length (again). |
| `gatelength` (`gl`) | CV | `0.5` | The gate length in input clock cycles. A value of 0.5 thus means half a clock cycle. A steady input clock is needed for this to work. Please note that if the gate length is >= 1.0, two succeeding notes will get a steady gate, which essentially means legato. If you don't use a steady clock, set this parameter to 0. This will output a minimal gate length of about 10 ms (basically just a trigger). |
| `keyboardmode` (`km`) | integer | `1` | This input sets how a keyboard, that is hooked to `keyboardcv`, and `keyboardgate` should be used for directly playing notes. You can set it to 0, 1 or 2: `0` = ignore the keyboard inputs; `1` = keyboard and sequencer play together, keyboard has precedence; `2` = mute sequencer, just play keyboard. |
| `keyboardcv` (`kc`) | pitch | | The pitch input of a keyboard. This is used for playing along with the `keyboardmode` or recording with `recordmode`. |
| `keyboardgate` (`kg`) | gate | | The gate input of a keyboard. A positive gate enabled play along (see `keyboardmode`) and also recording, if `recordmode` is set accordingly. |
| `recordmode` (`rm`) | integer | `0` | Use this input to record melodies played with a keyboard (namely `keyboardcv` and `keyboardgate`) into the sequencer. There are three possible settings: `0` = don't record; `1` = record, notes longer than one step will automatically tie steps via the gate pattern; `2` = record, don't tie notes. Ignore the length of the input note. |
| `recordsilence` (`rsi`) | gate | `0` | When this input is set to 1 while recording, silence will be recorded while `keyboardgate` is off. Otherwise you can just add notes to the sequence. |
| `copy` (`cy`) | trigger | | A trigger here copies the current page of the sequence to an internal clipboard. The clipboard is not part of any preset and is also not saved to the SD card. It can be used later for pasting it's data to another preset or another page of a sequence. |
| `paste` (`pa`) | trigger | | A trigger here copies the steps from the clipboard to the current page. |
| `pastefaders` (`pf`) | trigger | | This is like `paste`, but just the values of the faders of the current `fadermode` are copied. |
| `pastebuttons` (`pb`) | trigger | | This is like `paste`, but just the values of the faders of the current `buttonmode` are copied. Note: the button mode "start / end" is not supported by copy and paste. |
| `stepcopy` (`sc`) | gate | | Wire this input to a push button to enable copying of individual sequencer steps. Hold the button to start copying. Then touch one of the step buttons. This step is copied into the clipboard. When you press further step buttons while you still holding the button the copied step's settings overwrite the pressed step. |
| `doublerange` (`dr`) | trigger | | A trigger here doubles the current playing range and copies the contents of the previous range to the second half of the new range. This only works if the playing range (start/stop) is not at maximum. |
| `bulkedit` (`be`) | gate | | If you set this to 1, then if you move one fader (or encoder) or touch/press one button, all other faders (or encoders) *at the right of the modified step* will move along to the same value – even in the steps that are currently on another page. The application is to wire this input to some button. While you hold the button you can bring a certain parameter to the same value for many steps super fast. Do this on step 1 to modify the whole track. |
| `linktonext` (`ln`) | gate | `0` | This settings allows you to create motoquencer tracks that have more than one CV or gate output for each step. If you set this to 1, the next motoquencer circuit in your patch will by synchronized to this one. This means that it always plays the same step number – including all fancy operating like `shiftsteps`, `startstep`, `endstep`, `form`, `pattern` and `autoreset`. All those inputs and also `clock` and `reset` are *ignored* by the next motoquencer. The same holds for the "repeats" and "skip" setting of the steps. `fadermode` and `buttonmode` are extended to the next motoquencers by adding 10 for each motoquencer to follow. So `fadermode = 10` will show the CV of next motoquencer in the faders. `fadermode = 11` the CV randomization of the next motoquencer. `fadermode = 20` show the CV of the third linked motoquencer and so on. Don't set `fadermode` or `buttonmode` on the linked motoquencers. They will be ignored there. Note: The `linktonext` setting cannot by dynamically changed. It needs to be fixed 0 or 1. You cannot use any button or internal cable or other methods to change it while the patch is running. |
| `luckychance` (`lc`) | 0..1 | `1.0` | Sets tha chance for a step to be affected by the next "lucky" operation (see triggers below). |
| `luckyscope` (`ls`) | integer | `0` | Determines which part of the sequence is affected by the "lucky" operations. Depending on this setting: `0` = all steps between the current start and end step; `1` = all steps; `2` = all steps between start and end on the current page; `3` = all steps on the current page. |
| `luckyamount` (`la`) | 0..1 | `1.0` | Sets the amount of change that a "lucky" operation does to a step. The meaning depends on the operation. See the parameters below. |
| `luckycvbase` (`lv`) | 0..1 | `0.0` | This parameter affects only the operation `luckycvs` and `luckyfaders` when the fader mode is set to 0. It adds a fixed amount of CV to the random result, so that lies in the range `luckycvbase` … (`luckycvbase` + `luckyamount`). |
| `luckyfaders` (`lf`) | trigger | | Moves the currently selected faders (according to `fadermode`) to new random positions. `luckyamount` sets the maximum value of the fader, where 1 allows the maximum. |
| `luckybuttons` (`lb`) | trigger | | Randomly toggles the currently selected buttons (according to `buttonmode`). `luckyamount` only has an effect when the gate patterns are selected, since here, four different values are possible. `luckamount` restricts them if it is lower than 1. |
| `luckycvs` (`lcv`) | trigger | | Replaces the affected steps' CVs with a new random CVs. The lowest possible CV is `cvbase`. If `luckyamount` is 1, the highest possible CV is `cvbase` + `cvrange`, otherwise it is `cvbase` + `luckyamount` × `cvrange`. |
| `luckycvdrift` (`ld`) | trigger | | Modifies the affected steps' CV randomly up or down. They will stay in the CV range set by `cvbase` and `cvrange`. `luckyamount` controls the amount of change. |
| `luckyspread` (`lr`) | trigger | | First computes the average CV of all steps. Then changes the CV values of the affected steps such that their distance to the average increases or decreases. If `luckyamount` is greater than 0.5, the distance is increased. Otherwise it is decreased. |
| `luckyinvert` (`li`) | trigger | | Inverts the CVs of the affected steps within the allowed CV range. `luckyamount` has no influence. |
| `luckyrandomizecv` (`lrc`) | trigger | | Sets the "randomize CV" values of the affected steps to random values (yes, this is double randomization). The `luckyamount` sets the maximum randomization value that will be set. |
| `luckygates` (`lg`) | trigger | | Sets the gates of the affected steps randomly to on or off. The chance for on is determined by `luckyamount`. So with `luckyamount = 0` you clear all gates and with `luckyamount = 1` you set all gates. |
| `luckyskips` (`lk`) | trigger | | Sets the "skip this step" setting of the affected steps randomly to skip or normal. The chance for skip is determined by `luckyamount`. |
| `luckyties` (`lt`) | trigger | | Sets the "tie this step to the next" setting of the affected steps randomly to tie or normal. This is the same as setting the gate pattern to the upper most position. The chance for tie is determined by `luckyamount`. |
| `luckygatepattern` (`lgp`) | trigger | | Randomizes the gate pattern of the selected steps (there are four different values: once, all, hold and tie). Use `luckyamount` to reduce that set. |
| `luckygateprob` (`lga`) | trigger | | Sets the "randomize gate" values of the affected steps to random values (yes, this is double randomization). The `luckyamount` sets the minimum randomization value that will be set (yes, this is inverted). So with `luckyamount = 1` you disable randomization and make the gates play always. With `luckymount = 0` you set the gate propability to the lowest possible value (still not 0). |
| `luckyrepeats` (`lrp`) | trigger | | Randomly sets the number of repeats of the affected steps to something between 1 and 16 (the maximum). The `luckyamount` determines the maximum repetition number, where 1 stands for a maximum of 16 repetitions. |
| `luckyratchets` (`lrt`) | trigger | | Randomly sets the number of ratches of the affected steps to something between 1 and 8 (the maximum). The `luckyamount` determines the maximum ratchet number, where 1 stands for a maximum of 8 ratchets. |
| `luckyshuffle` (`lsh`) | trigger | | Randomly swaps all affected steps (their playing order) together will all their attributes. `luckyamount` has no influence. |
| `luckyreverse` (`lrv`) | trigger | | Reverses the playing order of the affected steps. `luckyamount` has no influence. |
| `buttoncolor` (`bc`) | CV | `0.1` | Set a user defined color for the gate buttons. This color is only used when `buttonmode = 0`. The main purpose of this option is to allow a separate color for the gate button in a linked sequencer, that does something else than gates. |
| `cvname` (`ce`) | text | | If you use a DB8E display controller, here you can set the title of the display when changing the step's CV value. Without this parameters it's just "CV" or "Number". |
| `gatename` (`gn`) | text | | If you use a DB8E display controller, here you can set the title of the display when changing the step's gate. Without this parameters it's "Gate" and the main text is either "silent" or "play". When you use this parameter, you select a custom title and the text changes to "on" and "off". |
| `display` (`dy`) | integer | `1` | This parameter selects the number of the DB8E display controller, where any change of this sequencer's state will show up. Per default, the first DB8E available is used. Set this to 2 if you have a second DB8E and want to use that. Note: any other controller is not being counted here, it's really just the number of the DB8E as counted from the master module. Set this to 0 to suppress any display output from this circuit. |
| `root` (`ro`) | integer | `0` | Set the root note here. 0 means C, 1 means C♯, 2 means D and so on. If you multiply the value of an input like `I1` with 120, then you can use a 1V/Oct input for selecting the root note via a sequencer, MIDI keyboard or the like. Also then you are compatible with the ROOT CV input of the Sinfonion. Values: `0` = C, `1` = C♯, `2` = D, `3` = D♯, `4` = E, `5` = F, `6` = F♯, `7` = G, `8` = G♯, `9` = A, `10` = A♯, `11` = B, `12` = C. |
| `degree` (`dg`) | integer | `0` | Set the musical scale. This is a number from 0 to 107. The first 12 and most important scales: `0` = lyd – Lydian major scale (it has a ♯4); `1` = maj – Normal major scale (ionian); `2` = X7 – Mixolydian (dominant seven chords); `3` = sus – mixolydian with 3rd/4th swapped; `4` = alt – Altered scale; `5` = hm5 – Harmonic minor scale from the 5th; `6` = dor – Dorian minor (minor with ♯13); `7` = min – Natural minor (aeolian); `8` = hm – Harmonic minor (♭6 but ♯7); `9` = phr – Phrygian minor scale (with ♭9); `10` = dim – Diminished scale (whole/half tone); `11` = aug – Augmented scale (just whole tones). Note: Alltogether there are 108 scales. Please see the manual ([scales](../scales.md)) for a complete list. |
| `select1` (`s1`) | gate | ☞ smart | Gate input for selecting the *root* note as being an allowed interval. When you want to create a playing interface for live operation you can patch the output of a toggle button (made with the circuit `[button]`) here. Note: When all `select` and `selectfill` inputs are 0, automatically all seven scale notes are selected, i.e. `select1` … `select13` will be set to one. |
| `select3` (`s3`) | gate | ☞ smart | Gate input for selecting the 3rd. |
| `select5` (`s5`) | gate | ☞ smart | Gate input for selecting the 5th. |
| `select7` (`s7`) | gate | ☞ smart | Gate input for selecting the 7th. |
| `select9` (`s9`) | gate | ☞ smart | Gate input for selecting the 9th (which is the same as the 2nd). |
| `select11` (`s11`) | gate | ☞ smart | Gate input for selecting the 11th (which is the same as the 4th). |
| `select13` (`s13`) | gate | ☞ smart | Gate input for selecting the 13th (which is the same as the 6th). |
| `selectfill1` (`sf1`) | gate | `off` | Selects the alternative 9th (i.e. the 9th that is *not* in the scale). |
| `selectfill2` (`sf2`) | gate | `off` | Selects the alternative 3rd (i.e. the 3rd that is *not* in the scale). |
| `selectfill3` (`sf3`) | gate | `off` | Selects the alternative 4th or 5th. In most cases this is the diminished 5th. |
| `selectfill4` (`sf4`) | gate | `off` | Selects the alternative 13th (i.e. the 13th that is *not* in the scale). |
| `selectfill5` (`sf5`) | gate | `off` | Selects the alternative 7th (i.e. the 7th that is *not* in the scale). |
| `harmonicshift` (`has`) | integer | `0` | This input can reduce harmonic complexity by disabling some of the scale or non-scale notes. It is an idea first found in the Sinfonion and also provided by the circuit [`sinfonionlink`](sinfonionlink.md). `harmonicshift` is staged after the `select...` inputs and further filters out (disables) notes based on their relation to the current scale. This means that first the 12 `select...` inputs select a subset of the 12 possible notes. After that `harmonicshift` can reduce this set further (it will never add notes). If `harmonicshift` is not zero, depending on its value some or more of the scale notes are disabled, even if they would be allowed by `select...`. You also can use negative values. These create rather strange sounds by removing the *simple* chord functions instead of the complex ones first. Values: `0` = off – all selected notes are allowed; `1` = disable all fill notes (non-scale notes); `2` = disable fills and 11th; `3` = disable fills, 11th and 13th; `4` = disable fills, 11th, 13th and 9th; `5` = disable fills, 11th, 13th, 9th and 7th; `6` = disable fills, 11th, 13th, 9th, 7th and 3rd; `7` = disable fills, 11th, 13th, 9th, 7th, 3rd and 5th; `-1` = disable the root note; `-2` = disable the root note and the 5th; `-3` = disable root, 3rd, 5th; `-4` = disable root, 3rd, 5th, 7th; `-5` = disable root, 3rd, 5th, 7th, 9th; `-6` = disable root, 3rd, 5th, 7th, 9th and 13th; `-7` = disable all scale notes (fill notes untouched). |
| `noteshift` (`nos`) | integer | `0` | Shifts the resulting output note(s) by this number of *scale* notes up or down (if negative). So the output note still is part of the scale but may be a note that is none of the selected ones. The maximum shift range is limited to -24 … +24. |
| `selectnoteshift` (`sns`) | integer | `0` | Shifts the output note by this number of *selected* scale notes up or down (if negative). If you use `noteshift` at the same time, first `selectnoteshift` is applied, then `noteshift`. The maximum shift range is limited to -24 … +24. |
| `tuningmode` (`tm`) | gate | `off` | While this is 1, the circuit will output the value set by `tuningpitch` instead of the actual pitch. This is ment to be a help for tuning your VCOs. |
| `tuningpitch` (`tp`) | pitch | `0V` | This pitch CV will be output while the tuning mode is active. |
| `transpose` (`tr`) | pitch | `0V` | This value is being added to the output pitch when not in tuning mode. It can be used for musical transposition or adding a vibrato. |
| `select` (`s`) | integer | ☞ smart | The select input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to 1). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | ☞ smart | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is 0, the circuit will be active if `select` is exactly 0 instead of a positive gate signal. In some cases this is more convenient. |
| `preset` (`pr`) | integer | ☞ smart | This is the preset number to save or to load. Note: the first preset has the number 0, not 1! For the whole story on presets please refer to the manual ([basics](../basics.md)). This circuit has 4 presets, so this number ranges from 0 to 3. |
| `loadpreset` (`lp`) | trigger | | A trigger here loads a preset. As a speciality you can use the trigger for selecting a preset at the same time. |
| `savepreset` (`sp`) | trigger | | A trigger here saves a preset. |
| `clear` (`cl`) | trigger | | A trigger here loads the default start state into the circuit. The presets are not affected, unless you use direct preset switching with the `preset` input and without triggers. And that case the current preset is also cleared. |
| `clearall` (`ca`) | trigger | | A trigger here loads the default start state into the circuit and into all of its presets. |
| `dontsave` (`dos`) | gate | `0` | If you set this to 1, the state of the circuit will not saved to the SD card and not loaded from the SD card when the Droid starts. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `cv` () | CV | The CV output (or pitch output, if you use `quantize`). |
| `gate` (`g`) | gate | The gate output. |
| `startofsequence` (`ssq`) | trigger | Outputs a trigger whenever the sequencer starts playing from the beginning. This can be used for synchronizing with other sequencers. An external `reset` will also cause this output to trigger. |
| `startofpart` (`spa`) | trigger | Outputs a trigger whenever a form part starts again. This is only interesting when you use `form`. |
| `startstepout` (`sso`) | integer | Outputs the current start step. This is useful in case it has been interactively set with the buttons and you need that information for another circuit. |
| `endstepout` (`eso`) | integer | Outputs the current end step. This is useful in case it has been interactively set with the buttons and you need that information for another circuit. |
| `currentstep` (`cst`) | integer | Outputs the number of the step that is currently being played (starting from 0). |
| `currentpage` (`cpg`) | integer | Outputs the number of the fader page that is currently played, i.e. the value you would have to feed into `page` in order to see the currently being played step. |
| `accumulator` (`acc`) | integer | The current value of the pitch accumulator (an integer number in the range 0 … 16). |

## See also

- [`motorfader`](motorfader.md) — direct access to a single motor fader.
- [`buttongroup`](buttongroup.md) — commonly wired to `page`, `fadermode` and `buttonmode`.
- [`minifonion`](minifonion.md) — broader discussion of `root`, `degree` and the `select...` inputs.
- [`arpeggio`](arpeggio.md) — same movement patterns as the `pattern` input.
- [`midiin`](midiin.md) — a typical source for `keyboardcv` / `keyboardgate` recording.
- [`button`](button.md) — for wiring `recordmode`, `stepcopy` and other triggers.
- [`sinfonionlink`](sinfonionlink.md) — origin of the `harmonicshift` idea.
