---
circuit: polytool
title: Change number of voices in polyphonic setups
obsolete: false
ram_bytes: 240
manual_pages: [360, 361]
category: switch-routing
tags: [polyphony, voices, voice-allocation, roundrobin, midi, cv-gate, transformer, chord, arpeggio]
see_also: [midiin, arpeggio]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Voice-allocation state machine (up to 16 in/out voices, roundrobin, oldest/newest/lowest/highest stealing) — real breadth and note-tracking state, but fully specified and maps onto Rack polyphony.
verification_note: "Headless: feed scripted pitch/gate voice streams and assert output-voice assignment against each voiceallocation value and the roundrobin rule."
---

# polytool — Change number of voices in polyphonic setups

The polytool is an intelligent "transformer" that can map melodies with *N*
parallel notes to synth voices with *M* parallel voices and can thus change the
polyphony of a melody.

This functionality is inspired by MIDI to CV interfaces (such as
[`midiin`](midiin.md)), which need to deal with the almost unlimited possible
polyphony of MIDI, where 127 parallel notes are possible and where the interface
needs to suffice with a fixed limited number of CV/gate outputs.

The usage is very simple: patch your input voices (CV/gate pairs) into
`pitchinputX` and `gateinputX`. And patch your output voices into `pitchoutputX`
and `gateoutputX`.

Here is an example for converting a three-fold polyphony into a single voice.
That voice is controlled by `O1` and `O2`:

```droid
[polytool]
    pitchinput1 = _PITCH_1
    pitchinput2 = _PITCH_2
    pitchinput3 = _PITCH_3
    gateinput1 = _GATE_1
    gateinput2 = _GATE_2
    gateinput3 = _GATE_3
    pitchoutput1 = O1
    gateoutput1 = O2
```

See how the parameter `voiceallocation` determines, which note should be played
if there is more than one at a time.

The polytool can also do the opposite: You input a serial melody with just one
note at a time and have that mapped to multiple output voices that make the
actual audible sound of the notes overlap. This can even be used to convert fast
short arpeggios into chord pads.

The next example shows this. At `pitchinput1` and `gateinput1` there is a melody,
for example from a sequencer or from the circuit [`arpeggio`](arpeggio.md). That
is then played on two output voices:

```droid
[polytool]
    pitchinput1 = _PITCH
    gateinput1 = _GATE
    pitchoutput1 = O1
    pitchoutput2 = O2
    gateoutput1 = O3
    gateoutput2 = O4
```

Here the parameter `roundrobin` decides how the notes will be distributed onto
the two output voices.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `pitchinput1 … pitchinput16` (`pi`) | pitch | | The pitches of up to 16 input voices. |
| `gateinput1 … gateinput16` (`gi`) | gate | | The gates of up to 16 input voices. |
| `roundrobin` (`rr`) | gate | `0` | Normally when looking for a free output for playing the next note, this circuit will start from `pitchoutput1` in its search. This way, if there are not more notes than outputs at any time, the notes played first will always be played at the lowest numbered outputs. This leads to a deterministic behaviour when it comes to playing things like chords. The same voice will always be used for the first note in the stream of MIDI events. When you switch `roundrobin` to 1, this changes. Now the outputs are scanned in a round-robin fashion, like in a rotating switch. That way every output has the same chance to get a new note. Here it can even make sense to define multiple voices even if the track is monophonic. When you use envelopes with longer release times, you can transform such a melody into chords with simultaneous notes. Note: When all outputs are currently used by a note, `roundrobin` has no influence. Here `voiceallocation` selects which of the notes will be dropped. |
| `voiceallocation` (`va`) | integer | `0` | When from the pitch inputs, at any given time, more voices are active than you have outputs assigned, normally the "oldest" notes would be cancelled. This behaviour can be configured here by setting `voiceallocation` to one of the following values: `0` – the oldest note will be cancelled (default); `1` – the new note will not be played and simply be omitted; `2` – the lowest note will be cancelled; `3` – the highest note will be cancelled. |

### voiceallocation values

| Value | Behaviour |
|-------|-----------|
| `0` | The oldest note will be cancelled (default) |
| `1` | The new note will not be played and simply be omitted |
| `2` | The lowest note will be cancelled |
| `3` | The highest note will be cancelled |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `pitchoutput1 … pitchoutput16` (`po`) | pitch | The pitches of up to 16 output voices. |
| `gateoutput1 … gateoutput16` (`go`) | gate | The gates of up to 16 output voices. |

## See also

- [`midiin`](midiin.md) — MIDI to CV interface whose voice allocation inspired
  this circuit.
- [`arpeggio`](arpeggio.md) — a source of serial melodies to re-spread across
  voices.
