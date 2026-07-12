---
circuit: midiin
title: MIDI to CV converter
obsolete: false
ram_bytes: 592
manual_pages: [289, 290, 291, 292, 293, 294, 295, 296]
category: midi
tags: [midi, cv, gate, trigger, note, velocity, pitchbend, clock, cc, control-change, polyphony, roundrobin, voiceallocation, notegate, pedal, x7, usb, trs, port, channel]
see_also: [midiout, midifileplayer, midithrough, slew, clocktool, button, sequencer]
impl_difficulty: moderate
controller_binding: x7-required
verification: rack-automated
spec_gap: false
difficulty_note: Standard MIDI-to-CV and VCV has MIDI infrastructure, but the breadth (polyphonic voice allocation, roundrobin, pitchbend, CC, clock, pedal, port/channel selection) makes it more than trivial. Needs an X7.
verification_note: "Rack-automated: drive a virtual MIDI source via the vcv-rack MCP server and read the pitch/gate/velocity/clock output jacks. Partial — note-to-CV and voice-allocation logic can be unit-tested headlessly if factored out of the MIDI I/O path, but TRS/USB port selection and live routing need Rack."
---

# midiin — MIDI to CV converter

This circuit converts incoming MIDI data into CV, gate and trigger signals. It
needs the X7 expander in order to work (see the manual
([hardware](../hardware.md)) for general information about the X7).

There are various useful applications of this circuit, some of which are:

- Attaching an external keyboard to your modular.
- Using an external hardware sequencer for playing melodies and beats in your
  modular.
- Use an external MIDI controller to influence your DROID patch.
- Use your phone or tablet as a MIDI controller to influence your patch (via
  USB).
- Connect two DROIDs (both with X7) and exchange real time data.

The X7 MIDI implementation is very comprehensive and gives you convenient access
to most of the MIDI features. Please refer to the table of inputs and outputs
for details. Here are just some very basic examples.

## Basic operation

The basic operation is quite simple. Per default `midiin` listens on all
available ports, both TRS and USB. The first port to receive MIDI data is used.
The following example controls one synth voice by converting MIDI note on / note
off messages into CV / gate signals:

```droid
[midiin]
    pitch = O1
    gate = O2
```

It's really as simple as that! Connect your MIDI keyboard or sequencer with the
X7 MIDI input, wire `O1` to the 1V/Oct input of a synth voice and `O2` to its
gate input and enjoy your music!

You can also precisely specify which ports to receive data from. All details are
explained below.

## Polyphonic patches

Do you have more than one synth voice to control? Then you can play several
notes at the same time by using up to *eight* `pitch` and `gate` outputs. Here
is an example with three voices, which uses a G8 expander for the gates:

```droid
[midiin]
    pitch1 = O1
    pitch2 = O2
    pitch3 = O3
    gate1 = G1
    gate2 = G2
    gate3 = G3
```

Here the parameters `roundrobin` and `voiceallocation` are interesting.
`roundrobin` influences which of the three outputs should be used for the next
note, in situations where more than one is free. `voiceallocation`, in contrast,
controls what should happen if the MIDI stream wants to play more simultaneous
notes than you have setup in `midiin`. The default is to cancel the oldest
currently playing note, but you can change that behaviour in various ways.

## Sequencing drums and triggers

When you use a MIDI sequencer for triggering drums, often each drum voice (bass
drum, snare drum, etc.) is triggered by a certain note, for example C-2 for the
bass drum, C♯-2 for the snare drum and so on. In this case it is more convenient
to use the `notegate` outputs. Check the following example:

```droid
[midiin]
    note1 = 24
    note2 = 25
    notegate1 = O1
    notegate2 = O2
    notegatevelocity1 = O3
    notegatevelocity2 = O4
```

Now whenever note 24 is played by the sequencer, `notegate1` will trigger. The
note numbers range from 0 to 127, with 0 being the lowest note and 127 the
highest. The MIDI standard specifies that note 0 is usually C-2 (two octaves
below C0). So note 24 would be C0 and note 25 C♯0.

