---
circuit: recorder
title: Record and playback CVs and gates
obsolete: false
ram_bytes: 1712
manual_pages: [374, 375, 376, 377, 378, 379]
category: sample-record
tags: [recorder, tape, record, playback, cv, gate, loop, scrub, sd-card, sample-hold, number, sequencer]
see_also: [cvlooper, button, gatetool, buttongroup, delay]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Deterministic virtual-tape record/playback of a CV plus 8 gates and a number, with pause/loop/speed/scrub/trim, triggered sampling and SD-card save/load — lots of state but fully specified for a given input; buttons/LEDs are optional (the mode input works headless).
verification_note: "Headless: record a scripted CV/gate/number stream and assert cvout/gateout/numberout under playbackspeed, scrub, trim and loop; save/load maps to file storage. Circuit is flagged experimental and its SD file format may change."
---

# recorder — Record and playback CVs and gates

Record and playback the movement of one CV, eight gates and one integer number
in the range 0 to 255, with permanent storage on the SD card.

> **Note:** This circuit is still experimental. In a future firmware version it
> might be changed or removed. Also the file format on the SD card for the saved
> recordings might change and a new version might not be able to load old
> recordings.

## Basic usage

The typical interface to the recorder is to use the three buttons "Record",
"Play" and "Stop". The stop button is optional if you are low in buttons. Here
is a simple example patch for recording a CV:

```droid
[p2b8]

[recorder]
    cvin = I1
    cvout = O1
    recordbutton = B1.1
    playbutton = B1.2
    stopbutton = B1.3
    recordled = L1.1
    playled = L1.2
    stopled = L1.3
```

Now feed some CV into `I1`. The circuit starts in idle / stopped mode and `L1.3`
is lit. In that mode the input is bypassed to the output, so that you can "hear"
the effects of the CV at `I1`.

When you press the record button (`B1.1`), the recording starts and `L1.1`
becomes lit. The input is still bypassed to the output but at the same time
written to the tape. Stop the recording either by pressing the stop button
`B1.3` or record again.

The following example also wires up the `pause` input (see the "Pausing"
section) and uses the short parameter aliases `record`, `play` and `stop`:

```droid
[recorder]
    cvin = I1
    cvout = O1
    record = B1.1
    play = B1.2
    stop = B1.3
    recordled = L1.1
    playled = L1.2
    stopled = L1.3
    pause = L1.4
```

Note: For your first experiments you might want to use the value of a pot as
input CV. Then you can record your pot movements:

```droid
[recorder]
    cvin = P1.1 # record pot P1.1
```

You can now play the recording by hitting `B1.2`. The LED in that button is lit
to indicate that the playback is going on. During playback the signal at `I1` is
ignored and instead the tape's content is sent to `O1`. The playback stops when
the recording has played completely or when you hit the stop button. Hitting the
play button during playback does not stop it but immediately restarts it from
the beginning.

Sharing the three buttons with other circuits can be done with the `select`
input – just as usual.

## Pausing

The `pause` input allows you to pause the tape. This input is different from the
three buttons as it does not expect a trigger but a gate (a state). You can use
a [`button`](button.md) circuit for that:

```droid
[p2b8]

[button]
    button = B1.4
    led = L1.4
```

When you enable `pause` during playback, the playback is hold and the output
sticks at the current CV. Disable pause to go on with the playback.

When you enable `pause` while recording, the tape stops and the input CV is no
longer recorded. But you can resume the recording later by disabling pause.

## Looping

The recorder has a simple loop function builtin. When you set the input `loop`
to 1, a playback immediately starts again when it's finished.

If looping is your main objective, please have a look at
[`cvlooper`](cvlooper.md). That circuit has some very useful features for a real
performance looper.

## Playback speed

With the parameter `playbackspeed` you can alter the speed of the playback. The
default value is 1. A value of 2 doubles the speed. The fun part: you even can
use a negative speed for running the tape backwards. In that case a press to the
play button starts the playback at the tape end.

