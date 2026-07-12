---
circuit: arpeggio
title: Arpeggiator – pattern based melody generator
obsolete: false
ram_bytes: 144
manual_pages: [140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150]
category: sequencer
tags: [arpeggiator, melody, pattern, pitch, scale, root, degree, interval, clock, 1v/oct, drop, octaves, pingpong, butterfly, harmonicshift, transpose]
see_also: [chord, minifonion, sinfonionlink]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: "Deterministic pattern engine over a rich scale/interval selection, but every parameter is well documented and reproducible from a clock."
verification_note: "Headless: clock the circuit and compare the pitch sequence against computed pattern/scale/drop/octave logic. Partial — patterns 5 and 6 are random and only their allowed note pool can be checked."
---

# arpeggio — Arpeggiator – pattern based melody generator

This circuit creates melodic patterns based on simple rules and many interesting
configuration settings, which can lead to very simple but also most complex
patterns.

## Introduction

In order to better understand how the arpeggiator works, let's compare four
different ways for constructing melodies:

| Method | Character |
|--------|-----------|
| Sequencer | manually composed melodies |
| Random generator | completely chaotic sequences |
| Turing machine, Algoquencer | pseudo-random melodies, which repeat themselves |
| Arpeggiator | melodies constructed from rules |

The rules for the arpeggiator can be as simple as *on each clock tick play the
next note in the C minor scale*. Additional parameters are for example the pitch
range, i.e. the start and the end note.

The arpeggiator shares root, scale and interval selection with
[`chord`](chord.md) and [`minifonion`](minifonion.md). If you own a Sinfonion:
the arpeggiator in the DROID is working a bit differently and is more about
general principles than about preprogrammed patterns. That makes it more
flexible and powerful.

## The simplest possible example

As always, we start with the simplest possible example. And it is simple,
indeed, since each of the many parameters has a useful default value. The only
input the arpeggiator *always* needs is a clock input. The word "clock" is
probably a bit misleading since it doesn't *need* to be a steady clock signal.
It can be any rhythmic pattern you like. Each clock tick advances the melody to
the next note and a new pitch CV will be presented at `output`, which is, of
course, in the typical 1V/oct scheme.

```droid
[arpeggio]
    clock = I1
    output = O1
```

Patch `I1` to an external clock and `O1` to the 1V/oct of some synth voice. The
easiest way is to use the same clock also for triggering the voice's envelope.

Now you will hear a C major scale (lydian) being played step by step in a range
from 0 V to 2 V. This makes 15 notes, since the scale consists of the seven
notes C, D, E, F♯, G, A and B and is repeated over two octaves, but the C is
here three times: at the beginning, in the middle and at the end.

When it reaches the end it immediately starts over again. So the second "bar" is
really just 7 eights here!

## Root, scale and interval selection

You probably don't like lydian C major. Changing that is easy with the inputs
`root` and `degree`. Please have a look at the [`minifonion`](minifonion.md)
circuit for an explanation of these parameters. You find the complete table of
all 108 scales in the manual ([scales](../scales.md)).

Let's go for a D minor (natural) scale as an example:

```droid
[arpeggio]
    clock = I1
    output = O1
    root = 2
    degree = 7
```

**Stop!** At this point you probably will complain about the fact that the
arpeggio still begins with C and not with D! But this is really the intended
behaviour!

The understanding to this lies in the parameters `pitch` and `range`. These
parameters set the pitch range within which the arpeggio travels. The default is
to start at 0 V (`pitch = 0`) and go two octaves up (`range = 2V`). But 0 V
corresponds to a C!

In other words: specifying a D (`root = 2`) as the root just selects the
collection of notes to use – not where to start.

You still want to start at D? No problem, just start the pitch range at D. This
is done by using the pitch of D as the lowest pitch. A D is two semitones above
C, so we need 2 × 1⁄12 V, which is `2/120` in DROID language. Let's also set the
range to one octave (`1V`):

