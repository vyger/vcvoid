---
circuit: midihirescc
title: 14-bit (hi-res) MIDI CC output
obsolete: false
experimental: true
ram_bytes: 264
manual_pages: []
category: midi
tags: [midi, cc, 14-bit, hi-res, high resolution, controller, msb, lsb, usb, ntx-8cv, expert sleepers, atomic, experimental]
see_also: [midiout, midiin, midithrough]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Change detection and rate limiting are midiout's contUpdate contract; the only new mechanics are the 14-bit split and emitting both bytes from one update.
verification_note: "Headless: assert pair adjacency with two slots moving on one tick, both byte orders, change detection under updaterate, the 0..1 -> 0..16383 mapping with clamping, USB port selection, the silent no-op without MIDI hardware, and that nothing is sent until a value moves."
---

# midihirescc — 14-bit (hi-res) MIDI CC output

> **⚠️ EXPERIMENTAL — vcvoid only.**
> `midihirescc` is **not** a DROID circuit. It does not exist in any firmware,
> the Droid Forge does not know it, and a patch using it **will not run on DROID
> hardware**. vcvoid refuses to load such a patch until you enable
> **"Allow experimental circuits"** in the master module's context menu.
> See [the experimental circuits index](index.md).

An ordinary MIDI continuous controller carries 7 bits — 128 steps. That is
audibly coarse for anything that becomes a control voltage at the other end: a
filter sweep steps, a slow fade staircases. MIDI's answer is to send the
controller twice, as a coarse byte on controller *n* and a fine byte on
controller *n + 32*, giving 14 bits and 16384 steps.

