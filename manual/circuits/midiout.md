---
circuit: midiout
title: CV to MIDI converter
obsolete: false
ram_bytes: 664
manual_pages: [297, 298, 299, 300, 301, 302, 303, 304, 305]
category: midi
tags: [midi, cv-to-midi, note, velocity, pitchbend, cc, clock, polyphony, x7, trs, usb, program-change, aftertouch, pitch-tracking]
see_also: [midiin, midithrough, midifileplayer]
impl_difficulty: moderate
controller_binding: x7-required
verification: rack-automated
spec_gap: false
difficulty_note: Requires the X7 MIDI/USB expander; very broad CV-to-MIDI surface (notes, velocity, CCs, pitchbend/tracking, clock, pedals) plus pitch-stabilization timing logic.
verification_note: "Drive CV/gate inputs in Rack and capture the emitted MIDI stream via a MIDI monitor module/MCP; most conversions are deterministic, but pitchstabilization and triggerdelay need real-time driving to observe."
---

# midiout — CV to MIDI converter

This circuit allows you to "play" notes via MIDI on an external hardware or
software synth. You also can send all sorts of other MIDI events. You need the
X7 expander for that to work (see the manual ([hardware](../hardware.md))).

The MIDI implementation of `midiout` is very comprehensive. Please look at the
table of input jacks for all features. Here I just want to show some basic
examples to get you started quickly. Fun fact: this is the only circuit that
does not have any outputs, because all output is done via MIDI!

## Basic operation

Easy things should be easy and complex things should be possible. So we start
with the easy things. Here is a patch that converts a CV / gate input from I1 /
I2 into a stream of MIDI notes and sends them out via the 3.5 mm TRS jack on
MIDI channel 1:

```droid
[midiout]
    pitch = I1
    gate = I2
```

Every time the gate input at I2 goes from off to on, the current pitch (1V/Oct)
is read from I1. Then one MIDI "note on" event is being created. The "velocity"
of that note is set to the default value of 1.0, which is the maximum (every
MIDI note event has a velocity, which is meant to reflect the speed at which the
key of the keyboard has been pressed).

You can specify any velocity you like with the jack `velocity`. Let's randomize
that. Since the velocity jack is just read just at the note starts, we don't
need a sample and hold here:

```droid
[random]
    minimum = 0.5 # minimum allowed velocity
    maximum = 1.0 # maximum allowed velocity
    output = _VELOCITY

[midiout]
    pitch = I1
    gate = I2
    velocity = _VELOCITY
```

Note: the range of the velocity goes from 0.0 to 1.0 – just as all other
parameters in `midiout` do. Internally MIDI uses the integer numbers 0 to 127.

## Output selection

You can send your MIDI stream either via the 3.5 mm TRS jack of the X7 (TRS
stands for "tip ring sleeve" – the structure of the stereo 3.5 mm plug) or via
the USB-C port. This is controlled by the parameters `usb` and `trs`.

Per default the stream is sent via TRS. As soon as you use either `usb` or `trs`
you set this explicitely. Here is a complete table of all possible usages of
these inputs (empty cells mean that the parameter is not used):

| usb | trs | Result |
|-----|-----|--------|
|  |  | Uses TRS only (default) |
| `usb = 1` |  | Uses USB only |
| `usb = 0` |  | Uses TRS only (default) |
|  | `trs = 1` | Uses TRS only (default) |
|  | `trs = 0` | Uses USB only |
| `usb = 0` | `trs = 1` | Uses TRS only (default) |
| `usb = 1` | `trs = 0` | Uses USB only |
| `usb = 1` | `trs = 1` | Uses both TRS and USB |
| `usb = 0` | `trs = 0` | Mute! does not send MIDI. |

Note: MIDI via USB has a much higher data rate then via TRS. If you use both USB
and TRS at the same time, USB will run at the same (lower) data rate as TRS.
This might lead to fewer updates for CCs and similar. The reason is that the
`midiout` circuit does not make a separate book keeping for USB and TRS but
creates just one common MIDI data stream. If that's an issue for you, duplicate
your `midiout` circuit and create one instance for TRS and one for USB. Then
they create two separate MIDI streams that are optimized for the specific
maximum data rates of their output ports.

If you have a MASTER18 – especially if combined with the X7 – you can have more
than one port of a type. When selecting the target ports you can use numbers
greater than 1 in this case.

The upper table still applies in the following way: If you don't specify the
port explicitely, the first port is used. For example if you specify `usb = 0`
and not `trs`, the port MIDI1 on the MASTER18 is used.

