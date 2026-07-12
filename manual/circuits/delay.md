---
circuit: delay
title: A tape delay for CVs, gates and numbers
obsolete: false
ram_bytes: 1672
manual_pages: [195, 196, 197]
category: utility
tags: [delay, tape, cv, gate, number, trigger-delay, clock, virtual-tape, sd-card, overflow]
see_also: [recorder, buttongroup, gatetool]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Virtual-tape delay for CV/8 gates/number with linear interpolation, adaptive sampling budget and triggered mode; complete spec, no controller dependency (SD save/load maps to plain file I/O).
verification_note: "Headless: feed cvin/gatein/numberin, advance time or clock ticks and assert outputs reproduce the inputs shifted by the delay. Partial — the adaptive sample-budgeting (avg one sample per 20 ms, 1 mV interpolation error) means long chaotic CVs verify against tolerance bands rather than exact samples."
---

# delay — A tape delay for CVs, gates and numbers

Use this circuit to delay the movement CVs, gates or integer numbers in time.
The usage is very simple. Feed input signals into the circuit, set a delay time
and these signals are output again delayed by that time.

> Note: This circuit is still experimental. In a future firmware version it might
> be changed or removed. Also the file format on the SD card for the saved
> recordings might change and a new version might not be able to load old
> recordings.

The basic usage of the delay is very simple:

```droid
[delay]
    cvin = I1
    cvout = O1
    delay = 0.5
```

Here the signal from `I1` is output again at `O1` with a delay of 0.5 seconds.

You can make the delay time depend on the speed of a clock signal. Just feed a
steady clock into `clock`. Now the `delay` parameter is measured in clock ticks –
not in seconds anymore.

```droid
[delay]
    cvin = I1
    cvout = O1
    clock = G1 # input clock
    delay = 4 # delay by 4 ticks
```

## Use as a trigger delay

Alongside the continous CV, eight gate signals can be fed through the delay. Use
`gatein1 … gatein8` and `gateout1 … gateout8` for this purpose:

```droid
[delay]
    gatein1 = G1
    gatein2 = G2
    gatein3 = G3
    gatein4 = G4
    gateout1 = G5
    gateout2 = G6
    gateout3 = G7
    gateout4 = G8
    delay = 0.5
```

Now the gate patterns at the inputs `G1` through `G4` appears time shifted by
0.5 seconds at the outputs `G5` through `G8`.

## Technical background and limitations

The two circuits [`recorder`](recorder.md) and [`delay`](delay.md) are based on
the same implementation of a virtual tape. This virtual tape has three tracks
with three recording and playback heads:

1. One head for recording a continous CV in the range −1 … +1 (which is
   −10 V … 10 V)
2. One head for recording eight gate tracks in parallel (CVs where just 0 and 1
   is recorded)
3. One head for recording a discrete integer number in the range 0 … 255

All these are recorded in parallel, so for example it's easy to record a CV/gate
signal with just one cvrecorder. The discrete number is useful for recording the
outputs of [`buttongroup`](buttongroup.md) circuits or the switches on the S10
similar things.

Note: The dynamic range of CV signal on the tape is just −1 … +1 (or
−10 V … +10 V). Any "too hot" signal is clipped to that range. The internal
resolution of the CV is 16 bit (precisly: one Volt is divided in 3200 steps). If
you need a larger range, you need to divide the input signal and multiply the
output signal by some factor, but loose a bit precision that way.

The track with the eight gates records just 0 and 1. Any other value will be
squeezed into that format: values below 0.1 (1 V) are considered 0, all other
values 1.

In order to use the RAM of the DROID as efficient as possible (and allow for
many multiple instances of these circuits), the tape uses just 256 samples. Each
time the state of one of the gates or the value of the number changes, a new
sample is created. A change in the input CV is handled more intelligently as the
CV values of the samples or interpolated linearily. The maximum error between
the interpolated value and the actual stored CV is limited to 0.0001 (which is
1 mV).

If the input CV is more chaotic, however, the number of samples per time is
limited to an average of one sample every 20 ms, while short periods with up to
10 samples without this limitations are allowed. This ensures that the minimum
recordable tape length is 256 × 20 ms, which is 5.12 seconds. Usually CVs are
not so chaotic but either stepped or moving smoothly, so the recordings can be
much longer.

If you have the special case of a stepped input CV – such as the output from a
sequencer or from a CV/gate keyboard – you can switch to an alternative mode.
Patch the gate output of the sequencer or keyboard into the `sample` input of
the circuit. This enables the "triggered mode". Here a new sample is just and
only created at each positive gate edge of the `sample` input. So the recordings
can be as long as 256 notes.