[`midiout`](../midiout.md) cannot do this for a controller of your choosing —
its `cc1 … cc8` are 7-bit, and its only hi-res paths are hardwired to `volume`
(CC#7 + CC#39) and `pitchbend`. This circuit sends any hi-res-capable controller
at full resolution, and sends both bytes together.

```droid
[midihirescc]
    usb       = 1
    channel   = 1
    hirescc22 = _FILTER
```

That sends `_FILTER` as CC#22 (coarse) plus CC#54 (fine) on channel 1, out of
the first USB port.

## The jack number is the controller number

There is no `ccnumber` parameter. The number in the jack name *is* the MIDI
controller number of the coarse byte, and the fine byte is always that number
plus 32:

| Jack | Coarse (MSB) | Fine (LSB) |
|------|--------------|------------|
| `hirescc1` | CC#1 | CC#33 |
| `hirescc22` | CC#22 | CC#54 |
| `hirescc31` | CC#31 | CC#63 |

MIDI defines this pairing for controllers 0 to 31 only: 32 to 63 *are* the fine
half, and every controller above 63 is single-byte. Naming the jacks this way
means a controller that cannot be sent at 14 bits is one you cannot write down —
there is no silent fall back to 7 bits, because there is nothing to fall back
from.

The available range is `hirescc1 … hirescc31`, so **every** controller that has
a 14-bit form is reachable except CC#0, which is Bank Select and would be read
as a bank change rather than a value.

Two of the jacks are legal but loaded, and worth avoiding unless you mean them:

- **`hirescc6`** — CC#6/38 is **Data Entry**, the payload half of RPN and NRPN
  messages. Outside a parameter-select context most devices will ignore it or do
  something surprising.
- **`hirescc7`** — CC#7/39 is **Volume**, which [`midiout`](../midiout.md)
  already sends as a 14-bit pair through its own `volume` jack. Two circuits
  driving it in one patch will fight.

## Both bytes, one update

Both bytes of a pair are emitted **from the same update**, back to back. They
cannot be split across ticks and no other controller can appear between them.

This matters because many receivers keep a *single* pending fine-byte latch
rather than one per controller. Interleave two controllers' bytes and the
receiver pairs the wrong halves. Doing the split in the patch instead — with
`math` circuits and `midiout` — cannot guarantee adjacency structurally; it
depends on getting the slot ordering right by hand, and caps a `midiout` circuit
at four hi-res destinations instead of eight. See
[`patches/lfo-cc-midi.ini`](../../../patches/lfo-cc-midi.ini) for what that
looks like, and
[`patches/lfo-cc-midi-hires.ini`](../../../patches/lfo-cc-midi-hires.ini) for
the same patch written with this circuit.

Both bytes are always sent, even when only the fine byte changed. Sending the
fine byte alone would halve the traffic for small movements, but a receiver that
applies the value when the coarse byte arrives would sit on a stale reading
until something else moved it.

### Byte order

`lsbfirst` sets the order within the pair, for the whole circuit:

- **`lsbfirst = 1` (default)** — fine byte, then coarse. A receiver with a
  pending-fine latch applies the complete 14-bit value the moment the coarse
  byte lands. This is what the Expert Sleepers NTX-8CV expects in its default
  configuration.
- **`lsbfirst = 0`** — coarse, then fine. The MIDI specification's recommended
  order; use it for receivers that follow the spec literally.

If you need both orders at once, use two circuits.

## Only what changed, only as often as needed

A controller is sent **when its 14-bit value actually changes** — an input that
is not moving costs nothing on the wire. `updaterate` caps how often a moving
one may be resent, in updates per second and per controller. The default is 500,
which is smooth for audio-rate-adjacent modulation without saturating a USB
connection; `updaterate = 0` stops updates entirely.

Note what this replaces: without change detection, a patch has to resend every
controller unconditionally on a clock, which is why the hand-rolled version
needs a ~500 Hz trigger feeding all sixteen CC slots.

**There is no initial send.** The circuit notes the starting value silently and
transmits only once that value moves. A patch whose inputs never move sends
nothing at all, and the receiver keeps whatever it had. If you need a known
starting state, move the value.

## USB only

There is no `trs` parameter. A classical MIDI cable carries 3125 bytes per
second; one 14-bit update is 6 bytes, so a DIN connection could sustain only a
few hundred updates per second in total across everything sharing the port. At
the rates this circuit is meant for, MIDI over USB is the only transport that
keeps up, so it is the only one offered.

`usb` selects which USB port, counting the master's own ports first and the X7's
after: `1` is the first USB port, `10` means all of them, `0` means none. With
no MIDI hardware in the chain at all (a MASTER with no X7), the circuit is a
silent no-op and the patch still loads — the same as [`midiout`](../midiout.md).

### How much can you send?

One update is 6 bytes. Eight controllers moving continuously at the default 500
updates per second is 24 kB/s, which USB handles comfortably. All 31 at that
rate is 93 kB/s, which is a lot of MIDI for a receiver to parse; if a receiver
starts lagging, lower `updaterate` before reducing the number of controllers —
resolution in *time* is usually what you can spare.

## Memory

`midihirescc` costs 264 bytes plus the usual per-parameter cost. That base is
charged whether you use one controller or all 31: the per-controller state (last
value sent and when) is a fixed-size table.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `hirescc1 … hirescc31` (`h`) | 0..1 | | A 14-bit controller. The jack number is the MIDI controller number of the coarse byte; the fine byte is that number plus 32. The value 0.0 … 1.0 is resolved to 0 … 16383 internally and clamped, so an input that overshoots saturates rather than wrapping. CC#0 has no jack, and CC#6 / CC#7 mean something specific to most receivers — see above. |
| `usb` (`u`) | integer | `1` | Which USB port to send on: 1 … n counts USB ports with the master's own first and the X7's after, 10 means all USB ports, 0 means none. There is no TRS output. |
| `channel` (`ch`) | integer | `1` | MIDI channel, 1 … 16. Both bytes of every pair are sent on this channel. |
| `lsbfirst` (`lf`) | gate | `1` | Byte order within a pair. 1 sends the fine byte first (what a receiver with a pending-fine latch needs); 0 sends the coarse byte first, per the MIDI specification. |
| `updaterate` (`ur`) | CV | `500` | Maximum updates per second, per controller. Controllers are sent only when their value changes; this caps how often a moving one is resent. Zero or negative stops all updates. |

## Outputs

This circuit has no outputs — all output is done via MIDI.

## See also

- [`midiout`](../midiout.md) — the real DROID MIDI output circuit: notes, clock,
  transport, program changes and 7-bit CCs. Use it for everything except hi-res
  controllers, and note that its `volume` is already a 14-bit CC#7/CC#39 pair.
- [`midiin`](../midiin.md) — MIDI to CV.
- [`midithrough`](../midithrough.md) — forward MIDI from input to output.
