---
circuit: minifonion
title: Musical quantizer
obsolete: false
ram_bytes: 112
manual_pages: [308, 309, 310, 311, 312, 313]
category: quantizer-pitch
tags: [quantizer, scale, pitch, 1v/oct, root, degree, sinfonion, harmonic-shift, transpose, chord, note-selection]
see_also: [arpeggio, chord, sinfonionlink]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Pure pitch math, but note selection, harmonicshift, noteshift/selectnoteshift and the 108 scales make for a lot of documented cases to get exactly right.
verification_note: "All 108 scales are tabulated in manual/scales.md, so quantized output for any input pitch, root, degree, select gates and shift can be checked headless against a computed reference."
---

# minifonion — Musical quantizer

This circuit is a very musical quantizer that gently moves any input CV (pitch
information on a 1V/oct base) into selected notes of a musical scale. Typically
the input CV is coming from a random source, LFO, melody generator or sequencer.

In fact the Minifonion is very similar to each of the three quantizer channels
in the Audiophile Circuit League Sinfonion – just without the user interface and
more flexible. It has Sinfonion compatible CVs for the root note and the scale
selection so it can easily be combined with it as long as you control the
Sinfonion via CV and stick to the first mode. But of course you do not need a
Sinfonion in order to use this circuit!

If you want to mimick a Sinfonion with the DROID you might also be interested in
the circuits [`arpeggio`](arpeggio.md) and [`chord`](chord.md).

## Example

Here is the simplest possible application – a quantization of some (random)
input pitch at `I1` to the seven notes of a C lydian major scale.

```droid
[minifonion]
    input = I1
    output = O2
```

Now let's change the root note to D (2 semitones above C) and the scale to
natural minor, so that we now quantize to a D minor scale:

```droid
[minifonion]
    input = I1
    output = O2
    root   = 2
    degree = 7
```

## Scales

Here is the table of the first 12 scales of the Minifonion. These are exactly
the same scales as those in the first mode (called *Chords*) of the Sinfonion:

| degree | Abbr. | Scale |
|--------|-------|-------|
| 0 | lyd | Lydian major scale (it has a ♯4) |
| 1 | maj | Normal major scale (ionian) |
| 2 | X7 | Mixolydian (dominant seven chords) |
| 3 | sus | mixolydian with 3rd/4th swapped |
| 4 | alt | Altered scale |
| 5 | hm5 | Harmonic minor scale from the 5th |
| 6 | dor | Dorian minor (minor with ♯13) |
| 7 | min | Natural minor (aeolian) |
| 8 | hm | Harmonic minor (♭6 but ♯7) |
| 9 | phr | Phrygian minor scale (with ♭9) |
| 10 | dim | Diminished scale (whole/half tone) |
| 11 | aug | Augmented scale (just whole tones) |

You find the complete table of all 108 scales in the manual
([scales](../scales.md)).

If you are a Sinfonion user, please note that the inputs `root` and `degree` of
the Minifonion are *not* based on semitones like the Sinfonion, but simply
expect whole numbers like 0, 1, 2 and so on (which corresponds to the CVs 0 V,
10 V, 20 V, etc.). So if you want those CV inputs to be compatible, you have to
multiply the values with the factor of 120 before sending them to the Minifonion:

```droid
[minifonion]
    input  = I1
    output = O2
    root   = I2 * 120 # base on semitones
    degree = I3 * 120 # base on semitones
```

## How the quantization works

The quantization is done in two steps. First the input pitch is quantized to the
*nearest semitone*. This means that the input voltage is slightly shifted up or
down to the next 1⁄12 V position.

If you have set all twelve `select…` inputs to `1` – which basically means
"quantize to a chromatic scale", we are done here. The resulting voltage is
output.

If not, there is a chance that the semitone is not part of the allowed scale and
note selection. In this case `minifonion` looks for the next semitone that's
allowed. You can specify the direction of the search with the input `direction`.
There are three possible values:

- `direction = 0` looks for the *nearest* allowed semitone.
- `direction = 1` looks *upwards* for the next allowed semitone. This is the
  *default*.
- `direction = 2` looks *downwards* for the next allowed semitone.

