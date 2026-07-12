---
circuit: detune
title: Detune multiple voices in a most disharmonic way
obsolete: false
ram_bytes: 56
manual_pages: [198]
category: quantizer-pitch
tags: [detune, disharmonic, voices, pitch, chord, sinfonion, tuning]
see_also: [sinfonionlink]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: true
difficulty_note: Per-voice detune plus a tuning-mode passthrough is simple to wire, but the actual detune curve is the undocumented part.
verification_note: "Headless once the algorithm is known: apply a chord and detune amount and compare the eight offset outputs. Blocked — the manual only states the algorithm is 'identical with the Sinfonion' and gives no formula, so reference data is needed to reproduce and verify the disharmonic offsets."
---

# detune — Detune multiple voices in a most disharmonic way

Sometimes braking the harmony – at least for some period of time – can be a way
to break through monotony. This circuit allows you to detune up to eight voices
in a most disharmonic way.

The application is simple. Before outputting your pitch information from your
sequencers, quantizers, chord generators, arpeggiators and friends, feed them
through this circuit. Then use the CV input `detune` to select the level of ugly
detuning.

For a first test, start with using a pot for the detune value:

```droid
[detune]
    detune = P1.1
    input1 = _PITCH1 # from somewhere
    input2 = _PITCH2 # from somewhere
    input3 = _PITCH3 # from somewhere
    input4 = _PITCH4 # from somewhere
    input5 = _PITCH5 # from somewhere
    output1 = O1
    output2 = O2
    output3 = O3
    output4 = O4
    output5 = O5
```

The crux of the matter is that all of the voices are detuned in realation to
each other. This circuit makes sure that each of the eight outputs is detuned in
a different way, so if you input a chord, it won't just simply move up and down
in pitch but gets distorted and disharmonic.

Hints:

- Try using a slow and slight bipolar triangle LFO.
- Try using an envelope with a short attack and a longer release to detune at
  the start of every bar and let the voices fade back into tune.

This detune algorithm is identical with that of the Sinfonion. Therefore it is a
good match for the `chaoticdetune` output of the circuit
[`sinfonionlink`](sinfonionlink.md).

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input1 … input8` (`i`) | CV | | The pitch inputs of the up to eight voices to be detuned. |
| `detune` (`d`) | CV | `0.0` | Selects the level of detuning. |
| `tuningmode` (`tm`) | gate | `0` | While this is `1`, the circuit will output the value set by `tuningpitch` instead of the actual pitch. This is ment to be a help for tuning your VCOs. |
| `tuningpitch` (`tp`) | pitch | `0V` | This pitch CV will be output while the tuning mode is active. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output1 … output8` (`o`) | CV | The detuned pitch outputs of the up to eight voices. |

## See also

- [`sinfonionlink`](sinfonionlink.md) — its `chaoticdetune` output is a good
  match for the `detune` input.
