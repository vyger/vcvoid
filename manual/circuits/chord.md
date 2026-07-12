---
circuit: chord
title: Chord generator
obsolete: false
ram_bytes: 144
manual_pages: [169, 170, 171, 172, 173, 174, 175, 176]
category: chord-harmony
tags: [chord, harmony, pitch, voices, scale, root, degree, inversion, spread, sinfonion, select, harmonicshift, transpose, voicing]
see_also: [button, minifonion, sinfonionlink]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: "Deterministic four-voice chord builder over scale/interval selection with inversion, spread, doubling rules and 3/2-voice modes."
verification_note: "Headless: set root/degree/select inputs and compare the four pitch outputs against computed scale-note, voicing and spread rules. Partial — the exact spacing when notes are distributed across the spread should be spot-checked against hardware."
---

# chord — Chord generator

This circuit creates the pitch information for up to four voices of a musical
chord. This means that you can attach the Volts per octave inputs of up to four
synth voices and they will play a nice musical chord.

Hereby you have the flexibility of building your chord out of any of the seven
notes of a selected scale. So you are not limited to root, 3rd, 5th and 7th. The
algorithm is similar to that in the Sinfonion but has an adapted mode for three
voiced chords in addition.

## Minimal example

Here is the most simple (and probably useless) example: it will play a C major 7
chord, i.e. output the respective pitch CVs for the notes C, E, G and B at the
outputs `O1`, `O2`, `O3` and `O4`:

```droid
[chord]
    output1 = O1
    output2 = O2
    output3 = O3
    output4 = O4
```

Output `O1` will be at 0 V, representing a C. Of course, if you just have three
voices, don't use `output4` and you will get a C major triad.

## Selecting root and scale

Most likely you do not want to play in C major all the time (or even never!), so
you can select the root note and the scale with the inputs `root` and `degree`.
Setting `root` to 2 and `degree` to 7, for example, will select D natural minor:

```droid
[chord]
    output1 = O1
    output2 = O2
    output3 = O3
    output4 = O4
    root    = 2
    degree = 7
```

`root` ranges from 0 to 11 and `degree` from 0 to 107. You find the complete
table of all 108 scales in the manual ([scales](../scales.md)).

But why the heck is that input named `degree`?? Well, it's a jargon from the
Sinfonion and does make sense there in some contexts. Please have a look into the
manual of the Sinfonion if you are interested!

## Selecting the pitch of the notes

Per default all outputs are in the first octave, i.e. in the range 0 V … 1 V. Per
convention this is very low and probably sounds ugly. With the `pitch` input you
can set the minimum pitch of the lowest output chord note. In the next example
this is read from `I1`. So you could, for example, patch a sequencer here and have
the chord outputs play a kind of four voiced melody:

```droid
[chord]
    pitch = I1
    output1 = O1
    output2 = O2
    output3 = O3
    output4 = O4
    root    = 2
    degree = 7
```

The `spread` parameter controls the maximum pitch of the highest output chord
note. It is always relative to the pitch of the lowest note plus one octave. So
if `spread` is 1.5 V (or 0.15), for example, the maximum allowed distance between
the lowest and the highest chord note is 2.5 octaves. As lowest note the chord
generator places the chord note that is nearest above the `pitch` input. As
highest note it places the one nearest to upper bound of the allowed range and
the remaining notes are distributed in between with the most equal spacing
possible.

## Selecting the chord notes

What makes the Sinfonion and also the harmonic circuits in the DROID stand apart
from other modules is the flexibility of note selection. So e.g. in C major, you
are not limited to playing the chord C/E/G/B. In fact you can choose any subset
from the currently selected scale.

For this there are seven inputs `select1`, `select3`, … `select13` that select
the notes of the current scale and another five inputs `selectfill1` …
`selectfill5` that select the notes not in the current scale. These 12 inputs are
binary inputs that expect either 0 or one 1. Each of them selects one of the seven
intervals of the scale for being part of the chord. Here is a table of all these
inputs and the notes they would select in a C major or C minor scale:

| Input | interval | step | C<sup>maj</sup> | C<sup>min</sup> |
|-------|----------|------|------|------|
| `select1` | root | I | C | C |
| `select3` | 3rd | III | E | E♭ |
| `select5` | 5th | V | G | G |
| `select7` | 7th | VII | B | B♭ |
| `select9` | 9th = 2nd | II | D | D |
| `select11` | 11th = 4th | IV | F | F |
| `select13` | 13th = 6th | VI | A | A♭ |

One typical way to select these notes is with seven toggle buttons, which is then
much like the Sinfonion does it. Assign the output of each of the seven buttons to
one of these functions:

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

[chord]
    select1 = L1.1
    select3 = L1.2
    select5 = L1.3
    select7 = L1.4
    select9 = L1.5
    select11 = L1.6
    select13 = L1.7
    output1 = O1
    output2 = O2
    output3 = O3
    output4 = O4