Here is how the MIDI ports are numbered in the various hardware configurations:

### MASTER + X7

| Value | Meaning |
|-------|---------|
| `usb = 1` | Send to the USB port on the X7 |
| `trs = 1` | Send to the TRS port on the X7 |

### MASTER18

| Value | Meaning |
|-------|---------|
| `usb = 1` | Send to the USB port |
| `trs = 1` | Send to the port MIDI1 |
| `trs = 2` | Send to the port MIDI2 |
| `trs = 10` | Send to both MIDI1 and MIDI2 |

### MASTER18 + X7

| Value | Meaning |
|-------|---------|
| `usb = 1` | Send to USB port on the MASTER18 |
| `usb = 2` | Send to USB port on the X7 |
| `usb = 10` | Send to both USB ports |
| `trs = 1` | Send to MIDI1 on the MASTER18 |
| `trs = 2` | Send to MIDI2 on the MASTER18 |
| `trs = 3` | Send to the TRS port on the X7 |
| `trs = 10` | Send to all three TRS ports |

## Polyphonic patches

One great motivation for doing CV to MIDI at all is playing polyphonic music on
hardware synths, because polyphony in Eurorack is quite costly and very time and
space consuming. One `midiout` circuit can play up to eight notes at the same
time and if that's not enough, add a second `midiout` circuit. For each
simultaneous note add one pair of `pitch` and `gate` jacks:

```droid
[midiout]
    pitch1 = I1
    pitch2 = I2
    pitch3 = I3
    gate1 = I5
    gate2 = I6
    gate3 = I7
```

If you work with velocity, each voice has its own velocity input:

```droid
[midiout]
    pitch1 = I1
    pitch2 = I2
    pitch3 = I3
    gate1 = I5
    gate2 = I6
    gate3 = I7
    velocity1 = 0.6
    velocity2 = 0.8
    velocity3 = 1.0
```

## CC and other controllers

There are several continuous values that you can change over time. The following
example lets you control the MIDI CC number 17 via input I3 (at a range from 0 V
to 10 V) and the volume and modulation wheel with two pots:

```droid
[midiout]
    pitch = I1
    gate = I2
    ccnumber1 = 17
    cc1 = I3
    volume = P1.1
    modwheel = P1.2
```

## Note gates

Note gates are a convenient way to directly trigger certain notes. Here you
select up to eight notes and get one dedicated trigger for each. You select the
note number with `note1`, `note2`, etc. These are MIDI note numbers from 0 to
127, where 0 is usually a C-2 (and 24 a C0). When you send a trigger into the
corresponding `notegate` input, that note will be played.

```droid
[midiout]
    note1 = 24
    note2 = 25
    notegate1 = I1
    notegate2 = I2
```

This is sometimes convenient when triggering drum voices.

## Creating a MIDI clock

If you want to simulate a MIDI sequencer, you need to provide a MIDI clock. This
can be injected into the output either by sending a modular clock that is running
on 16th notes into `clock`, or a raw MIDI clock into `midiclock`.

Example: You want your clock to run at 120 BPM. BPM means beats per minute. And
a beat is ment to be a quarter note. 120 quarter notes a minute means two
quarter notes a second and that means eight 16th notes a second, hence our clock
needs to run at 8 Hz.

```droid
[lfo]
    hz = 8 # 120 BPM
    square = _CLOCK

[midiout]
    clock = _CLOCK
```

Note: The input jack `clock` receives 16th clocks. The actual MIDI clock is
derived from that by multiplying it by 6. This means that the circuit
interpolates the clock by measuring its speed and introducing five artifical
clocks ticks inbetween the original ticks. While this works reasonably well for
a steady clock, changes in clocks speed cannot be picked up very fast.

So if you work with a clock that can change the speed, better use the jack
`midiclock` instead and directly supply the MIDI clock (at a six times higher
speed). Here is the same example but now we directly create the MIDI clock:

```droid
[lfo]
    hz = 48 # 120 BPM MIDI clock
    square = _MIDICLOCK

[midiout]
    midiclock = _MIDICLOCK
```

## Start, Stop, Reset

MIDI sequencers also output "start" and "stop" messages. You can send them
either via triggers into `start` and `stop` or use the input `running` for both.
When `running` goes high, a "start" message is sent, when it goes low a "stop"
message.

## Pitch tracking

