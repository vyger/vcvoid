---
circuit: cvlooper
title: Clocked CV looper
obsolete: false
ram_bytes: 17336
manual_pages: [190, 191, 192]
category: sample-record
tags: [looper, cv-looper, loop, record, overdub, overlay, tape, clock, gate, bypass, pause, tapespeed]
see_also: [button, clocktool, lfo, copy]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Large always-recording tape buffer (8000 samples) with intricate clock-tick quantization (nearest tick, past or future), overlay/overdub, tapespeed resampling and dynamic length — complete spec but heavy state and timing logic.
verification_note: "Headless: drive cvin/gatein against a steady clock, toggle loopswitch/overlay/overdub/bypass and assert cvout/gateout reproduce the recorded window. Partial — quantization of the loop-switch to the nearest tick and tapespeed resampling need careful timing fixtures rather than single-sample checks."
---

# cvlooper — Clocked CV looper

Easy to use clocked CV looper that also loops an additional gate and can do
overlay and overdub.

This circuit is a very easy to use CV looper. It records an incoming CV (and
optionally a gate as well) on a virtual tape loop with a resolution of one sample
per ms. The length of this tape is eight seconds. If you need a longer loop time,
you can reduce the tape speed. At a speed of 0.5 you have a maximum loop time of
16 seconds and a resolution of one sample per 2 ms (which is still pretty decent
for most applications).

This looper is meant to be playable in a live situation as easily as possible.
For that purpose it does not implement the typical loop start → loop stop scheme
– which requires the musician to know beforehand that she will start a loop.
Instead the looper is *always* recording. The loop length is specified in *clock
ticks*. And as soon as the looping is activated, the previous *x* clock ticks of
CV information will be repeated over and over.

Here is an example for a simple looper for one CV without a gate:

```droid
[button]
    button         = B1.1
    led            = L1.1

[cvlooper]
    cvin       = I1
    clock      = I8          # steady clock
    cvout      = O1
    length     = 16          # 16 clock ticks
    loopswitch = L1.1
```

The button `B1.1` is converted into a toggle button for activating the looping.
The CV is read from `I1` and is sent to `O1`. As long as the loop switch is off
the looper is in bypass mode and simply copies `I1` to `O1`. At the same time it
is always recording to its internal endless tape. When the loop switch is
switched on, the last 16 clock ticks of CV information is looped to `O1` and `I1`
is ignored.

Please note: for your convenience the exact time when the loop switch is switched
on is *quantized to the nearest clock tick* – may it be in the future or past.
This makes playing exactly in time much easier.

The second example adds a gate signal – such as output by a ribbon controller.
The gate is running through `I2`→`O2`.

```droid
[button]
    button         = B1.1
    led            = L1.1

[cvlooper]
    cvin       = I1
    gatein     = I2
    clock      = I8          # steady clock
    cvout      = O1
    gateout    = O2
    length     = 16          # 16 clock ticks
    loopswitch = L1.1
```

Using a gate changes the behaviour of the CV looper. The state of `gatein` (not
the exact voltage) is being looped as well. The CV is recorded to the tape *only
while the gate is high*.

Using a gate makes two additional features possible:

1. When `overlay` is **on** and the input gate is active, the input CV will
   override that on the tape and instead the source signal from `cvin` is
   bypassed to the output. The tape's content stays untouched. This allows you to
   overlay the loop CV with your own from time to time.
2. On the other hand, when `overdub` is **on** and the input gate is active, the
   input CV will be written to the tape and replaces the recorded CV at those
   places. And it also will be routed to the output at the same time.

Toggle buttons would fit nicely for these two functions.

Please note: you always need a clock! The CV looper is useless without one. If
you do not want to use an external clock, you can make use of the [`lfo`](lfo.md)
circuit for creating an internal clock.

What if you want to loop more than one CV? Just create more `cvlooper` circuits –
one for each CV. And control them from the same set of buttons.

## Changing the tape or clock speed

It is possible to change the tape speed on the fly in order to slow down or speed
up the recorded loop's content. It is important – however – to always change the
tape speed and clock speed *at the same time and in the same manner*. Otherwise
you will get stuttering effects. So if you double the `tapespeed` you also need
to double the frequency of the clock.

## Changing the length