```

Now you can use the buttons to change the chord notes on the fly. Of course,
however, you also can use other signals for the selection. Maybe random gates,
slowly running LFOs, a sequencer, whatever you like!

But what happens, if you do not select exactly four notes?

- If you don't select any note (or do not patch the `select`-inputs at all), all
  scale notes are selected.
- If you select just one note, all four outputs will play that same note.
- If you select two notes, `output1` and `output3` will play the first note and
  `output2` and `output4` the second one.
- If you select three notes, `output4` will play the same as `output1`.
- If you select five, six or seven notes, just the first four notes will be used.

If some of the notes are doubled and you use a large enough `spread`, they will be
placed at different octaves.

By the way: It's of course no problem to just use three or even just two of the
outputs, if you don't need or have a total of four voices.

## Chord inversion

The chord generator lets you nail down the chord structure to a certain
inversion. If you set `inversion` to 1, the root note (or, to be more precise,
the first selected note) will be placed as the lowest note. Similarly the
inversions 2, 3 and 4 will make the respective other selected notes the lowest
note.

Setting `inversion` to 0 (which is the default) will allow any note to be the
lowest. This allows the chord to be closest to the `pitch` input.

## Triggered mode

The `trigger` input is essentially a sample & hold for the outputs. So as soon as
you patch that input, all outputs are frozen until the next trigger.

## Chords with three voices

The chord generation circuit can also create chords with just three output
voices. Simply omit the output `output4`. When it is not connected, the "three
voice mode" is activated:

```droid
[chord]
    output1 = O1
    output2 = O2
    output3 = O3
    root    = 2
    degree = 7