The following example maps the speed to a pot that's scaled to a range from -5
to 5 (five times speed backwards to five times speed forwards). The center
position sets the speed to 0 and stops the tape.

```droid
[recorder]
    playbackspeed = P1.1 * 10 - 5
    ...
```

## Scrubbing

Scrubbing is a special playback mode that's enabled by `scrub = 1`. During
scrubbing no linear playback is done. Instead, you select a position on the tape
with the input CV `scrubposition`. Example:

```droid
[button]
    button = B1.5
    led = L1.5

[recorder]
    scrub = L1.5
    scrubposition = P1.1
    ...
```

While the button `B1.5` is enabled, the recorder outputs the CV that's at the
position that `P1.1` selects. The left position of the pot (or the value 0)
selects the start of the recording, the right position (1) the end.

While `scrub` is 1, the current state (play, record, stop) of the recorder is
ignored. It is in scrub mode. The `playled` output is 1, the other LED outputs
are 0.

## Trimming the start and end

The two inputs `trimstart` and `trimend` range from 0 to 1 and limit the portion
of the recording that is used for playback or scrubbing. For example
`trimstart = 0.1` and `trimend = 0.8` disables the first 10% and the last 20% of
the recording.

If you map the trimming positions to two pots you can manually select a portion.
Just make sure that you start with the `trimstart` pot fully left and `trimend`
fully right:

```droid
[recorder]
    trimstart = P1.1
    trimend = P1.2
    ...
```

This limitation is not permanent. The recording itself is not modified by using
trimming.

## Recording gates and numbers

Along the CV, the recorder also records the state of up to eight input gates.
You could record the output of a multi-track drum sequencer or even a manually
tapped button pattern with that:

```droid
[recorder]
    gatein1 = I1
    gatein2 = I2
    gatein3 = I3
    gatein4 = I4
    gateout1 = O1
    gateout2 = O2
    gateout3 = O3
    gateout4 = O4
    ...
```

Unlike `cvin` and `cvout` the gate tracks on the tape just distinguish between 0
and 1.

In addition you can record one discrete integer number from 0 to 255:

```droid
[recorder]
    numberin = I1
    numberout = O1
    ...
```

Unlike with the CV, no linear interpolation is done for these integer numbers.
Every time the input number changes a new sample is created.

Applications for recording a number could be chord progressions or melodies that
are represented by note numbers rather than pitch CVs.

## Technical background and limitations

The two circuits [`recorder`](recorder.md) and [`delay`](delay.md) are based on
the same implementation of a virtual tape. This virtual tape has three tracks
with three recording and playback heads:

1. One head for recording a continuous CV in the range −1 … +1 (which is
   −10 V … 10 V)
2. One head for recording eight gate tracks in parallel (CVs where just 0 and 1
   is recorded)
3. One head for recording a discrete integer number in the range 0 … 255

All these are recorded in parallel, so for example it's easy to record a CV/gate
signal with just one recorder. The discrete number is useful for recording the
outputs of [`buttongroup`](buttongroup.md) circuits or the switches on the S10
or similar things.

Note: The dynamic range of CV signal on the tape is just −1 … +1 (or
−10 V … +10 V). Any "too hot" signal is clipped to that range. The internal
resolution of the CV is 16 bit (precisely: one Volt is divided in 3200 steps).
If you need a larger range, you need to divide the input signal and multiply the
output signal by some factor, but lose a bit precision that way.

The track with the eight gates records just 0 and 1. Any other value will be
squeezed into that format: values below 0.1 (1 V) are considered 0, all other
values 1.

In order to use the RAM of the DROID as efficient as possible (and allow for
many multiple instances of these circuits), the tape uses just 256 samples. Each
time the state of one of the gates or the value of the number changes, a new
sample is created. A change in the input CV is handled more intelligently as the
CV values of the samples or interpolated linearly. The maximum error between the
interpolated value and the actual stored CV is limited to 0.0001 (which is 1 mV).