```droid
[arpeggio]
    clock = I1
    output = O1
    root = 2
    degree = 7
    pitch = 2/120
    range = 1V
```

And voilà: here you get the D minor scale arpeggiated!

## Patterns

This "go through the scale" mode is just one of several possible patterns. The
pattern is selected with the `pattern` input. And the default value of `0`
produces the result we just have seen. Let's look at pattern `1`. This goes two
steps forward and one step backward in the scale:

```droid
[arpeggio]
    clock = I1
    output = O1
    root = 2
    degree = 7
    pitch = 2/120
    range = 1V
    pattern = 1
```

Since pattern 1 repeats its structure every three notes it's best to display it
in a metric that is divisible by three.

Pattern 2 is similar, but makes one double step forward instead of two single
steps.

Pattern 3 goes a double step forward, a double step backward and a single step
forward.

Pattern 4 is even more sophisticated. It goes a double step forward, a single
step forward, a double step backward and again a single step forward.

Pattern 5 is a bit different since for each note it flips a coin for deciding
whether to go one step up or down.

And Pattern 6 simply randomly chooses one of the possible notes. So strictly
spoken this has nothing to do with "arpeggiation", but it's fun, so what?

Note: it's not entirely impossible that future versions of the arpeggiator
introduce new patterns. So better do not yet rely on these numbers to be fixed
forever.

## The range

Per default the pattern is played in a range of two octaves. But that can be set
easily with two parameters. `pitch` defines the lowest possible pitch of a note.
The arpeggiator will chose the start note such that it is in the scale and just
at or above this pitch.

And `range` defines the voltage range the pattern is being played upwards until
it starts again. So if `range` is 2 V, you get a range of two octaves. A range
of 0 will deform the pattern into one single note.

For interactive playing, mapping `pitch` and `range` to pots is fun:

```droid
[p2b8]

[arpeggio]
    clock = I1
    output = O1
    pitch = P1.1
    range = P1.2
```

## Changing the playing direction

So far all pattern where going more or less upwards. From lower notes to higher
notes. This can be changed by setting `direction` to `1`. Now the arpeggiator
starts with the highest allowed note and reverses the pattern for going
downwards. Why not map this setting to a nice toggle button?

```droid
[p2b8]

[button]
    button = B1.1
    led = L1.1
    output = _DIRECTION

[arpeggio]
    clock = I1
    output = O1
    pitch = P1.1
    range = P1.2
    direction = _DIRECTION
```

Another setting that influences the direction is the `pingpong` parameter. This
is a binary (gate) input, too. If it is set to 1 the direction of the pattern
changes into the opposite once the end of the range has been reached. Check this
example...

```droid
[arpeggio]
    clock = I1
    output = O1
    pingpong = 1
    pitch = 0
    range = 7/120
```

... will create the following melody. Why is that? Well – 7⁄120 is the same as
7 × 1⁄12 V, which is 7 semitones, which is in turn one fifth. Since no root and
degree are defined we are back at C major lydian. The pattern is 0 (default) –
hence the simple note-by-note scale. And `pingpong = 1` makes the pattern going
down again after having reached the upper limit.

## Octaves up and down

The nice thing about all these parameter is that you can combine them all. They
interact with each other and most combinations do useful things (well, when
using the "random" pattern, the direction and pingpong are without effect, of
course). And there is one more fun setting: `octaves`. This can be 0 (default)
or 1 or 2.

When `octaves` is 1, each note is directly followed by the same note one octave
above. That octave note is ignoring the `range`-parameter. It is always in
addition to the selected range. Here is an example:

```droid
[arpeggio]
    clock = I1
    output = O1
    range = 1V
    octaves = 1
```

Set `octaves = 2` and you get the same but the octaves go *down* instead.

## Dropping

The `drop` input lets you select different schemes of leaving out notes from the
original line of scale notes. For example `drop = 1` will leave out every second
note. Here is an example:

