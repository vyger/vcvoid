---
circuit: midithrough
title: Forward MIDI events from input to one or more outputs
obsolete: false
ram_bytes: 240
manual_pages: [306, 307]
category: midi
tags: [midi, forward, route, merge, trs, usb, x7, master18, auto-detect, broadcast, port]
see_also: [midiin, midiout]
impl_difficulty: easy
controller_binding: x7-required
verification: rack-automated
spec_gap: false
difficulty_note: Requires the X7; logic is just port-to-port forwarding/merging with auto-detect and broadcast, minus Sysex.
verification_note: "Send MIDI into a source port in Rack and confirm it appears on the selected target port(s); auto-detect (10) and broadcast (10) each need a real MIDI stream to exercise."
---

# midithrough — Forward MIDI events from input to one or more outputs

Use this circuit to forward MIDI data from input ports to output ports. You can
get events from one TRS and one USB port and forward to multiple TRS and
multiple USB ports (if you have an X7).

You specify the source and target of the events with the four parameters
`fromusb`, `fromtrs`, `tousb` and `totrs`.

The number of USB and TRS (3.5 mm tip ring sleeve jack) ports depends on your
hardware configuration. Here is how your ports are numbered:

### MASTER + X7

| Value | Meaning |
|-------|---------|
| `usb = 1` | USB port on the X7 |
| `trs = 1` | TRS port on the X7 |

### MASTER18

| Value | Meaning |
|-------|---------|
| `usb = 1` | USB port on the MASTER18 |
| `trs = 1` | TRS port MIDI1 on the MASTER18 |
| `trs = 2` | TRS port MIDI2 on the MASTER18 |

### MASTER18 + X7

| Value | Meaning |
|-------|---------|
| `usb = 1` | USB port on the MASTER18 |
| `usb = 2` | USB port on the X7 |
| `trs = 1` | TRS port MIDI1 on the MASTER18 |
| `trs = 2` | TRS port MIDI2 on the MASTER18 |
| `trs = 3` | TRS port on the X7 |

In addition there is port auto detection and broadcast. If you specify
`fromusb = 10` or `fromtrs = 10`, the first port that actually sends events is
used as input – until the data stops for a second or more. If this is the case,
auto detection is redone and another port might be the lucky one.

Setting `tousb = 10` or `totrs = 10` forwards the events to all ports of the
given type.

## Examples

Forward MIDI events from the first USB port to the second TRS port:

```droid
[midithrough]
    fromusb = 1
    totrs = 2
```

Forward MIDI events from the first active TRS port to the first TRS output:

```droid
[midithrough]
    fromtrs = 10 # auto detect
    totrs = 1
```

Forward MIDI events from the second TRS port to all USB and TRS ports:

```droid
[midithrough]
    fromtrs = 1
    totrs = 10 # all TRS ports
    tousb = 10 # all USB ports
```

Note: All [`midiin`](midiin.md), [`midiout`](midiout.md) and other `midithrough`
circuits still work. When multiple circuits send events to the same port, the
events are merged – as long as the output speed of MIDI allows for all the
events.

By using `midiout` and `midithrough` with the same output port, you can thus
"splice in" MIDI events to an existing stream.

Notes:

- As of now, Sysex messages are not forwarded. Sorry for that. If that's
  becoming important we might add this feature.
- If you forward from USB to TRS make sure that you do not send more than 3125
  bytes per second. TRS cannot output faster. It's limited by the MIDI standard.
  If you send MIDI data faster, some events will get lost.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `fromtrs` (`ft`) | integer | ☞ smart | Selects a TRS port to use as input (3.5 mm jack). `fromtrs = 0` disables TRS, `fromtrs = 10` enables auto detection. See the manual of [`midiin`](midiin.md) for details on port selection. |
| `fromusb` (`fu`) | integer | ☞ smart | Selects a USB port to use as input. `fromusb = 0` disables USB, `fromusb = 10` enables auto detection. See the manual of [`midiin`](midiin.md) for details on port selection. |
| `totrs` (`tt`) | integer | ☞ smart | Selects which TRS MIDI port to output to. See the manual of [`midiout`](midiout.md) for details on port selection. |
| `tousb` (`tu`) | integer | ☞ smart | Selects which USB MIDI port to output to. See the manual of [`midiout`](midiout.md) for details on port selection. |

## Outputs

This circuit has no outputs — all output is done via MIDI.

## See also

- [`midiin`](midiin.md) — convert incoming MIDI into CV and gates.
- [`midiout`](midiout.md) — CV to MIDI converter.
