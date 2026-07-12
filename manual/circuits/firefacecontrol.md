---
circuit: firefacecontrol
title: Control a RME Fireface interface (experimental)
obsolete: false
ram_bytes: 1088
manual_pages: [256, 257, 258]
category: midi
tags: [rme, fireface, audio interface, x7, midi, mackie control, mixer, panning, mute, motor fader]
see_also: [motorfader]
impl_difficulty: not-feasible
controller_binding: x7-required
difficulty_note: Controls an external RME Fireface audio interface over one-way Mackie-Control MIDI (needs an X7); no such device exists in VCV Rack and the protocol cannot read device state, so there is no meaningful in-Rack target to emulate.
---

# firefacecontrol — Control a RME Fireface interface (experimental)

This experimental circuit allows you to control the most important volumes and
mixes of an RME Fireface audio interface. It's also a perfect match for the M4
motor fader units. You need an X7 in order to use this circuit.

Please note that this circuit is still experimental. Its main problem is that the
MIDI implementation of the Fireface is more designed for user interaction via a
Mackie Control and not for general automation. This is very sad.

For example there is a MIDI CC for changing the panning of a channel. But instead
of simply having the CC going from 0 (left) via 64 (mid) to 127 (right), it uses
various CC values as *commands* for modifying the existing panning by some fixed
value to the left or to the right. So without knowing the current setting, it's
not possible to send the correct CC commands. And for DROID there is no way to
know, since MIDI is a one way communication.

RME: if you are reading this: please contact me so that we can fix this.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `trs` | integer | `1` | |
| `outputlevel1 … outputlevel16` (`ol`) | 0..1 | | Output levels. |
| `mainoutput` (`mo`) | integer | `1` | |
| `phonesoutput1, phonesoutput2` (`po`) | integer | | |
| `outputmix1in1 … outputmix1in16` (`o1i`) | 0..1 | | |
| `outputmix2in1 … outputmix2in16` (`o2i`) | 0..1 | | |
| `outputmix3in1 … outputmix3in16` (`o3i`) | 0..1 | | |
| `outputmix4in1 … outputmix4in16` (`o4i`) | 0..1 | | |
| `outputmix5in1 … outputmix5in16` (`o5i`) | 0..1 | | |
| `outputmix6in1 … outputmix6in16` (`o6i`) | 0..1 | | |
| `outputmix7in1 … outputmix7in16` (`o7i`) | 0..1 | | |
| `outputmix8in1 … outputmix8in16` (`o8i`) | 0..1 | | |
| `outputmix9in1 … outputmix9in16` (`o9i`) | 0..1 | | |
| `outputmix10in1 … outputmix10in16` (`o10i`) | 0..1 | | |
| `outputmix11in1 … outputmix11in16` (`o11i`) | 0..1 | | |
| `outputmix12in1 … outputmix12in16` (`o12i`) | 0..1 | | |
| `outputmix13in1 … outputmix13in16` (`o13i`) | 0..1 | | |
| `outputmix14in1 … outputmix14in16` (`o14i`) | 0..1 | | |
| `outputmix15in1 … outputmix15in16` (`o15i`) | 0..1 | | |
| `outputmix16in1 … outputmix16in16` (`o16i`) | 0..1 | | |
| `postfader1 … postfader16` (`pf`) | gate | | |
| `pan1 … pan16` (`p`) | 0..1 | | |
| `unmute1 … unmute16` (`u`) | 0..1 | | |
| `update` (`ud`) | trigger | | |
| `select` (`s`) | integer | ☞ smart | The select input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to 1). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | ☞ smart | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is 0, the circuit will be active if `select` is exactly 0 instead of a positive gate signal. In some cases this is more convenient. |