```

All parameters work as expected but there are some important adaptions. This is
not the same as using the four voiced mode and just look at the first three
outputs. For example:

- The spreading uses a simplified algorithm with just a bottom, middle and top
  note.
- If just three intervals are selected, you don't get a duplication of the first
  note on `output2`, as you would otherwise.

## Chords with two voices

Even if just two outputs are connected, you can still make use of this circuit.
Now just the first two `select...` inputs are taken into account. But things like
inversion and spreading works nevertheless.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `pitch` (`p`) | pitch | `0V` | This sets the minimum pitch of the lowest note of the chord. |
| `spread` (`s`) | pitch | `0V` | Selects the range between the lowest and highest note of the chord measured in 1V/oct, while a spread of 0 means that all chord notes are within one octave, a spread of 1 V means that the notes are spread out over two octaves and so on. |
| `inversion` (`iv`) | integer | `0` | Selects the inversion of the chord. 1 means that the root note should be the lowest note, 2 will make the second selected note the lowest note, 3 the 3rd and 4 the 4th. The default, however, is 0 and doesn't fix the inversion. Rather that inversion is chosen that creates the chord closest to the input pitch. |
| `trigger` (`t`) | trigger | | This jack is optional. If you patch it, the Chord generator just reads a new input pitch when it receives a trigger. |
| `root` (`ro`) | integer | `0` | Set the root note here. 0 means C, 1 means C♯, 2 means D and so on. If you multiply the value of an input like `I1` with 120, then you can use a 1V/Oct input for selecting the root note via a sequencer, MIDI keyboard or the like. Also then you are compatible with the ROOT CV input of the Sinfonion. See the root note table below. |
| `degree` (`dg`) | integer | `0` | Set the musical scale. This is a number from 0 to 107. See the scale table below for the first 12 and most important scales. You find a list of all 108 scales in the manual ([scales](../scales.md)). |
| `select1` (`s1`) | gate | ☞ smart | Gate input for selecting the root note as being an allowed interval. When you want to create a playing interface for live operation you can patch the output of a toggle button (made with the circuit [`button`](button.md)) here. Note: When all `select` and `selectfill` inputs are 0, automatically all seven scale notes are selected, i.e. `select1` … `select13` will be set to one. |
| `select3` (`s3`) | gate | ☞ smart | Gate input for selecting the 3rd. |
| `select5` (`s5`) | gate | ☞ smart | Gate input for selecting the 5th. |
| `select7` (`s7`) | gate | ☞ smart | Gate input for selecting the 7th. |
| `select9` (`s9`) | gate | ☞ smart | Gate input for selecting the 9th (which is the same as the 2nd). |
| `select11` (`s11`) | gate | ☞ smart | Gate input for selecting the 11th (which is the same as the 4th). |
| `select13` (`s13`) | gate | ☞ smart | Gate input for selecting the 13th (which is the same as the 6th). |
| `selectfill1` (`sf1`) | gate | `off` | Selects the alternative 9th (i.e. the 9th that is not in the scale). |
| `selectfill2` (`sf2`) | gate | `off` | Selects the alternative 3rd (i.e. the 3rd that is not in the scale). |
| `selectfill3` (`sf3`) | gate | `off` | Selects the alternative 4th or 5th. In most cases this is the diminished 5th. |
| `selectfill4` (`sf4`) | gate | `off` | Selects the alternative 13th (i.e. the 13th that is not in the scale). |
| `selectfill5` (`sf5`) | gate | `off` | Selects the alternative 7th (i.e. the 7th that is not in the scale). |
| `harmonicshift` (`has`) | integer | `0` | This input can reduce harmonic complexity by disabling some of the scale or non-scale notes. It is an idea first found in the Sinfonion and also provided by the circuit [`sinfonionlink`](sinfonionlink.md). `harmonicshift` is staged after the `select...` inputs and further filters out (disables) notes based on their relation to the current scale. This means that first the 12 `select...` inputs select a subset of the 12 possible notes. After that `harmonicshift` can reduce this set further (it will never add notes). If `harmonicshift` is not zero, depending on its value some or more of the scale notes are disabled, even if they would be allowed by `select...`. Or in other words: the harmonic material is reduced. You also can use negative values. These create rather strange sounds by removing the simple chord functions instead of the complex ones first. See the value table below. |
| `noteshift` (`nos`) | integer | `0` | Shifts the resulting output note(s) by this number of scale notes up or down (if negative). So the output note still is part of the scale but may be a note that is none of the selected ones. The maximum shift range is limited to -24 … +24. |
| `selectnoteshift` (`sns`) | integer | `0` | Shifts the output note by this number of selected scale notes up or down (if negative). If you use `noteshift` at the same time, first `selectnoteshift` is applied, then `noteshift`. The maximum shift range is limited to -24 … +24. |
| `tuningmode` (`tm`) | gate | `off` | While this is 1, the circuit will output the value set by `tuningpitch` instead of the actual pitch. This is meant to be a help for tuning your VCOs. |
| `tuningpitch` (`tp`) | pitch | `0V` | This pitch CV will be output while the tuning mode is active. |
| `transpose` (`tr`) | pitch | `0V` | This value is being added to the output pitch when not in tuning mode. It can be used for musical transposition or adding a vibrato. |

### Root note table (`root`)

| Value | Note |
|-------|------|
| 0 | C |
| 1 | C♯ |
| 2 | D |
| 3 | D♯ |
| 4 | E |
| 5 | F |
| 6 | F♯ |
| 7 | G |
| 8 | G♯ |
| 9 | A |
| 10 | A♯ |
| 11 | B |
| 12 | C |

### Scale table (`degree`)

The first 12 and most important scales. Altogether there are 108 scales; please
see the manual ([scales](../scales.md)) for a complete list.

| Value | Scale |
|-------|-------|
| 0 | lyd – Lydian major scale (it has a ♯4) |
| 1 | maj – Normal major scale (ionian) |
| 2 | X7 – Mixolydian (dominant seven chords) |
| 3 | sus – mixolydian with 3rd/4th swapped |
| 4 | alt – Altered scale |
| 5 | hm5 – Harmonic minor scale from the 5th |
| 6 | dor – Dorian minor (minor with ♯13) |
| 7 | min – Natural minor (aeolian) |
| 8 | hm – Harmonic minor (♭6 but ♯7) |
| 9 | phr – Phrygian minor scale (with ♭9) |
| 10 | dim – Diminished scale (whole/half tone) |
| 11 | aug – Augmented scale (just whole tones) |

### Harmonic shift table (`harmonicshift`)

| Value | Effect |
|-------|--------|
| 0 | off – all selected notes are allowed |
| 1 | disable all fill notes (non-scale notes) |
| 2 | disable fills and 11th |
| 3 | disable fills, 11th and 13th |
| 4 | disable fills, 11th, 13th and 9th |
| 5 | disable fills, 11th, 13th, 9th and 7th |
| 6 | disable fills, 11th, 13th, 9th, 7th and 3rd |
| 7 | disable fills, 11th, 13th, 9th, 7th, 3rd and 5th |
| -1 | disable the root note |
| -2 | disable the root note and the 5th |
| -3 | disable root, 3rd, 5th |
| -4 | disable root, 3rd, 5th, 7th |
| -5 | disable root, 3rd, 5th, 7th, 9th |
| -6 | disable root, 3rd, 5th, 7th, 9th and 13th |
| -7 | disable all scale notes (fill notes untouched) |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output1 … output4` (`o`) | pitch | 1st … 4th pitch output. |

## See also

- [`button`](button.md) — toggle buttons make a natural live playing interface for the `select...` inputs.
- [`sinfonionlink`](sinfonionlink.md) — shares the harmonic material (including `harmonicshift`) with a Sinfonion.
