---
circuit: clocktool
title: Clock divider / multiplier / shifter
obsolete: false
ram_bytes: 96
manual_pages: [178, 179, 180]
category: clock-timing
tags: [clock, divider, multiplier, duty-cycle, gate-length, delay, shift, timing, bpm, tempo]
see_also: [algoquencer, timing]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: "Clock divide/multiply/shift plus duty-cycle and gate-length control; the multiply and delay paths require learning the input clock period."
verification_note: "Headless: drive a steady clock and compare output edges against computed divide/multiply/delay/dutycycle values. Deterministic given a steady clock; allow for the initial clock-learning transient."
---

# clocktool — Clock divider / multiplier / shifter

This circuit implements various clock modifications, such as a clock divider, a
clock multiplier, a tool for changing the length of an incoming gate signal and
a clock time shift.

## Multiply and divide

Here is an example of a simple clock divider that divides the incoming clock by 7
(i.e. for 7 incoming clocks one outgoing clock is being produced).

```droid
[clocktool]
    clock       = I1 # patch a clock here
    output      = O1
    divide      = 7
```

This example doubles the speed of the clock by inserting one additional clock
tick right in the middle between two incoming ones:

```droid
[clocktool]
    clock    = I1 # patch a clock here
    output   = O1
    multiply = 2
```

By using multiplication and division at the same time you can create rhythms like
"two over three":

```droid
[clocktool]
    clock    = I1 # patch a clock here
    output   = O1
    divide   = 3
    multiply = 2
```

Per default the outgoing clock has a duty cycle of 50%, which means that it is
50% of the time high and 50% of the time low – basically a symmetrical square
wave. You can change this with the `dutycycle` input, e.g. to 20%:

```droid
[clocktool]
    clock     = I1 # patch a clock here
    output    = O1
    dutycycle = 20% # same as 0.2
```

## Time shifting the clock

The input `delay` can be used to delay the clock signal. It needs a steady input
clock to work. The possible range of `delay` is -1.0 … 1.0. A value of 1.0 is
equivalent of delaying each clock by exactly one cycle – which is pretty useless,
since it results in the same output clock. But for example a value of 0.1 will
delay the clock by 10%. Here is an example:

```droid
[clocktool]
    clock        = I1 # patch a clock here
    output       = O1
    delay        = 0.1 # same as 10%
```

Using a negative number will result in a clock that is always slightly before the
original clock. This example shifts the output clock 10% ahead of the input
clock:

```droid
[clocktool]
    clock        = I1 # patch a clock here
    output       = O1
    delay        = -0.1
```

Please note that this is not a trigger delay, since it requires a steady input
clock. Otherwise funny and strange things can happen. Also it should be obvious,
that shifting a clock ahead needs knowledge when exactly the next input clock
tick will happen.

Feeding a trigger sequencer like the [`algoquencer`](algoquencer.md) with a
shifted clock allows you to fine tune the exact timing of that voice. You can
easily map the shift amount to a pot for tuning that live by ear:

```droid
[clocktool]
    clock          = I1 # patch a clock here
    output         = _SHIFTED_CLOCK
    delay          = P1.1 * 0.2 - 0.1 # limit to +/- 10%

[algoquencer]
    clock     = _SHIFTED_CLOCK
    ...
```

Please also have a look at [`timing`](timing.md). That can do a similar thing but
is also able to shift the timing differently for each beat in a sequence of
several beats.

If you combine `delay` with `divide` or `multiply`, the delay is applied first.
This means that the amount of delay is in relation to one input clock cycle. The
delayed input clock is then run through the divider and multiplier. If you like
it vice versa, split things up into two `clocktool` circuits, where the first one
does the divide/multiply, feed that output into the second one and do the
delaying there.

## Gate length

Per default the length of the output gate is 10 ms – independently of the length
of the input gate. You can change the gate length either with the jack
`gatelength` and specify a fixed number of seconds, or by using `dutycycle`,
which is a percentage of the output clock rate. Please note: if your gate length
exceeds the time until the next output gate, both will be "joined" and thus no
new gate will be emitted.

Please note if you use `dutycycle`: right at the start of the clock signal or
after a greater speed change of the clock, `clocktool` needs a short time to
learn the new clock speed and correctly adapt the new gate length. This might
lead to two merging gates, which in turn causes a missing gate output.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `clock` (`c`) | trigger | | Patch a steady clock here for this circuit to be of any use. |
| `reset` (`r`) | trigger | | A trigger here resets the internal counters. This is useful if you use the clock divider and want to restart the internal counting from 0, in order to align the clock divider with some external sequencers or the like. |
| `divide` (`d`) | integer | `1` | Number to divide the clock through. This will be rounded to the nearest integer number. Note: if you want to use an external CV then you need to multiply that with some useful number, since otherwise you will get a number between 0 and 1 which is not useful at all. Remember: 10 V translates to a number of 1. |
| `multiply` (`m`) | integer | `1` | Number to multiply the clock with. Same considerations hold as for `divide`. |
| `dutycycle` (`dc`) | 0..1 (mid 0.5) | ☞ smart | Output duty cycle of the clock – which is essentially a square wave – in a range from 0.0 to 1.0 or 0% to 100%. If you don't patch anything here, the length of the trigger output pulses will be 10 ms (DROID's standard trigger duration). |
| `gatelength` (`gl`) | CV | ☞ smart | This jack is alternative to `dutycycle` and will override it if it is used. It sets the length of each output pulse to a fixed value that is independent of the incoming clock. A value of 0.5 (a CV of 5 volts) translates into a gate length of 0.5 seconds. When unpatched, the default 10 ms trigger duration applies. |
| `delay` (`dl`) | CV | `0.0` | This CV allows you to shift the input clock beat around in time. A value of 0.1 will delay each beat by 10% of a clock cycle. A value of -0.1 is also allowed and shifts the beat 10% ahead. For an unmodulated delay -0.1 and 0.9 is just the same, because the output clock will have the same relation to the input clock. But if you modify the delay from 0.0 to 0.9, the next tick will be delayed by 90% of one cycle, where a modification from 0.0 to -0.1 will play the next tick by 10% earlier. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | gate | Here comes the modified clock. |
| `inputpitch` (`ip`) | CV | Experimental output that outputs a representation of the input clock's pitch on a 1V/octave base, based on the reference of 60 BPM (1 Hz). This means that an input clock of 120 BPM will output 1V (a value of 0.1), since 120 BPM it is one octave higher than 60 BPM. If you feed that value to the `rate` input of an LFO you get that running at exactly the same speed (not in the same phase, however). |
| `outputpitch` (`op`) | CV | Same for the modified output clock. |

## See also

- [`algoquencer`](algoquencer.md) — trigger sequencer that can be fine-tuned with a shifted clock.
- [`timing`](timing.md) — can shift timing differently for each beat in a sequence.