Note: That way you would loose the gate length, since the end of the gate does
not trigger a new sample. Use the [`gatetool`](gatetool.md) with the `inputgate`
and `outputedge` to get one trigger at each edge and feed that into `sample`.

## Saving the tape to disk

The delay does not support presets because of memory limitations. But you can
save the current contents of the tape to your SD card. This is done by the two
trigger inputs `save` and `load`, which are usually mapped to some buttons. Here
is a simple example.

```droid
[delay]
    save = B1.5
    load = B1.6
    ...
```

If you hit button `B1.5`, the file `tape0001.bin` is created on your SD card.
Button `B1.6` loads that file into the circuit.

You can use any file number from 1 to 9999 by using the parameter `filenumber`.
You might want to map that to a rotary switch of an S10:

```droid
[delay]
    save = B1.5
    load = B1.6
    filenumber = S2.1
    ...
```

Note: Loading and saving is done in real time from/to your SD card. The files
are very small, but the operation can take a small number of milliseconds.
During that time no circuit will do its job. And if your SD card is missing,
things lag a bit more due to timeouts.

One important difference to presets is that these files can be share among
circuits and even among different patches. A recording of the
[`recorder`](recorder.md) circuit can be loaded with every `recorder` or `delay`
circuit.

## Loading and saving

You might wonder, why this circuit offers loading and saving of the tape's
content to the SD card. The reason is not because it's super useful but because
`delay` uses the same tape implementation as [`recorder`](recorder.md) and
saving is part of that.

When you load a file into the tape, it's contents will be audible for a short
time. But soon after the tape is overwritten with the new incoming data.

Saving might make more sense. You could make a snapshot of the tape's content
and load that into a [`recorder`](recorder.md) for playback. But even that
doesn't seem to be game changing material.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `delay` (`dl`) | CV | `1.0` | The CVs are delayed by this amount of seconds. If you patch `clock` as well, the delay is specified in clock tick, so then `delay = 1` means "delay by one clock tick". |
| `cvin` (`ci`) | CV | `0.0` | Continous input CV. |
| `numberin` (`ni`) | integer | | Discrete input number in the range 0 … 255. |
| `gatein1 … gatein8` (`gi`) | gate | | Input gates. |
| `clock` (`c`) | gate | | If you use this clock input, all time inputs are measured in clock ticks instead of seconds. |
| `sample` (`sm`) | trigger | | If you patch this input, "triggered" mode is enabled. In this mode, the virtual tape just records a new CV on each trigger at `sample`. So it just records stepped CVs, no slopes and no CV changes between the triggers. |
| `timewindow` (`tw`) | CV | `0.0` | When in triggered mode, this optional parameter helps tackling a problem that many hardware sequencers show: often their pitch CV is not at its final destination value at the time their gate is being output. Often you see a very short "slew" ramp of say 5 ms after the gate. During that time the pitch CV moves from its former to the new value. Now if you trigger the cvtape circuit with the sequencer's gate you will essentially sample the *previous* pitch CV instead of the new one. Or maybe something in between. The `timewindow` parameter configures a short time window after the trigger to `trigger`. During that time period the tape will constantly adapt the last sample to a changed input CV. When that time is over, the input is finally frozen on the tape. The `timewindow` parameter is in seconds. So when you set `timewindow` to say 0.005 (which means 5 ms), you give the input CV 5 ms time for settling to its final value after a trigger to `sample` before freezing it. |
| `bypass` (`b`) | gate | `off` | Setting `bypass` to `on` copies the input signals directly to the outputs, regardless of any other stuff going on. |
| `save` (`sv`) | trigger | | Send a trigger here to save the current contents of the tape to a file on the SD card. The filename is `tapeXXXX.bin`, where `XXXX` is replaced by the number set by `filenumber`. |
| `load` (`ld`) | trigger | | Send a trigger here to load a previously saved file into the tape. Use `filenumber` so specify which file to load. |
| `filenumber` (`f`) | integer | `1` | Number of the file to load or save. The range is 0 - 9999. If `filenumber` is 123, the name on the SD card is `tape0123.bin`. These files are shared between all `recorder` and `delay` circuits. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `cvout` (`co`) | CV | Output of the continous input CV. |
| `numberout` (`no`) | integer | Output of the discrete number. |
| `gateout1 … gateout8` (`go`) | gate | Output of the gates. |
| `overflow` (`ov`) | gate | When the internal memory of the tape is exceeded and data got lost, this gate goes to 1 for 0.5 seconds. If you are suspecting this situation, you can wire this output to an LED and observe the memory status that way. |

## See also

- [`recorder`](recorder.md) — shares the same virtual-tape implementation.
- [`buttongroup`](buttongroup.md) — a good source for the discrete `numberin`.
- [`gatetool`](gatetool.md) — turn gate edges into triggers for `sample`.