If the input CV is more chaotic, however, the number of samples per time is
limited to an average of one sample every 20 ms, while short periods with up to
10 samples without this limitation are allowed. This ensures that the minimum
recordable tape length is 256 × 20 ms, which is 5.12 seconds. Usually CVs are
not so chaotic but either stepped or moving smoothly, so the recordings can be
much longer.

If you have the special case of a stepped input CV – such as the output from a
sequencer or from a CV/gate keyboard – you can switch to an alternative mode.
Patch the gate output of the sequencer or keyboard into the `sample` input of
the circuit. This enables the "triggered mode". Here a new sample is just and
only created at each positive gate edge of the `sample` input. So the recordings
can be as long as 256 notes.

Note: That way you would lose the gate length, since the end of the gate does
not trigger a new sample. Use the [`gatetool`](gatetool.md) with the `inputgate`
and `outputedge` to get one trigger at each edge and feed that into `sample`.

## Saving the tape to disk

The recorder does not support presets because of memory limitations. But you can
save the current contents of the tape to your SD card. This is done by the two
trigger inputs `save` and `load`, which are usually mapped to some buttons. Here
is a simple example.

```droid
[recorder]
    save = B1.5
    load = B1.6
    ...
```

If you hit button `B1.5`, the file `tape0001.bin` is created on your SD card.
Button `B1.6` loads that file into the circuit.

You can use any file number from 1 to 9999 by using the parameter `filenumber`.
You might want to map that to a rotary switch of an S10:

```droid
[recorder]
    save = B1.5
    load = B1.6
    filenumber = S2.1
    ...
```

Note: Loading and saving is done in real time from/to your SD card. The files
are very small, but the operation can take a small number of milliseconds.
During that time no circuit will do its job. And if your SD card is missing,
things lag a bit more due to timeouts.

One important difference to presets is that these files can be shared among
circuits and even among different patches. A recording of the recorder circuit
can be loaded with every recorder or [`delay`](delay.md) circuit.

## Display