```droid
[arpeggio]
    clock = I1
    output = O1
    drop = 1
```

If you have a closer look, you will see that in the upper octave other notes are
being played than in the lower octave. This can sound very interesting!

Dropping can, of course, be combined with other patterns as well. Let's see the
line for pattern 1:

```droid
[arpeggio]
    clock = I1
    output = O1
    drop = 1
    pattern = 1
```

There are more dropping-schemes. Please have a look into the table of input
parameters down below.

## Note selection

The most important thing comes last. For didactical reasons! What *really* makes
this arpeggiator so musically versatile is its interval selection. This is the
same as for the [`minifonion`](minifonion.md) and the [`chord`](chord.md)
generator.

The point is that you are not restricted to the seven notes of a scale. For this
there are seven inputs `select1`, `select3`, ... `select13` that select the
notes of the current scale and another five inputs `selectfill1` ...
`selectfill5` that select the notes not in the current scale. These 12 inputs
are binary inputs that expect either 0 or one 1. Each of them selects one of the
seven intervals of the scale for being part of the chord. Here is a table of all
these inputs and the notes they would select in a C major or C minor scale:

| Input | interval | step | C maj | C min |
|-------|----------|------|-------|-------|
| `select1` | root | I | C | C |
| `select3` | 3rd | III | E | E♭ |
| `select5` | 5th | V | G | G |
| `select7` | 7th | VII | B | B♭ |
| `select9` | 9th = 2nd | II | D | D |
| `select11` | 11th = 4th | IV | F | F |
| `select13` | 13th = 6th | VI | A | A♭ |

Let's make a simple example: The arpeggio of a C major *triad* over two octaves
going up and down again:

```droid
[arpeggio]
    clock = I1
    output = O1
    select1 = 1
    select3 = 1
    select5 = 1
    pingpong = 1
```

One typical way to select these notes is with seven toggle buttons. Much like
the Sinfonion. Assign the output of each of the seven buttons to one of these
functions:

```droid
[p2b8]

[button]
    button = B1.1
    led = L1.1

[button]
    button = B1.2
    led = L1.2

[button]
    button = B1.3
    led = L1.3

[button]
    button = B1.4
    led = L1.4

[button]
    button = B1.5
    led = L1.5

[button]
    button = B1.6
    led = L1.6

[button]
    button = B1.7
    led = L1.7

[arpeggio]
    clock = I1
    select1 = L1.1
    select3 = L1.2
    select5 = L1.3
    select7 = L1.4
    select9 = L1.5
    select11 = L1.6
    select13 = L1.7
    output = O1
```