The velocities of the two notes are available at `O3` and `O4` at a range of
0 V … 10 V.

Another application of note gates is to use keys on a MIDI keyboard or touch pads
of a MIDI controller as buttons in your patch! In fact the [`button`](button.md)
circuit can be wired to such note gates. It's just that you don't have a
corresponding LED. But you can use the DROID's own LEDs for that.

The following example uses the note 24 in order to toggle a (virtual) button and
use the first input LED of the master as LED for the button:

```droid
[midiin]
    note1 = 24
    notegate1 = _NOTE24

[button]
    button = _NOTE24
    led = R1
    output = _SOMETHING # ...
```

Please note: [`midiout`](midiout.md) has similar `note1 … note8` inputs. But
there the pitches are specified in 1V/Oct. So don't mix them up!

## Start, Stop and Clock

MIDI sequencers usually send a steady MIDI clock at 24 PPQ, which means 24
pulses per quarter note, which in turn means 6 pulses per 16th note, which is the
typical clock speed for modular systems. But also 48 PPQ and 96 PPQ are
possible.

You get easy access to the clock by various clock outputs running at different
speeds. The jack labelled just `clock` outputs the 16th note clock. The
following example just sends that clock to the `O1` output:

```droid
[midiin]
    clock = O1
```

Hereby it is assumed that the MIDI clock is running at 24 PPQ. If its running
faster, simply use one of the other clock outputs, which divides down the clock.
Or use [`clocktool`](clocktool.md) for dividing yourself.

Also the START and STOP messages of MIDI sequencers are accessible, either as
two separate triggers, or as a running state. For example you can use the
`start` output as a reset signal for some DROID circuit:

```droid
[midiin]
    clock = _CLOCK
    start = _RESET

[sequencer]
    clock = _CLOCK
    reset = _RESET
    ...
```

## Getting CCs

MIDI does not only transport note events but also *controllers*. Most of these
are continuous values, much like CVs. `midiin` gives you access to the current
value of a couple of standard controllers like `volume` and `modwheel` with
dedicated outputs. And in addition up to four custom CCs can be output. All such
controllers are converted into values from 0 to 1 (or 0 V to 10 V if you output
them directly):

```droid
[midiin]
    volume = O1
    modwheel = O2
    ccnumber1 = 10 # get update from CC#10
    cc1 = O3 # send current CC value to O3
```

## Using multiple midiins

You are not restricted to one `midiin` circuit but can use up to 32 of these in
your patch. There are different reasons why multiple ones can be useful, e.g.:

- You want to control different voices from different MIDI channels
- You want to fetch more than four CCs.

All `midiin` circuits will get their own copy of the MIDI data stream and can do
their own things with it. You might want to use `channel = ...` in order to just
get only the events of a specific MIDI channel.

## Pedals

The MIDI standard defines five different types of foot pedals. The state of
these – up or down – is transmitted by means of five different control changes
(CCs). `midiin` automatically interpretes them corresponding to their intended
meaning as follows:

- **Damper pedal (CC 64):** While down, notes still linger on, even if they end.
  Internally, the "note off" event of all notes will be delayed until the pedal
  is up. This pedal is sometimes also called "sustain pedal", since it makes
  notes sustain.
- **Portamento pedal (CC 65):** Sets the `portamento` output to 1 while down.
  You can use that output for enabling a slew limiter with the circuit
  [`slew`](slew.md).
- **Sostenuto pedal (CC 66):** Sostenuto is the smarter version of sustain. Such
  a pedal is found as the middle of three pedals on grand pianos. When it goes
  down, all notes that are *currently played* are sustained as long as the pedal
  is held. But *new* notes, that start during that period, are *not* sustained.
  That's the difference. The `midiin` circuit automatically makes CC 66 behave
  in exactly that way. That, of course, just makes sense in a polyphonic patch,
  where you have enough voices that can play the sustained notes.