If you have a DB8E display controller (see the manual
([hardware](../hardware.md))), the display will show **Recording**, **Playback**
or **Bypass** whenever you change the recording mode.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `playbutton` (`pb`) | trigger | | A trigger here starts or restarts the playback. |
| `recordbutton` (`rb`) | trigger | | A trigger here starts or stops the recording. |
| `stopbutton` (`sb`) | trigger | | A trigger here stops an ongoing playback or recording. |
| `loop` (`lo`) | gate | `off` | Set this to 1 to enable loop mode. In loop mode the playback is restarted immediately when it's finished. |
| `pause` (`p`) | gate | `off` | While this is 1, playback or recording is halted (the tape stops moving for the while). |
| `mode` (`m`) | integer | | Using this input is an alternative to using the three button inputs. If you patch `mode`, the three buttons (and LED outputs) are ignored. Instead you set the mode with this input: `0` = Idle / stopped, `1` = Playback, `2` = Recording. Since you set the mode from "outside", the recorder cannot switch it by itself. So if the mode is set to 1 (playback) and the playback is finished, it stays in playback mode and continues outputting the last sample. |
| `playbackspeed` (`ps`) | CV | `1.0` | Sets the speed of the tape during playback. 1 is normal speed, 0.5 half speed, 2 double speed, and so on. Negative speeds are allowed and move the tape backwards. The speed 0 stops the tape. |
| `scrub` (`sc`) | gate | `off` | If 1 enables scrubbing. Now the outputs reflect the tape position that is set with `scrubposition`. |
| `scrubposition` (`sp`) | 0..1 | `0.0` | Position of the tape to play when scrubbing is enabled. |
| `trimstart` (`ts`) | 0..1 | `0.0` | Omits a fraction of the recording at the beginning during playback and scrubbing. 0.1 omits the first 10%. |
| `trimend` (`te`) | 0..1 | `1.0` | Omits a fraction of the recording at the end during playback and scrubbing. 0.8 omits the last 20% (not 80%!). |
| `cvin` (`ci`) | CV | `0.0` | Continuous input CV. |
| `numberin` (`ni`) | integer | | Discrete input number in the range 0 … 255. |
| `gatein1 … gatein8` (`gi`) | CV | | Input gates. |
| `clock` (`c`) | trigger | | If you use this clock input, all time inputs are measured in clock ticks instead of seconds. |
| `sample` (`sm`) | trigger | | If you patch this input, "triggered" mode is enabled. In this mode, the virtual tape just records a new CV on each trigger at `sample`. So it just records stepped CVs, no slopes and no CV changes between the triggers. |
| `timewindow` (`tw`) | CV | `0.0` | When in triggered mode, this optional parameter helps tackling a problem that many hardware sequencers show: often their pitch CV is not at its final destination value at the time their gate is being output. Often you see a very short "slew" ramp of say 5 ms after the gate. During that time the pitch CV moves from its former to the new value. Now if you trigger the recorder with the sequencer's gate you will essentially sample the previous pitch CV instead of the new one. Or maybe something in between. The `timewindow` parameter configures a short time window after the trigger to `sample`. During that time period the tape will constantly adapt the last sample to a changed input CV. When that time is over, the input is finally frozen on the tape. The `timewindow` parameter is in seconds. So when you set `timewindow` to say 0.005 (which means 5 ms), you give the input CV 5 ms time for settling to its final value after a trigger to `sample` before freezing it. |
| `bypass` (`b`) | gate | `off` | Setting `bypass` to on copies the input signals directly to the outputs, regardless of any other stuff going on. |
| `save` (`sv`) | trigger | | Send a trigger here to save the current contents of the tape to a file on the SD card. The filename is `tapeXXXX.bin`, where XXXX is replaced by the number set by `filenumber`. |
| `load` (`ld`) | trigger | | Send a trigger here to load a previously saved file into the tape. Use `filenumber` to specify which file to load. |
| `filenumber` (`f`) | integer | `1` | Number of the file to load or save. The range is 0 - 9999. If `filenumber` is 123, the name on the SD card is `tape0123.bin`. These files are shared between all recorder and delay circuits. |
| `display` (`dy`) | integer | `1` | This parameter selects the number of the DB8E display controller, where any change of this circuit's state will show up. Per default, the first DB8E available is used. Set this to 2 if you have a second DB8E and want to use that. Note: any other controller is not being counted here, it's really just the number of the DB8E as counted from the master module. Set this to 0 to suppress any display output from this circuit. |
| `header` (`hr`) | text | ☞ smart | This text is displayed at the top of the DB8E display whenever this circuit shows up in the display. If you omit this, an automatic title is used. For example if the output of this circuit is fed into an internal patch cable, the name of this cable is displayed. Setting `header = ""` or `header = 0` removes the header and just displays the actual value. |
| `select` (`s`) | integer | ☞ smart | The select input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to 1). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | ☞ smart | This input makes the select input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is 0, the circuit will be active if `select` is exactly 0 instead of a positive gate signal. In some cases this is more convenient. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `recordled` (`rl`) | gate | Is 1 during recordings. |
| `playled` (`pl`) | gate | Is 1 during playback or scrubbing. |
| `stopled` (`sl`) | gate | Is 1 when no playback, recording or scrubbing is going on. |
| `cvout` (`co`) | CV | Output of the continuous input CV. |
| `numberout` (`no`) | integer | Output of the discrete number. |
| `gateout1 … gateout8` (`go`) | gate | Output of the gates. |
| `overflow` (`ov`) | gate | When the internal memory of the tape is exceeded and data got lost, this gate goes to 1 for 0.5 seconds. If you are suspecting this situation, you can wire this output to an LED and observe the memory status that way. |

## See also

- [`cvlooper`](cvlooper.md) — a dedicated performance looper with more useful looping features.
- [`delay`](delay.md) — based on the same virtual tape; shares the saved tape files.
- [`button`](button.md) — for driving the `pause` and other gate/state inputs.
- [`gatetool`](gatetool.md) — to derive an edge trigger for `sample`.
- [`buttongroup`](buttongroup.md) — a common source for the recordable integer number.
