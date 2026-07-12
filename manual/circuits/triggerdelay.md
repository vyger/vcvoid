---
circuit: triggerdelay
title: Trigger Delay with multi tap and optional clocking
obsolete: false
ram_bytes: 248
manual_pages: [404, 405]
category: clock-timing
tags: [trigger, delay, gate, multi-tap, repeat, clock, gatelength, overflow, mute]
see_also: []
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Multi-tap trigger/gate delay with a 16-event memory, gate-length retention or override, repeats, mute and seconds-or-clock-cycle timing plus an overflow output.
verification_note: "Deterministic; feed a known trigger pattern and verify output timing, gate lengths, repeats and the 0.5 s overflow gate headless against a computed reference."
---

# triggerdelay — Trigger Delay with multi tap and optional clocking

This circuit implements a CV controllable delay for a trigger or gate signal. It
listens for triggers at `input` and sends the same triggers *later* to the
`output`. It does *not* look at the voltage level of the inputs. The output
triggers are always sent with 10 V (`I1` … `I8`) or 5 V (on the G8 expander).

As a difference to an analog trigger delay this circuit is capable of keeping
memory of up to 16 triggers. This means it is able to process further incoming
triggers while previous triggers are still in the delay. This allows you to delay
complex rhythmic patterns, e.g. in order to reuse the output of one track of a
trigger sequencer shifted in time for another instrument.

Furthermore, it is able to retain the gate length of the original input signal
and output the delayed gate with exactly the same length.

## Example

Here is the simplest possible example, which delays an incoming gates / triggers
by exactly one second:

```droid
[triggerdelay]
    input = G1
    output = G2
```

You can set the delay in seconds via the `delay` jack. And if you patch
`gatelength`, the original gate length is being ignored and overridden by this
value (also in seconds):

```droid
[triggerdelay]
    input = G1
    output = G2
    delay = 0.1 # 0.1 seconds
    gatelength = 0.05 # 50 ms
```

## Clocked mode

`triggerdelay` supports a clocked mode, in which all timing is relative to an
input clock. You enable clocked mode by simply patching a steady clock into
`clock`. Now `delay` and `gatelength` are relative to *one clock cycle*.

The following example delays all input triggers by one clock cycle (which is the
default):

```droid
[triggerdelay]
    input = G1
    output = G2
    clock = G3
```

If you specify `delay` and/or `gatelength` they are now measured in clock cycles:

```droid
[triggerdelay]
    input = G1
    output = G2
    clock = G3
    delay = 16 # clock cycles
    gatelength = 0.5 # half a clock cycle
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | gate | `0` | Patch triggers or gates to be delayed here. |
| `delay` (`dl`) | CV | `1.0` | Amount of time the incoming triggers are being delayed. When `clock` is patched, this is in relation to one clock cycle, so a delay of 4 will delay the input pattern by exactly 4 beats. Fractions are allowed also. If `clock` is not patched, this parameter is in seconds. So for example in order to delay by 100 ms you need a delay of `0.1`. |
| `gatelength` (`gl`) | CV | ☞ smart | Unless you patch this jack the length of the output gates is exactly the length of the input gates. By use of this parameter you override that length and set a fixed length in *seconds* – or if `clock` is being used – in clock cycles. |
| `repeats` (`rp`) | integer | `1` | Number of times the delayed trigger is being repeated. Each further repetition is with the same delay. |
| `mute` (`m`) | gate | `0` | A high gate signal suppresses any further output gates. However, the current gate is finished normally. |
| `clock` (`c`) | gate | | When you patch this input, the trigger delay runs in clocked mode. In this mode `delay` is relative to one clock cycle. I.e. a delay if `0.5` will delay the trigger by half a clock cycle. The same holds for `gatelength`. That is measured in clock cycles, too. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | gate | Outputs the delayed triggers/gates, while keeping the gate length – unless you have changed that. |
| `overflow` (`ov`) | gate | Whenever there are more input triggers than this circuit can keep memory of, this output outputs a gate of 0.5 sec length. You can wire this to an LED in order to know when this happens. |