Changing `length` parameter on the fly is supported and just works. Remember: it
does not set the length of the tape loop but just the length of that part that is
played back. The recording is always done with the maximum length. So if you
*increase* the length while playing back you will get access to the older parts
of the CV history that way. Just don't make the length longer than the actual
tape (see below).

## Limitations

1. The value range of `cvin` and `cvout` is -1 … +1 – or -10 V … +10 V. If that
   range is not sufficient for you, you need to scale it yourself. For example
   you could divide the value by 10 before sending it to `cvin` and multiply it
   with 10 after it arrives at `cvout`:

   ```droid
   [cvlooper]
       cvin = _INPUT_CV / 10
       cvout = _OUT
       ...

   [copy]
       input = _OUT * 10
       output = _OUTPUT_CV
   ```

2. Memory (RAM) is a valuable resource. The CV looper limits itself to 8000
   samples in order not to waste too much memory and leave space for other
   circuits as well (the Droid master has about 100.000 bytes of memory and 8000
   samples need 16.000 bytes). But if you want to make longer loops, you can
   reduce the tape speed and thus use less samples per second.

3. The total loop length can be 128 clock ticks at most. If you need more ticks,
   you can divide the input clock down, using [`clocktool`](clocktool.md):

   ```droid
   [clocktool]
       clock             = G1
       divide            = 2
       output            = _LOOP_CLOCK

   [cvlooper]
       clock             = _LOOP_CLOCK
       cvin              = I5
       tapespeed         = 0.2 # max loop five x longer
       cvout             = O5
       length            = 128 # = 256 original ticks
       loopswitch        = _SOME_BUTTON
   ```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `cvin` (`ci`) | CV | `0.0` | Input CV that should be looped. |
| `gatein` (`gi`) | gate | `1` | Optional input gate. If you do not patch something here, the gate is assumed to be always high. |
| `clock` (`c`) | trigger | | Input clock. The clock is mandatory and is the base for the definition of the loop length. Also the loop switch is quantized in time to the nearest clock. |
| `reset` (`r`) | trigger | | A trigger here resets the playback head immediately to the start of the loop, if you are in playback mode. |
| `length` (`l`) | integer | `16` | Length of the loop in clock ticks. Example: You get a length of 16 ticks by patching the number 16 to `length`. If you want to set the length by means of an external CV that would require 160 Volts. So you need to multiply your input by some useful number in that case. |
| `tapespeed` (`s`) | CV | `1.0` | Relative tape speed, where 1.0 is the normal speed. So a value of `0.5` slows down the speed thus increasing the effective tape length from 8 to 16 seconds while reducing the sampling rate from 1 ms to 2 ms per sample. Changing the tape speed on the fly probably leads to interesting results. |
| `loopswitch` (`ls`) | gate | ☞ smart | Mandatory parameter: While the loop switch is off the CV looper simply sends all input CV and gate to their respective outputs. At the same time CV and gate are also recorded to the tape. When the loop switch is on, the CV and gate are being read from the tape, instead. The input CV and gate are now ignored. |
| `pause` (`p`) | gate | `off` | This is a binary input. If you send a high signal here, the looper pauses. This only works in playback mode. The current CV value is hold the entire time. This is *not* the same as bypass, since in bypass mode the original CV will be routed through. |
| `overlay` (`ov`) | gate | `off` | Overlaying changes the behaviour while looping is active. If `overlay` is set to on, while the input gate is active the gate and CV will be sent directly from the inputs rather than read from the tape. |
| `overdub` (`od`) | gate | `off` | Overdubbing also changes the behaviour during the looping: If it is active then while the input gate is high the input gate and CV will be written to the tape – thus changing the loop on the fly. |
| `bypass` (`b`) | gate | `off` | Setting `bypass` to on copies the input CV and gate from their inputs to their outputs *while keeping the loop's content untouched*. This disabled the looping for the while, but you can get back to it later. Note: this is different from turning off the loop switch, because then your tape's content would be overwritten. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `cvout` (`co`) | CV | Output of the bypassed or looped CV. |
| `gateout` (`go`) | gate | Output of the bypassed or looped gate. |

## See also

- [`clocktool`](clocktool.md) — divide the clock down for longer loops.
- [`lfo`](lfo.md) — generate an internal clock if you have no external one.