- **Soft pedal (CC 67):** Sets the `soft` output to 1 while held.
- **Legato pedal (CC 68):** While down, ties consequtive notes together by
  keeping `gate` at 1 between notes.

## Port selection

The `midiin` circuit can receive up to two MIDI streams in parallel – one from a
USB jack and one from a TRS jack. Merging together multiple streams from two USB
jacks or two TRS jacks is currently not possible.

The two inputs `usb` and `trs` define from which physical ports (jacks) MIDI
data should be processed. TRS stands for "tip ring sleeve". By this we mean the
3.5 mm MIDI input jack on the X7 or the MASTER18.

For each of the jack types "USB" and "TRS" you can choose one of three options:

- process data from a specific port
- auto detection: process data from the port that has received MIDI input first
- don't process data

For processing data from a specific port, specify `trs =` or `usb =` with the
number of the port. The numbers depend on your hardware configuration.

**MASTER + X7:**

| Setting | Meaning |
|---------|---------|
| `usb = 1` | USB port on the X7 |
| `trs = 1` | TRS port on the X7 |

**MASTER18:**

| Setting | Meaning |
|---------|---------|
| `usb = 1` | USB port on the MASTER18 |
| `trs = 1` | TRS port MIDI1 on the MASTER18 |
| `trs = 2` | TRS port MIDI2 on the MASTER18 |

**MASTER18 + X7:**

| Setting | Meaning |
|---------|---------|
| `usb = 1` | USB port on the MASTER18 |
| `usb = 2` | USB port on the X7 |
| `trs = 1` | TRS port MIDI1 on the MASTER18 |
| `trs = 2` | TRS port MIDI2 on the MASTER18 |
| `trs = 3` | TRS port on the X7 |

Auto detection is selected by using the port number 10. In this case DROID
watches all ports (of the selected type) for incoming MIDI data. As soon as it
sees data on one port it locks in to this port and ignores all other ports for
the while. After no MIDI data is seen on the locked port for a couple of
seconds, auto detection happens again, so you can e.g. unplug a device from
port 1 and plug it into port 2 and after a couple of seconds things work again.

By setting `trs = 0` or `usb = 0`, this type of port is deactivated.

If you just specify one of `trs` and `usb`, the other one will be deactivated –
unless you set the port to 0, which sets the other port to auto detection.
Sounds complicated? It's done that way to meet most peoples expectations! Let's
view a couple of examples.

Guess you have a MASTER and an X7. The default is to do auto detection on both
ports. For this you don't need to specify neither `trs` nor `usb`:

```droid
[midiin]
    pitch = O1
    gate = O2
    ...
```

The next example just processes data from the USB port of the X7:

```droid
[midiin]
    usb = 1
```

Process just data from the TRS port:

```droid
[midiin]
    trs = 1
```

Deactivating USB automatically sets TRS to auto detection, if `trs` is omitted.
So the following example does the same, since the X7 has just one TRS port:

```droid
[midiin]
    usb = 0 # disable USB, enable TRS
```

Now let's assume that you have a MASTER18 plus X7. The following example just
processes data from the X7's TRS port:

```droid
[midiin]
    trs = 3
```

And here is how to process data from the USB port on the MASTER18 plus data from
that TRS port that receives MIDI data first:

