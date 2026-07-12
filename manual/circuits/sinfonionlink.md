---
circuit: sinfonionlink
title: Sync harmonic state from Sinfonion
obsolete: false
ram_bytes: 56
manual_pages: [386, 387]
category: chord-harmony
tags: [sinfonion, harmonic-sync, root, degree, scale, transpose, chaotic-detune, harmonic-shift, clock, acl, master18]
see_also: [detune, minifonion, chord, arpeggio, motoquencer]
impl_difficulty: not-feasible
controller_binding: master-only
difficulty_note: Decodes a proprietary ACL Sinfonion "Harmonic Sync" signal received on MASTER18 input I1; there is no Sinfonion in VCV to emit that signal and its on-cable encoding is undocumented, so there is nothing to receive and no reference to reproduce.
---

# sinfonionlink — Sync harmonic state from Sinfonion

This circuit allows you to sync the current harmonic state of a Sinfonion to
your MASTER18. This allows you to keep your DROID in sync with the current
settings of root, degree, mode, transpose, chaotic detune and harmonic shift of
the Sinfonion. It also gives you access to the Sinfonion's clock as well as
triggers for the start of a sequence, bar or beat.

The ACL Sinfonion has a feature called *Harmonic Sync*. It is used to connect
two or more Sinfonions and share the current harmonic state, so that all
Sinfonions and all voices of your modular play in relation to the same root note
and scale all the time. This sync is using a plain mono patch cable and you can
even use a long cable to sync with your band mate.

The input `I1` of the MASTER18 is able to receive and intereprete this
information and present it as outputs of this circuit. This is how to set it up:

1. On your Sinfonion, set *Out 1* to *Sync Master*.
2. Use a normal patch cable to connect the Sinfonion's *OUT 1* jack to the `I1`
   jack of the MASTER18.
3. Add a `sinfonionlink` circuit to your patch.
4. Use the outputs of this circuit to provide your other circuits with the
   information about the current harmonic situation such as the root note and the
   scale.

This is a basic example for using a Droid to add one more quantizer to a
Sinfonion:

```droid
[sinfonionlink]
    root = _ROOT
    degree = _DEGREE

[minifonion]
    input = I1
    root = _ROOT
    degree = _DEGREE
```

Notes:

- The MASTER18 can only receive the sync, not send it (be a sync master). But it
  can remote control the Sinfonion via various CVs, which needs more cables and CV
  outputs but covers the same functionality.
- Harmonic Sync allows for a 1:n communication. You can use a passive mult to
  distribute the signal to as many MASTER18 s and Sinfonions as you like.
- The MASTER does not have this circuit.

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `root` (`ro`) | integer | The current root note as an integer number. C = 0, C♯ = 1, D = 2 and so on. |
| `degree` (`dg`) | integer | The current scale (the Sinfonion uses the word degree for this). This is an integer number. You find a list of all available scales in the manual ([scales](../scales.md)). |
| `transpose` (`tr`) | pitch | The current global transposition of the Sinfonion. This is in 1V/Oct, so you can add it to your pitch whereever you output one. |
| `chaoticdetune` (`ch`) | CV | The current value of the chaotic detune. You can feed this into the `detune` input of the circuit [`detune`](detune.md). |
| `harmonicshift` (`has`) | integer | Harmonic shift is a feature of the Sinfonion that allows to reduce harmonic complexity via CV (or the builtin pots POT1 or POT2). The idea is that the more you rise the CV, the less complex scale notes are allowed. This output gives you access to the current setting of harmonic shift of the Sinfonion. It is an integer number between −7 and 7. You can directly feed it into the `harmonicshift` input of circuits like [`minifonion`](minifonion.md), [`chord`](chord.md), [`arpeggio`](arpeggio.md) or [`motoquencer`](motoquencer.md). Harmonic shift is explained in detail in the manual chapter of `minifonion`. |
| `linkstate` (`ls`) | gate | Outputs 1 if the link to the Sinfonion is up and active, otherwise 0. |
| `clock` (`c`) | gate | Gives you a copy of the Sinfonion's clock input. |
| `reset` (`r`) | trigger | Outputs a trigger whenever in Song mode the Sinfonion forwards to the first bar of the song. |
| `step` (`s`) | trigger | Outputs a trigger for every step of a song. |
| `beat` (`b`) | trigger | Outputs a trigger for every beat (subdivision of a step). |

## See also

- [`minifonion`](minifonion.md), [`chord`](chord.md), [`arpeggio`](arpeggio.md),
  [`motoquencer`](motoquencer.md) — accept the `harmonicshift` output.
- [`detune`](detune.md) — accepts the `chaoticdetune` output.