Pitch tracking is an advanced feature that works in monophonic setups. Here
`midiout` watches the input pitch all the time and adapts the pitch of the
currently played note via MIDI pitchbend events in order to reflect the pitch
changes. See the documentation of the `pitchtracking` jack for details.

## Pitch stabilization

MIDI output appears simple to implement, but isn't when you look at the details.
One tricky problem is that many modules that output pitch information are not
very precise in timing. Sequencers often need a couple of milliseconds for the
pitch CV to reach its final value and stabilize after the gate is being output.

The following diagram shows a gate signal going high (blue) and a pitch signal
with a small ramp reaching its final destination shortly afterwards (red):

I've seen a very similar situation indeed when I attached an oscilloscope to the
output of a very famous Eurorack sequencer.

Now when you would issue "note on" right at the beginning of the gate, you would
obviously output the wrong pitch. What you need to do is to first wait for some
time. You need to delay the note event until the pitch is stable. Of course this
introduces some undesirable latency, so it is crucial to keep that as short as
possible.

The `midiout` circuit has two methods for doing this. The first one is enabled
per default and called `pitchstabilization`. Here, as soon as the gate goes
high, it watches how pitch evolves over time. And it delays the "note on" as
long as the pitch is still moving. When it has stabilized – i.e. on the same
level for at least some very short time – the note event is issued immediately.
This keeps the latency at a minimum.

If that does not work out well for you, you can deactivate this algorithm. One
reason could be that your pitch never stabilizes, since it is some ever evolving
random data:

```droid
[midiout]
    pitch = I1
    gate = I2
    pitchstabilization = 0
```

The second method is introducing a fixed delay of the gate signal with the input
`triggerdelay`. Using that parameter automatically disables pitch stabilization:

```droid
[midiout]
    pitch = I1
    gate = I2
    triggerdelay = 3.5 # delay gate by 3.5 ms
```

Now the gate is delayed exactly 3.5 ms every time. You need to try out various
useful values yourself. The best value depends on your sequencer (or whatever
other source you are using).

You can also activate both methods at once. This makes sense in situations,
where the pitch is stable for a very short time after the gate but afterwards
begins to move, like in the following diagram:

As you can see, now after the gate comes high the pitch lingers on for 2 ms at
its old value until the ramp starts. Here set the `triggerdelay` to 2 and
explicitly set `pitchstabilization = 1`:

```droid
[midiout]
    pitch = I1
    gate = I2
    triggerdelay = 2
    pitchstabilization = 1
```

## Sending notes by number

If you are familiar with MIDI, you sometimes might want to send a certain note
number rather than a pitch. MIDI knows notes from 0 (C-2) to 127. To do this,
divide your number by 120 before sending it to `pitch`.

```droid
[midiout]
    pitch = _SOMENUMBER / 120
    gate = _SOMEGATE
```