Now you can switch on and off scale notes for being part of the patterns. Have
fun!

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `pitch` (`p`) | pitch | `0V` | Sets the base pitch of the arpeggio. The first note of the pattern will be the nearest selected note just above that pitch. |
| `range` (`ra`) | pitch | `2V` | Selects the range between the lowest and highest note of the arpeggio. A range of 0 means that there is just one single note possible and the arpeggio will stick to that note. A value of 1 V (or 0.1) means that the arpeggio will run over one octave. The maximum allowed range is 0.8 (8 octaves). Higher values will be capped to that. |
| `clock` (`c`) | trigger | | This input is vital: each trigger here make the arpeggio move forward by one step and adapt the pitch output. Without a clock the arpeggio will do nothing but stick to the same note all the time. |
| `reset` (`r`) | trigger | | Resets the arpeggio to the first step of the current pattern. |
| `pattern` (`pt`) | integer | `0` | Selects one of a list of arpeggio pattern. The following patterns are available:<br>`0` step forward through the allowed notes (→)<br>`1` two steps forward, one step backward (→→←)<br>`2` double step forward, one step backward (⇒←)<br>`3` double step forward, double step backward, single step forward (⇒⇐→)<br>`4` double step forward, single step forward, double step backward, single step forward (⇒→⇐→)<br>`5` random single step forward or backward (↔)<br>`6` random jump to any allowed (other) note (⇕) |
| `direction` (`d`) | gate | `0` | Sets the general direction in which the pattern moves. `0` means upwards and `1` means downwards. |
| `pingpong` (`pp`) | gate | `0` | If set to `1`, the pattern will reverse its direction once it has reached the end of the range. Otherwise it restarts from the beginning. So enabling `pingpong` is a bit like a triangle wave, whereas otherwise it's more like a sawtooth. |
| `butterfly` (`by`) | gate | `0` | If set to `1`, every second note in the range of selected notes will be mirrored. So for example you have selected the notes 1 - 10, the new order will be 1, 10, 2, 9, 3, 8, 4, 7, 5, 6. |
| `drop` (`dr`) | integer | `0` | Selects a scheme of skipping some of the allowed scale notes. Four different values are allowed:<br>`0` Do not skip any notes<br>`1` Skip every second selected note<br>`2` Skip every third selected note<br>`3` Skip the 2nd and 3rd note of each group of three |
| `octaves` (`oc`) | gate | `0` | When this is set to 1 or 2, each note will be followed by the same note one octave up (for `1`) or down (for `2`) respectively. These additional octave notes are in addition to the selected range.<br>`0` Don't play octaves<br>`1` Each note is followed by the same note one octave up<br>`2` Each note is followed by the same note one octave down |
| `startnote` (`sn`) | integer | `0` | When `startnote` is set to non-zero, it will force the pattern to begin with a certain scale note regardless of the current note selection. `1` will select the first note of the scale (root), `2` the second and so on until `7`, which selects the 7th as start note.<br><br>Using `startnote` effectively reduces the range of notes. Instead of the full range of `pitch` … `pitch + range` a reduced range is played, since some of the lower notes are skipped, if the direction is upwards, and some of the upper notes, if the direction is downwards.<br><br>The start note is used in all situations where the pattern is reset to its beginning. This is after an external reset or if the pattern has reached the end of the range. Note: If you have set `pingpong = 1`, the pattern is never reset by itself, so `startnote` is just used after an external reset, here.<br><br>Don't mess up the start note with the *lowest* note in the arpeggio. If want to control the lowest note, used `pitch` instead of `startnote`. Sometimes this has a similar effect, but not always. |
| `autoreset` (`ar`) | integer | `0` | When `autoreset` a non-zero number, the arpeggio melody will be reset to the start after that number of clock ticks. For example if you set `autoreset = 5`, and the pattern would play 7 notes until it loops back to its start, after the 5th step a restart is forced. That's also true if the pattern is shorter. If `autoreset = 5` and the melody already loops after 3 steps, it is played once in full (3 steps) and once just the first 2 steps, since then the auto reset happens.<br><br>A trigger to `reset` makes `autoreset` set it's internal counter to 0.<br><br>Autoreset gives you direct control over the rhythmic feel that your arpeggio melodies have. |
| `root` (`ro`) | integer | `0` | Set the root note here. `0` means C, `1` means C♯, `2` means D and so on. If you multiply the value of an input like `I1` with 120, then you can use a 1V/Oct input for selecting the root note via a sequencer, MIDI keyboard or the like. Also then you are compatible with the ROOT CV input of the Sinfonion.<br><br>`0` C · `1` C♯ · `2` D · `3` D♯ · `4` E · `5` F · `6` F♯ · `7` G · `8` G♯ · `9` A · `10` A♯ · `11` B · `12` C |
| `degree` (`dg`) | integer | `0` | Set the musical scale. This is a number from `0` to `107`. Below are the first 12 and most important scales. You find a list of all 108 scales in the manual ([scales](../scales.md)).<br><br>`0` lyd – Lydian major scale (it has a ♯4)<br>`1` maj – Normal major scale (ionian)<br>`2` X7 – Mixolydian (dominant seven chords)<br>`3` sus – mixolydian with 3rd/4th swapped<br>`4` alt – Altered scale<br>`5` hm5 – Harmonic minor scale from the 5th<br>`6` dor – Dorian minor (minor with ♯13)<br>`7` min – Natural minor (aeolian)<br>`8` hm – Harmonic minor (♭6 but ♯7)<br>`9` phr – Phrygian minor scale (with ♭9)<br>`10` dim – Diminished scale (whole/half tone)<br>`11` aug – Augmented scale (just whole tones)<br><br>Note: Alltogether there are 108 scales. Please see the manual ([scales](../scales.md)) for a complete list. |
| `select1` (`s1`) | gate | ☞ smart | Gate input for selecting the *root* note as being an allowed interval. When you want to create a playing interface for live operation you can patch the output of a toggle button (made with the circuit [`button`](button.md)) here.<br><br>Note: When all `select` and `selectfill` inputs are 0, automatically all seven scale notes are selected, i.e. `select1` ... `select13` will be set to one. |
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
| `harmonicshift` (`has`) | integer | `0` | This input can reduce harmonic complexity by disabling some of the scale or non-scale notes. It is an idea first found in the Sinfonion and also provided by the circuit [`sinfonionlink`](sinfonionlink.md).<br><br>`harmonicshift` is staged after the `select...` inputs and further filters out (disables) notes based on their relation to the current scale. This means that first the 12 `select...` inputs select a subset of the 12 possible notes. After that `harmonicshift` can reduce this set further (it will never add notes).<br><br>If `harmonicshift` is not zero, depending on its value some or more of the scale notes are disabled, even if they would be allowed by `select...`. Or in other words: the harmonic material is reduced.<br><br>You also can use negative values. These create rather strange sounds by removing the *simple* chord functions instead of the complex ones first.<br><br>Here are the possible values:<br>`0` off – all selected notes are allowed<br>`1` disable all fill notes (non-scale notes)<br>`2` disable fills and 11th<br>`3` disable fills, 11th and 13th<br>`4` disable fills, 11th, 13th and 9th<br>`5` disable fills, 11th, 13th, 9th and 7th<br>`6` disable fills, 11th, 13th, 9th, 7th and 3rd<br>`7` disable fills, 11th, 13th, 9th, 7th, 3rd and 5th<br>`-1` disable the root note<br>`-2` disable the root note and the 5th<br>`-3` disable root, 3rd, 5th<br>`-4` disable root, 3rd, 5th, 7th<br>`-5` disable root, 3rd, 5th, 7th, 9th<br>`-6` disable root, 3rd, 5th, 7th, 9th and 13th<br>`-7` disable all scale notes (fill notes untouched) |
| `noteshift` (`nos`) | integer | `0` | Shifts the resulting output note(s) by this number of *scale* notes up or down (if negative). So the output note still is part of the scale but may be a note that is none of the selected ones. The maximum shift range is limited to -24 … +24. |
| `selectnoteshift` (`sns`) | integer | `0` | Shifts the output note by this number of *selected* scale notes up or down (if negative). If you use `noteshift` at the same time, first `selectnoteshift` is applied, then `noteshift`. The maximum shift range is limited to -24 … +24. |
| `tuningmode` (`tm`) | gate | `off` | While this is `1`, the circuit will output the value set by `tuningpitch` instead of the actual pitch. This is ment to be a help for tuning your VCOs. |
| `tuningpitch` (`tp`) | pitch | `0V` | This pitch CV will be output while the tuning mode is active. |
| `transpose` (`tr`) | pitch | `0V` | This value is being added to the output pitch when not in tuning mode. It can be used for musical transposition or adding a vibrato. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | pitch | This is what it's all about: here comes the pitch CV for the current arpeggio note. |

## See also

- [`minifonion`](minifonion.md) — shares the same root, scale and interval selection.
- [`chord`](chord.md) — chord generator using the same note-selection scheme.
- [`sinfonionlink`](sinfonionlink.md) — provides the `harmonicshift` idea.
