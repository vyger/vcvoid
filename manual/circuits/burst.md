---
circuit: burst
title: Generate burst of pulses
obsolete: false
ram_bytes: 40
manual_pages: [152, 153]
category: clock-timing
tags: [burst, pulses, triggers, clock, taptempo, trigger-delay, ratchet, count, skip]
see_also: [lfo, triggerdelay]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: "Fires a fixed count of 50%-duty pulses at a rate set by hz/rate/taptempo with skip and restart behaviour; all deterministic."
verification_note: "Headless: trigger the burst and compare output pulse count, spacing and gate width against the computed rate; taptempo and restart-on-retrigger are deterministic."
---

# burst — Generate burst of pulses

This circuit produces – when triggered – a number of pulses. It can be used for
solving various musical or technical tasks. Look at this example:

```droid
[burst]
    trigger      = I1
    hz           = 10
    count        = 5
    output       = O1
```

When a trigger arrives at `I1`, the output `O1` will send five triggers in a
row, with a distance of 0.1 seconds (thus 10 Hz). The gate length is fixed to
half of the cycle (thus here 0.05 seconds). This means that the pulse width is
50% – or in other words – the faster the burst the shorter the outgoing
triggers.

Note: When a new trigger arrives while the current burst is still ongoing, it
will not be finished but restarted from the beginning immediately.

If you want the bursts to be synchronized to a musical clock, you can use the
`taptempo` input (here `I2`):

```droid
[burst]
    taptempo = I2
    count    = 4
    trigger = I1
    output   = O1
```

Similar to the circuit [`lfo`](lfo.md), there is a third input for selecting the
speed: `rate`. This works on a 1 V/Oct base, so here is an example for
outputting the bursts at half of the clock speed (-1 V pitches down one octave,
which is the same as half of the speed):

```droid
[burst]
    taptempo = I2
    rate     = -1V
    count    = 4
    trigger = I1
    output   = O1
```

`burst` can also be used for very fast switching through things like presets in
external gear. Here you might want fast updates. Simply set a very high
frequency. Burst makes sure that the actual output rate is limited to the
maximum the DROID hardware can do, so not one single burst can get lost. Also
you might want to use the `skip` input, which skips a certain number of ticks
before starting. This can be used to send out a reset signal to some input and
*after that* sending a couple of skip forward triggers to some other input:

```droid
[burst]
    hz = 5000
    skip = 5
    count = 3
    trigger = I1
    output   = O1
```

Another very simple yet useful application of `burst` is converting a gate
signal into a short trigger. That way you can for example convert a *running*
state from MIDI into a reset trigger. Since `count` defaults to `1`, you don't
need any parameters except the input and output:

```droid
[burst]
    trigger = _MIDI_RUNNING
    output = _RESET
```

In this example the trigger is emitted when the running state goes from 0 to 1.

## Simple clocked trigger delay

Another application of `burst` is a clocked trigger delay. Consider the
following patch:

```droid
[burst]
    taptempo = I1
    trigger = I2
    skip = 7
    output = O1
```

A trigger at `I2` will be delayed by 7 clock cycles.

Note: This simple trigger delay has no memory of more than one trigger. Any
ongoing trigger currently being delayed is overridden and forgotten as soon as
the next trigger arrives. If that is what you want, fine. If you are looking for
a more complex trigger delay, you find one in the circuit
[`triggerdelay`](triggerdelay.md) circuit.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `rate` (`ra`) | CV | `0.0` | Frequency control: The default frequency of the burst rate is 1 Hz (one trigger per second or 60 BPM if you like). Each volt doubles the frequency. So an input of 1 V (a number of `0.1`) speeds up to two triggers per second (120 BPM), 2 V (`0.2`) creates triggers at 4 Hz (240 BPM) and so on. On the other hand negative voltages reduce the speed, so -1 V (`-0.1`) will give 0.5 Hz (30 BPM) and so on. |
| `taptempo` (`tt`) | gate | | Feed a reference clock here and the burst will run at the speed of that clock – albeit optionally modified by `rate`. Please see the manual ([basics](../basics.md)) for details on using taptempo inputs. |
| `hz` (``) | CV | `1.0` | Set the frequency in Hz directly by setting a number here. This is exclusive to `taptempo`, but will work in combination with `rate`. |
| `trigger` (`t`) | trigger | | Send a trigger here in order to start the bursts. |
| `reset` (`r`) | trigger | | Send a trigger here to immediately stop any ongoing burst. |
| `count` (`c`) | integer | `1` | Number of triggers to send in one burst. |
| `skip` (`s`) | integer | `0` | Number of time slots to wait before starting with the burst. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | trigger | The triggers are output here. |

## See also

- [`lfo`](lfo.md) — shares the same `rate` speed-selection scheme.
- [`triggerdelay`](triggerdelay.md) — a more complex trigger delay with memory.