Why not 127? Because the `pitch` input counts notes by semitones. And one
semitone in modular is 1/12 V, which in Droid means 1/120. Dividing by 127 will
be slightly off and send wrong note numbers.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `channel` (`ch`) | integer | `1` | Selects the MIDI channel to send the events on. Default is to send on channel 1. There are 16 channels. Make sure that the receiving device listens to this (or to all) channels. |
| `usb` (`usb`) | gate | ☞ smart | Set `usb = 1` if you want to send the MIDI output to the USB-C port. You can set `trs = 1`, as well, for sending the data to both outputs. If you don't use `usb` nor `trs`, the output will be sent to the TRS output only. |
| `trs` (`trs`) | gate | ☞ smart | This controls wether the MIDI data is sent via the TRS output of the X7. If you just want the TRS output, you don't need this, because that is the default. If you want the output both on USB and TRS, you need to set `usb = 1` and `trs = 1` at the same time. |
| `pitch1 … pitch8` (`p`) | pitch | `0V` | Pitch of the notes to be played in modular style (1 V/octave). The range is from -2 V (MIDI note 0, usually C-2) to 8.583 V (MIDI note 127, usually G9). You can use up to eight pitch inputs for playing up to eight notes in parallel. `pitch1` can be abbreviated with just `pitch`. |
| `gate1 … gate8` (`g`) | gate |  | A positive edge into the gate jacks trigger note on messages (starts the note at the pitch set by the corresponding `pitch` input). A negative edge ends the currently played note. |
| `velocity1 … velocity8` (`v`) | 0..1 | `1.0` | The velocities for the up to eight notes. The velocity value is just picked up at the start of the note (at the positive edge of the corresponding `gate` inputs). It ranges from 0.0 to 1.0. A value of 0.0 is practically the same as "note off". The default velocity is 1.0. |
| `noteoffvelocity1 … noteoffvelocity8` (`nv`) | 0..1 | ☞ smart | MIDI also sends a velocity at the end of a note. The idea is to model the speed with which a key is being released. This is rarely used. If you don't use these jacks, the velocity for "note off" events is the same as that for "note on" events. |
| `pressure1 … pressure8` (`pr`) | 0..1 | ☞ smart | Sends key pressure events for individually played notes via the MIDI event "polyphonic key pressure" (this is not a CC!). These values are not processed at the time of note on/off events but all the time and can also change while a note is already being played. This corresponds to "aftertouch" key pressure on keyboards that have a pressure sensor per key. If nothing is patched here, no pressure events are sent. |
| `channelpressure` (`cp`) | 0..1 | ☞ smart | Whenever this CV changes, sends a MIDI channel pressure event, also known as "aftertouch". This corresponds to keyboards that just have one global pressure sensor and not one per key. If nothing is patched here, no channel pressure events are sent. |
| `pitchstabilization` (`ps`) | gate | `1` | Enables or disables pitch stabilization. It is on per default and can be disabled by setting this jack to 0. Pitch stabilization fixes timing issues where the input pitch needs some time for reaching the target pitch after a gate. |
| `triggerdelay` (`td`) | CV | `0.0` | Introduces a delay between the incoming gate signal (just the positive edge) and the "note on" event. This can tackle the problem when your pitch input (sequencer etc.) needs some time after the gate in order to reach and stabilize the target pitch. The delay is specified in milliseconds, so a typical useful value would be 5 (5 ms). This is an alternative to the automatic `pitchstabilization`. Note: `triggerdelay` disables `pitchstabilization`, as long as that is not set to 1 explicitly. If both are used at the same time, the `triggerdelay` happens before the pitch stabilization. So it is a minimum delay. |
| `lowestnote` (`ln`) | integer | `0` | With this input you can restrict the notes being played by setting a lower bound. In MIDI the notes range from 0 (C-2) to 127 (G9). By setting `lowestnote` to 24 (C0), all notes below this note are simply ignored. This allows for example for a keyboard split by using a second circuit with a `highestnote` of 23. Note gates are not being affected by this bound. |
| `highestnote` (`hn`) | integer | `127` | Sets an upper limit to the note being played, similar to `lowestnote`. Note gates are not being affected by this bound. |
| `notegate1 … notegate16` (`ng`) | gate |  | You can define up to 16 notes that can be directly controlled with a dedicated gate. This is convenient for playing drum sounds directly from triggers and also for using DROID controllers as MIDI controllers. A trigger or gate to `notegate1` will directly play the note whose pitch is set by `note1`. |
| `note1 … note16` (`n`) | integer | ☞ smart | MIDI notes to be played via `notegate`. The range is from 0 to 127. Per default the notes are set to the MIDI notes 0, 1, 2 … 15. |
| `notegatevelocity1 … notegatevelocity16` (`ngv`) | 0..1 | `1.0` | Here you can set the velocities used by the notegates. In order to keep it simple, this velocity is used for note on and note off events (nobody cares about the note off velocity anyway). If you do not use these jacks, the note gates will always use the maximum velocity. |
| `modwheel` (`w`) | 0..1 | `0.0` | Sets the current value of the modulation wheel. Any change here sends a midi CC#1 with a new value for the modulation wheel. The input range is 0.0 … 1.0 and will be converted into the MIDI range of 0 … 127. Note: in future we might support CC#33, which is the LSB value of CC#1 and increases the resolution from 128 to 16384 different values, at the cost – however – of two additional bytes being sent. |
| `volume` (`vo`) | 0..1 | `1.0` | Sets the volume of the target device. This is done by sending the MIDI CC#7 (VOLUME MSB) and MIDI CC#39 (VOLUME LSB). Using these two CCs enables a 14 bit high resolution 16384 levels (not just 127). Some devices to not react to CC#39 and simply ignore the LSB (least significant byte). The volume CV ranges from 0.0 (silent) to 1.0 (the default). |
| `pitchbend` (`pb`) | CV | `0.0` | Bends the pitches of all currently played notes up and down by a range that is configured or elsewhere defined by the device that plays our stuff. The range of this CV is -1.0 … 1.0 for covering the maximum pitch bend range. Most times that range is two semitones up and down. This CV does not behave in a 1V/oct way! |
| `pitchtracking` (`pt`) | integer | `0` | Pitch tracking is an advanced feature that allows you to track continuous changes in the incoming pitch CV while the note is already playing. It does this by listening to the input CV and converting any change into a MIDI "pitch bend" change. This feature has two limitations: First, there is just one global pitch bend value per channel, not one per note. So this feature only works in a monophonic situation. Only the value of `pitch1` is being tracked. When you play more than one note per channel, funny things might probably happen. Also the maximum range is limited by the pitch bend range of your target device. That is usually preset to 2 semitones up and down. If you can increase it, please also adapt `pitchbendrange` so this circuit knows about it. Pitch tracking has two levels: `pitchbendrange = 1` will alter the pitch of the current note within the maximum range of pitch bend and will clip any further changes. `pitchbendrange = 2`, in contrast, plays a new note if the current range is exceeded. Depending on your sound settings this "dent" might be audible or not. `0` = pitch tracking is off; `1` = just use MIDI pitch bend; `2` = use new note on larger changes. Note: When you use pitch tracking at the same time as `pitchbend`, both pitch alterations will add up. |
| `pitchbendrange` (`pbr`) | pitch | `1/6 V` | Defines the range of the effect of pitch bend at the target device on a 1V/oct base. Note: You cannot change that actual range here. You just can make sure that this circuit has the correct assumption of that range. If your target device has a configuration for extending the range, and you have set that for example to 1 octave, set `pitchbendrange` to 1 V. This allows `pitchtracking` to correctly adapt in-note pitch changes. Note: This has no effect on the `pitchbend` CV. |
| `ccnumber1 … ccnumber8` (`cn`) | integer | `0` | Specifies up to eight different CC numbers that can be continuously updated via the corresponding `cc1` through `cc8` inputs. The value needs to be an integer number from 0 to 127. |
| `cc1 … cc8` (`cc`) | 0..1 | ☞ smart | The current value of the CCs that are specified with `ccnumber1` through `ccnumber8`. The range is always from 0.0 to 1.0 (which is mapped to the number 0 to 127 on the MIDI wire). If you don't patch anything here, no CC events will be sent, of course. |
| `cctrigger1 … cctrigger8` (`ct`) | trigger |  | Usually `midiout` will send out a new CC event every time the input value of a CC has changed (with some rate limit in order not to flood the MIDI stream). When you use these inputs, an alternative method is enabled. Now CC events are created whenever a trigger arrives here. No more updates will be sent automatically. This is useful for target devices that use CCs just as messages, i.e. as one time events and not for updating a continous value. |
| `updateccs` (`uc`) | trigger |  | A trigger here sends an update for all CCs that you have in use (used `cc` inputs). Normally an update is just sent once initially and then when the input CV at one of the `cc` inputs changes its value. With the trigger you can force updates. This might be neccessary if the receiving device has lost memory of the current states of the CCs (e.g. due to a power cycle). Note: Unlike the `cctrigger` inputs, this trigger does not change the way the CC inputs work. It is just a hint for DROID that forces one additonal update. |
| `delayinitialccs` (`dc`) | CV | `1.0` | When the Droid starts it needs a short time until the X7 is operating and your PC / DAW is able to receive the MIDI events via USB. Initial CC updates during that short time period might get lost and you are missing the correct CC states (which are updated later only on changes). In order to avoid that, the Droid wait a short time after starting before it sends the first CC events. That delay can be tuned here. It is a time in seconds. |
| `bank` (`ba`) | integer | ☞ smart | Selects the current "bank". Some MIDI devices have more than 128 programs (i.e., patches, instruments, preset, etc). A MIDI Program Change message supports switching between only 128 programs. So, "Bank Select" (sometimes also called bank switch) is sometimes used to allow switching between groups of 128 programs. Bank select uses the MIDI CCs #0 (MSB) and #32 (LSB) together to form a number of 16384 different banks. The input value thus ranges from 1 to 16384. Most devices, however, restrict themselves to just 128 banks and just use the MSB (CC#0). If that is the case, you need to set `bank` to 128 for bank 2, 256 for bank 3 and so on. This can be done by simply multiplying the actual bank number with 128. |
| `program` (`pm`) | integer | ☞ smart | Select the current "program". This is a number from 1 to 128. |
| `programchange` (`pc`) | trigger |  | A trigger here will send out a "program change" MIDI message even if the value of `bank` or `program` has not changed. |
| `start` (`st`) | trigger |  | If you send a trigger here, the MIDI message START will be emitted. Don't use this jack if you also use `running`. Note: START/STOP messages are not bound to a specific channel. |
| `stop` (`sp`) | trigger |  | If you send a trigger here, the MIDI message STOP will be emitted. Don't use this jack if you also use `running`. Note: START/STOP messages are not bound to a specific channel. |
| `running` (`ru`) | gate |  | This is an alternative to the jacks `start` and `stop`. It combines both into one "running" state. When this gate input goes high, a START message is sent, when it goes low a STOP message. So you can work with a state rather than with state changes. Note: START/STOP messages are not bound to a specific channel. |
| `systemreset` (`sr`) | trigger |  | A trigger here will send the MIDI real-time message "RESET", that is supposed to bring the device into some start state. |
| `allnotesoff` (`ao`) | trigger |  | A trigger here will send the MIDI CC#123 "ALL NOTES OFF", which is essentially the same as releasing all currently held keys. |
| `allsoundoff` (`aso`) | trigger |  | A trigger here will send the MIDI CC#120 "ALL SOUND OFF", which is supposed to make the device silent as soon as possible. |
| `damper` (`dp`) | gate | `0` | This gate input simulates a hold or damper pedal. This is done via the CC#64. If the gate goes to high, a value of 127 is being sent, when it goes back to low, a value of 0. When the damper pedal is pressed, the device is supposed to hold all currently played notes and not react to any subsequent "NOTE OFF" of those notes as long as the pedal is held. When the pedal is released, all notes that had been held by the pedal should be released. |
| `portamento` (`po`) | gate | `0` | Controls the portamento pedal. The receiver is meant to activate some kind of glide effect as long as this gate is high. |
| `sostenuto` (`su`) | gate | `0` | This enables the sustain pedal. This is similar to but not exactly the same as the damper pedal as it just holds notes that are pressed while the pedal goes down. |
| `soft` (`so`) | gate | `0` | Controls the soft pedal. The receiving synth voice is meant to play notes softer while this pedal is hold down. |
| `legato` (`lg`) | gate | `0` | Controls the legato pedal, which ties subsequent notes together. |
| `clock` (`c`) | trigger |  | If you feed a steady clock here, a MIDI clock signal will be derived from this and sent through the output wire. The MIDI beat clock or simply MIDI clock is defined to send pulses at 24 PPQN: 24 pulses per quarter note. One quarter note has four 16ths, so the MIDI clock is running at 6 pulses per 16th note, and in the modular environment it is very common to work with 16th pulses as a master clock. So this `clock` jack is meant to retrieve a modular master clock, multiplies this by 6 and creates a MIDI clock from it. |
| `midiclock` (`mc`) | trigger |  | This is an alternative to `clock`: don't use both at the same time. Here you can directly send the MIDI clock in 24 PPQN. |
| `activesensing` (`as`) | gate | `1` | This is a switch that disables or enables active sensing. This is a MIDI feature where a MIDI sender emits one message of the type "active sensing" every 300 ms. The receiver can use this in order to detect if the connection is still active and also immediately reset (and turn all sound off) if it is not. Active sensing is enabled per default. You can disable it here by setting `activesensing = 0`. Note: If you have more than one `midiout` circuit sending to the same port, you should activate `activesensing` just for one of them in order to avoid useless duplicate MIDI events. |
| `updaterate` (`ur`) | CV | `50.0` | Specifies the maximum rate at which continuous controllers like the CCs, volume, pitchbend and channelpressure are updated. This limitation is necessary in order not to flood the MIDI interface with too many updates because of just minimal changes. This rate is specified in updates per second and the default is 50. A zero or negative value will completely stop all updates. Note: depending on how many events are happening on your channel, fewer updates might be possible. MIDI over a classical cable is limited to 3125 bytes per second. Events typically need 1, 2 or 3 bytes each. |
| `select` (`s`) | integer | ☞ smart | The `select` input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to 1). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | ☞ smart | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is 0, the circuit will be active if `select` is exactly 0 instead of a positive gate signal. In some cases this is more convenient. |

## Outputs

This circuit has no outputs — all output is done via MIDI.

## See also

- [`midiin`](midiin.md) — the reverse: convert incoming MIDI into CV and gates.
- [`midithrough`](midithrough.md) — forward MIDI events from input to output ports.
- [`midifileplayer`](midifileplayer.md) — play MIDI files.