```droid
[midiin]
    usb = 1
    trs = 10 # auto detection
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `trs` (``) | integer | ☞ smart | Selects a TRS port to use (3.5 mm jack). `trs = 0` disables TRS, `trs = 10` enables auto detection. See the port selection section above for details. When left unpatched, ports default to auto detection (see the port selection rules). |
| `usb` (``) | integer | ☞ smart | Selects a USB port to use. `usb = 0` disables USB, `usb = 10` enables auto detection. See the port selection section above for details. When left unpatched, ports default to auto detection (see the port selection rules). |
| `initialrunning` (`ir`) | integer | `2` | This parameter sets which "running" state is assumed when your DROID starts. The idea behind this parameter is, that at this point of time you cannot know the real running state of the MIDI stream, since e.g. the DROID might have started after the sequencer at the sending end of the line. You have three ways to set this: start in stopped state (`0`), start in running state (`1`) and an inbetween "automatic" mode (`2`). In the auto mode, you start in stopped state but automatically switch to running as soon as a note on event is received. At that moment a MIDI START event is simulated. Note: as this parameter is just read once at the absolute system start, you cannot assign a dynamic CV input or control here. |
| `systemreset` (`sr`) | trigger | | A trigger here resets the whole MIDI state of this circuit. It does the same as a MIDI RESET message: It stops all playing notes, resets the controllers, the states of the pedals and so on. |
| `channel` (`ch`) | integer | ☞ smart | Only execute / play commands from a certain MIDI channel. There are 16 MIDI channels. It ranges from 1 to 16. When left unpatched, all channels are processed. |
| `tuningmode` (`tm`) | gate | `off` | If set to 1, all pitch outputs will go to the CV selected for `tuningpitch` (which defaults to 2 V), and all gate outputs will play gates at 120 BPM. This helps getting all attached voices tuned when working with many voices. |
| `tuningpitch` (`tp`) | pitch | `2V` | This pitch CV will be output while the tuning mode is active. |
| `transpose` (`tr`) | pitch | `0V` | Transposes all output pitches by this value by adding the value. So in order to transpose one octave down, set this input to `-1V` or `-0.1`. Changes in the transposition are immediately reflected, even for currently already active notes. |
| `holdvelocity` (`hv`) | gate | `0` | If this is set to 1, the velocity output for a voice will not be affected by note off events. It's just altered at the beginning of new notes. The velocity is kept after the note ends. This way during the release phase of an envelope triggered by the gate, the original velocity still lasts on. In most cases the note off velocity is set to 0, which would immediately cut off the release phase when the velocity is patched into a VCA. |
| `pitchbendrange` (`pbr`) | pitch | `1/6 V` | Sets the value to the desired maximum that `pitchbend` should output, and likewise it's negative counterpart at its minimum value. At the middle position it always outputs 0. This defaults to 2⁄12 V, which corresponds to one whole tone. Note: setting this to a negative value is allowed and will invert pitch bend. |
| `bendpitch` (`bp`) | gate | `1` | When set to 1 (which is the default), the pitch bend will directly be applied to all output pitches. Alternatively you can set it to 0 and use the output `pitchbend`, for using it elsewhere. |
| `roundrobin` (`rr`) | gate | `0` | Normally when looking for a free output for playing the next note, this circuit will start from `pitch1` in its search. This way, if there are not more notes than outputs at any time, the notes played first will always be played at the lowest numbered outputs. This leads to a deterministic behaviour when it comes to playing things like chords. The same voice will always be used for the first note in the stream of MIDI events. When you switch `roundrobin` to 1, this changes. Now the outputs are scanned in a round-robin fashion, like in a rotating switch. That way every output has the same chance to get a new note. Here it can even make sense to define multiple voices even if the track is monophonic. When you use envelopes with longer release times, you can transform such a melody into chords with simultaneous notes. Note: When all outputs are currently used by a note, `roundrobin` has no influence. Here `voiceallocation` selects which of the notes will be dropped. |
| `voiceallocation` (`va`) | integer | `0` | When the MIDI stream, at any given time, needs to play more notes than you have voices assigned, normally the "oldest" notes would be cancelled. This behaviour can be configured here by setting `voiceallocation` to one of the following values: `0` the oldest note will be cancelled (default); `1` the new note will not be played and simply be omitted; `2` the lowest note will be cancelled; `3` the highest note will be cancelled. |
| `notegap` (`ngp`) | CV | `0.0` | When your MIDI device plays a note so "long" that it lasts exactly until the next note begins – or if due to a lack of used pitch outputs one currently played note has to be replaced with a new one, the `gate` output will have no time to go low for a sufficient time between the two notes. In effect it won't trigger any envelope for the new note but will play "legato". If you don't like this, you can use `notegap`. This input specifies a number of **milliseconds** that the gate will be forced down before the new note begins. This has the drawback of introducing some latency, of course! So I suggest that you start with `notegap = 1` and then check out if your envelope is fast enough to trigger. If not, increase the value. If you are using DROID's own [`contour`](contour.md) circuit or trigger something else internally in your patch, you can use `notegap = 0.1`. That is sufficient and introduces barely any latency. A value of `0.0` keeps the default of the legato mode. Note: the `notegap` parameter does not affect the `trigger` outputs. |
| `ccnumber1 … ccnumber4` (`cn`) | integer | `0` | You can listen to up to four CCs (control changes). For example if you are interested in the current value of CC#17, set `ccnumber1 = 17` and use the output `cc1` for getting the value of CC 17. |
| `lowestnote` (`ln`) | integer | `0` | With this input you can restrict the notes being played by setting a lower bound. In MIDI the notes range from 0 (C-2) to 127 (G9). By setting `lowestnote` to 24 (C0), all notes below this note are simply ignored. This allows for example for a keyboard split by using a second circuit with a `highestnote` of 23. Note gates are not being affected by this bound. |
| `highestnote` (`hn`) | integer | `127` | Sets an upper limit to the note being played, similar to `lowestnote`. The "Notegates" are not being affected by this bound. |
| `note1 … note16` (`n`) | integer | ☞ smart | Selects up to 16 individual notes for which you can get a dedicated gate signal. Per default these values are set to 0 for `note1` (meaning C-2), 1 for `note2` (meaning C♯-2) and so on. For each of these notes you get a corresponding gate output (see `notegate1`, `notegate2`, etc.). These gates are high as long as the selected notes are being hold. One application is to use just one [`midifileplayer`](midifileplayer.md) or `midiin` circuit for sequencing up to 16 drum voices. Another application is to use a MIDI keyboard or controller as a button expander – just like a P2B8 or B32. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `clock` (`c`) | gate | If the MIDI sender sends a MIDI clock, you get a 16th note clock output here. This is the same as the `clock16` jack and just a convenient abbreviation. |
| `clock8` (`c8`) | gate | Gets an 8th clock here (like `clock` divided by 2). |
| `clock8t` (`c8t`) | gate | Gets an 8th triplets clock here. This is faster than `clock8` but slower than `clock`. |
| `clock16` (`c16`) | gate | The same as `clock`: a clock running at 16th notes. |
| `clock4` (`c4`) | gate | A clock at the speed of quarter notes. |
| `midiclock` (`mc`) | gate | Here you get the original MIDI clock. This is 6 times faster than `clock` and 24 times faster than `clock4`. This is because the MIDI clock is specified to run at 24 PPQ, i.e. 24 pulses per quarter note. |
| `start` (`st`) | trigger | This jack sends a trigger when a MIDI START message arrives. |
| `continue` (`co`) | trigger | This jack sends a trigger when a MIDI CONTINUE message arrives. |
| `stop` (`sp`) | trigger | This jack sends a trigger when a MIDI STOP message arrives. |
| `running` (`ru`) | gate | This jack remembers the current running state according to previous START and STOP messages. |
| `active` (`a`) | gate | If the sending device supports active sensing, this output is high as long as a device is connected. Otherwise its high if at least one MIDI message has been received. |
| `pitch1 … pitch8` (`p`) | pitch | Pitch outputs. Since MIDI tracks can be polyphonic – i.e. play several notes at the same time – you can assign up to eight outputs here. The notes will be distributed to the defined outputs according to the settings `roundrobin` and `voiceallocation`. |
| `velocity1 … velocity8` (`v`) | 0..1 | For each voice there is an optional velocity output, which translates the MIDI velocity into values from 0 to 1. |
| `pressure1 … pressure8` (`pr`) | 0..1 | MIDI provides two different messages for sending "after-touch" information, i.e. information about how strong a key is pressed down after the initial hit. Some keyboards just have one pressure sensor in total and send the current maximum pressure information of all keys in one message ("channel pressure"). Others have one pressure sensor per key and send "polyphonic key pressure" messages. This circuit maps both to a `pressure` output per note that is being played. So if your keyboard (or sequencer or DAW or whatever) sends polyphonic key pressure events and you use multiple `pitchX` outputs, wire the individual `pressureX` outputs to wherever you like. Otherwise you can simply use `pressure1` for all notes (which can be abbreviated with `pressure`), since it is the same for all note outputs anyway. `pressure` outputs a value from 0 to 1. |
| `gate1 … gate8` (`g`) | gate | Gate outputs for the up to eight simultaneous note outputs. |
| `trigger1 … trigger8` (`t`) | trigger | Trigger outputs for the up to eight simultaneous note outputs. The difference to the gate outputs is, that these just send a short trigger of 5 ms at the start of the note. This can be interesting in situations where the notes have no gaps in between so that gate will never go low. |
| `cc1 … cc4` (``) | 0..1 | Outputs the current value of the four CC number that are defined with the inputs `ccnumber1 … ccnumber4`. CCs have a range from 0 to 127, but this is converted in the range 0.0 … 1.0 here, in order to make it easier to use that as a CV. If you need the raw number, multiply the output with 127. Note: as long as no CC message with the selected number happened, this output will be set to 0. |
| `cctrigger1 … cctrigger4` (`ct`) | trigger | These outputs send a trigger whenever a CC event matching the corresponding `ccnumber` is processed. Some devices uses triggers in such a way – as events rather then indicating the change of a continous value. So if you set `ccnumber2 = 17`, the output `cctrigger2` sends a trigger whenever CC#17 is received. |
| `notegate1 … notegate16` (`ng`) | gate | Outputs a high gate whenever the corresponding note (which is selected by `note1` through `note16`) is currently being played. |
| `notegatevelocity1 … notegatevelocity16` (`ngv`) | gate | When a note on event is sent using `notegate…`, the corresponding velocity is avalable at this output. A note off event resets the output velocity to 0. |
| `pitchbend` (`pb`) | CV | Outputs the current pitch bend value as a bipolar voltage. The range can be set with `pitchbendrange`. |
| `programchange` (`pc`) | trigger | Sends a trigger whenever a MIDI program change message arrives. Just before sending the trigger it sets `program` to the new program number (something from 0 to 127). Note: This trigger is also being output when the program change message sends the same program number as previously, i.e. if there is no actual *change*. |
| `program` (`pm`) | integer | The number of the last program change. This starts at 0. |
| `bank` (`ba`) | integer | Outputs the number of the currently selected bank – from 0 to 16384. MIDI defines the MSB of the bank to be changed with CC#0 and the LSB with CC#32. That means if you just use CC#0, you will only be able to select the banks 0, 128, 256, and so on. As long as no bank select CC has been received, `bank` will output 0. |
| `modwheel` (`w`) | 0..1 | Output the current state of the mod wheel level – within the range from 0.0 to 1.0. The mod wheel is changed by MIDI control change 1. |
| `volume` (`vo`) | 0..1 | Outputs the current global volume as set by MIDI control change 7. |
| `portamento` (`po`) | gate | This output gives you access to the current state of the "portamento pedal" (MIDI CC 65). You can use it to enable an external slew circuit for creating portamento effects. |
| `soft` (`so`) | gate | This output gives you access to the current state of the "soft pedal" (MIDI CC 67). It is 1 while the pedal is hold and 0 otherwise. |

## See also

- [`midiout`](midiout.md) — CV to MIDI; the reverse direction (note there `note1 … note8` are specified in 1V/Oct).
- [`midifileplayer`](midifileplayer.md) — plays back MIDI files, with the same style of note/drum outputs.
- [`midithrough`](midithrough.md) — forwards a MIDI stream unchanged.
- [`slew`](slew.md) — slew limiter that pairs with the `portamento` output.
- [`clocktool`](clocktool.md) — for dividing/manipulating the clock outputs.
- [`button`](button.md) — can be driven by `notegate` outputs to turn MIDI keys into virtual buttons.