In this search it never goes out of the range -10 V … +10 V. If there is no
matching semitone before this border is reached, the voltage of the border is
output (even if this semitone is not part of your note selection).

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | pitch | `0V` | Patch the unquantized input voltage here. |
| `trigger` (`t`) | trigger | | This jack is optional. If you patch it, the Minifonion will work in triggered mode. Here the output pitch is always frozen until the next trigger happens. |
| `bypass` (`b`) | gate | `off` | If you set this gate input to `1` then quantization is bypassed and the input voltage is directly copied to the output. |
| `direction` (`d`) | integer | `1` | Set the direction in which to search for the next allowed semitone in the current scale and note selection: `0` search for the nearest semitone, `1` search upwards, `2` search downwards. |
| `root` (`ro`) | integer | `0` | Set the root note here. `0` means C, `1` means C♯, `2` means D and so on. If you multiply the value of an input like `I1` with 120, then you can use a 1V/Oct input for selecting the root note via a sequencer, MIDI keyboard or the like. Also then you are compatible with the ROOT CV input of the Sinfonion. Values: `0` C, `1` C♯, `2` D, `3` D♯, `4` E, `5` F, `6` F♯, `7` G, `8` G♯, `9` A, `10` A♯, `11` B, `12` C. |
| `degree` (`dg`) | integer | `0` | Set the musical scale. This is a number from `0` to `107`. See the Scales table above for the first 12 and most important scales; the complete list of all 108 scales is in the manual ([scales](../scales.md)). |
| `select1` (`s1`) | gate | ☞ smart | Gate input for selecting the *root* note as being an allowed interval. When you want to create a playing interface for live operation you can patch the output of a toggle button (made with the circuit [`button`](button.md)) here. When all `select` and `selectfill` inputs are `0`, automatically all seven scale notes are selected, i.e. `select1 … select13` will be set to one. |
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
| `harmonicshift` (`has`) | integer | `0` | This input can reduce harmonic complexity by disabling some of the scale or non-scale notes. It is an idea first found in the Sinfonion and also provided by the circuit [`sinfonionlink`](sinfonionlink.md). `harmonicshift` is staged after the `select…` inputs and further filters out (disables) notes based on their relation to the current scale: first the 12 `select…` inputs select a subset of the 12 possible notes, then `harmonicshift` can reduce this set further (it will never add notes). Negative values create rather strange sounds by removing the *simple* chord functions instead of the complex ones first. See the Harmonic shift table below. |
| `noteshift` (`nos`) | integer | `0` | Shifts the resulting output note(s) by this number of *scale* notes up or down (if negative). So the output note still is part of the scale but may be a note that is none of the selected ones. The maximum shift range is limited to -24 … +24. |
| `selectnoteshift` (`sns`) | integer | `0` | Shifts the output note by this number of *selected* scale notes up or down (if negative). If you use `noteshift` at the same time, first `selectnoteshift` is applied, then `noteshift`. The maximum shift range is limited to -24 … +24. |
| `tuningmode` (`tm`) | gate | `off` | While this is `1`, the circuit will output the value set by `tuningpitch` instead of the actual pitch. This is meant to be a help for tuning your VCOs. |
| `tuningpitch` (`tp`) | pitch | `0V` | This pitch CV will be output while the tuning mode is active. |
| `transpose` (`tr`) | pitch | `0V` | This value is being added to the output pitch when not in tuning mode. It can be used for musical transposition or adding a vibrato. |

### Harmonic shift values

| Value | Effect |
|-------|--------|
| `0` | off – all selected notes are allowed |
| `1` | disable all fill notes (non-scale notes) |
| `2` | disable fills and 11th |
| `3` | disable fills, 11th and 13th |
| `4` | disable fills, 11th, 13th and 9th |
| `5` | disable fills, 11th, 13th, 9th and 7th |
| `6` | disable fills, 11th, 13th, 9th, 7th and 3rd |
| `7` | disable fills, 11th, 13th, 9th, 7th, 3rd and 5th |
| `-1` | disable the root note |
| `-2` | disable the root note and the 5th |
| `-3` | disable root, 3rd, 5th |
| `-4` | disable root, 3rd, 5th, 7th |
| `-5` | disable root, 3rd, 5th, 7th, 9th |
| `-6` | disable root, 3rd, 5th, 7th, 9th and 13th |
| `-7` | disable all scale notes (fill notes untouched) |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | pitch | Here comes your quantized output voltage. |
| `notechange` (`n`) | trigger | Whenever the quantization changes to a new note a trigger with the duration 10 ms is output here. No trigger is output in bypass mode. |

## See also

- [`arpeggio`](arpeggio.md) — arpeggiator, useful for mimicking a Sinfonion.
- [`chord`](chord.md) — chord generator, useful for mimicking a Sinfonion.
- [`sinfonionlink`](sinfonionlink.md) — provides the same harmonic-shift idea.
