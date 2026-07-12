---
circuit: midifileplayer
title: MIDI file player
obsolete: false
ram_bytes: 6416
manual_pages: [282, 283, 284, 285, 286, 287, 288]
category: midi
tags: [midi, sd-card, file, playback, polyphony, pitch, gate, velocity, pitch-bend, mod-wheel, cc, clock, tempo, drum-trigger]
see_also: [midiin, midiout]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Plays a local SD MIDI file (no X7 needed) but requires a Standard MIDI File parser plus 8-voice allocation, roundrobin, tempo/clock scaling, CCs and note-gates.
verification_note: "The file-to-CV/gate/velocity mapping is a deterministic function of the parsed file, tempo and clock, so it can be checked headless against a reference MIDI file decoded independently; only SD-load latency behavior would need Rack."
---

# midifileplayer — MIDI file player

This circuit can read MIDI files from your Micro SD card and "play" them by
creating respective CVs for gate, pitch, velocity, pitch bend and other outputs,
which you can then route to synth voices in your modular — or do other crazy
stuff with that information.

MIDI files are organized in tracks. Each circuit of this type can play just one
track at a time. If you want to play more tracks, use more `midifileplayer`
circuits in parallel.

Just as MIDI streams, MIDI files contain *channel* information for each note and
each controller event. These channels are currently completely ignored. If you
think you can convince me that this is bad and that you have a useful
interpretation of the channels within the scope of the MIDI file player, please
let me know.

Some limitations of the current implementation are:

- Just one track can be played at a time.
- The maximum length of a track is 6000 bytes. Longer tracks cannot be loaded.
  Sorry. But this is quite long and is enough for approximately 1500 note
  events. Note: The size of the total file can be as large as you like.
- The channel information is ignored.
- Some meta events such as program change, all notes off, etc. are not yet
  recognized. Many of them just make sense in MIDI streams, not in files,
  anyway.

Features of the current implementation:

- Up to eight voices in parallel with flexible voice allocation algorithms
- Support for velocity, pitch bend, mod wheel, and global volume
- You can output the original MIDI clock from the file.
- You can adjust the tempo continuously.
- You can use external clocking (ignoring the tempo of the file).

## Getting started

Here is the simplest possible example: Copy your MIDI file to the SD card and
name it `midi1.mid`. And here is the patch that plays the first track with a
single voice:

```droid
[midifileplayer]
    pitch = O1
    gate = O2
```

Now patch `O1` to the 1V/Oct of a synth voice and `O2` to its gate. This voice
should then play the notes from the first track of the file.

The playback starts immediately when the DROID starts. Per default the track is
looped. You can restart the playback with the `reset` input. And the other way
round: you get a trigger at `endoftrack` when the playback of the track has
finished.

## Selecting file and track

You can have more than one MIDI file on your SD card. The MIDI files on the card
must be named `midi1.mid`, `midi2.mid`, and so on. Gaps are allowed. You can
have up to 9999 MIDI files that way. The last one would have the name
`midi9999.mid`. Don't use leading zeroes! The file `midi0001.mid` cannot be
played!

You can then select one of these files with the `file` parameter, so e.g.
`file = 17` would play `midi17.mid`. If you omit that, `midi1.mid` will be
played. If no such file is present on the card, nothing will be played.

A MIDI file can contain several tracks. The `track` parameter specifies the
number of the track in the file you want to play. Hereby only the non-empty
tracks will be counted. This is important since many MIDI files have tracks that
just contain meta information and no note events.

If you omit the track number, the first non-empty track will be played. If your
track number is out of range, the last track in the file will be selected.

The parameters `file` and `track` are — of course — CV controllable. So you can
switch between files and tracks by means of buttons, switches, external CV, you
name it. Whenever the file or track changes, DROID loads the selected track from
the SD card into its memory. This is also the case when the DROID starts. Also a
track change restarts playback.

Note: loading a track from the SD card might take a couple of milliseconds.
During that time DROID won't run as usual. All inputs will be ignored and all
outputs freeze. So switching at a high rate might lead to unexpected results. If
you need to have a playback started in perfect timing, use the `reset` input as
an exact trigger. If you do not want to use a trigger but rather a play/stop
gate, you can use the `speed` input for that. Setting the speed to 0 stops
playback and 1 starts it immediately.

## Polyphonic tracks

MIDI streams and files consist of *note on* and *note off* events. So there is
no length parameter in a note. It just contains the note number (in semitones)
and a velocity.

If the track contains situations where a new note starts while another one is
still on, the track is polyphonic, as you need more than one synth voice to play
correctly.

The MIDI file player allows you to define up to *eight* voices for playing
notes. Each voice consists of a `pitchX` and a `gateX` output (and an optional
`velocityX` output). By patching these outputs the player knows how many voices
are available.

If the number of simultaneous notes exceeds the number of attached voices, some
notes have to be cut off or completely omitted. You can flexibly change the
behaviour in such a situation. See the description of the parameter `dropnotes`
for details.

Here is an example for playing with up to three voices:

```droid
[midifileplayer]
    file = 2
    track = 1
    pitch1 = O1
    pitch2 = O2
    pitch3 = O3
    gate1 = G1
    gate2 = G2
    gate3 = G3
```

## Speed and Clocking

A MIDI file contains absolute timing information of when to exactly play which
note. For that purpose every note event in the file has a relative *time stamp*,
measured in ticks. The player honors this information and plays the tracks
exactly in their original speed... unless... you change it of course.

To do so you have two options. The first one is the `speed` parameter. At `1.0`
you get the original playing speed. `0.5` will play at half the speed and `2.0`
at the double speed. This can be mapped to a pot, of course (here I chose a range
from 0 to 2):

```droid
[midifileplayer]
    pitch = O1
    gate = O2
    speed = P1.1 * 2
```

Turning the pot totally CCW will completely freeze the playback.

If you need the internal clock of the MIDI player in order to synchronize with
the rest of your patch, you can get two clocks running at different resolutions
at the two outputs `clockout` and `midiclock`. See their descriptions below for
details.

The second option is clocking the player externally. In that case the tempo
information from the MIDI file is ignored. External clocking allows you to
synchronize the MIDI playback with the rest of your patch, which may contain
additional sequencers and stuff. Patch your external clock into the `clock`
input. Each clock will then play a 16ᵗʰ note's time equivalent of content:

```droid
[midifileplayer]
    pitch = O1
    gate = O2
    clock = G1
```

Note: this does *not* mean that the notes are quantized to 16th notes. You still
have the complete resolution.

## Other controls and parameters

MIDI files may contain information about pitch bend, a global volume (CC 7), the
mod wheel (CC 1) and velocity (per note). These are all available as CV outputs.
See the table of outputs for details. Most other CCs are currently not available
since they are very rarely used in MIDI files. Future versions of the MIDI file
player might give access to these.

## Error handling

When working with files, errors can happen. The MIDI file might be missing,
corrupted, whatever. In order to make life easier for you, the MIDI file player
can show you an error status at the output `error`. Write the error to an R
register that is free, that will make one of the LEDs lit up and show an error
color.

The following patch shows the errors at the LED of input 1:

```droid
[midifileplayer]
    pitch = O1
    gate = O2
    error = R1
```

Please see the table of outputs below for the various errors and their color
codes.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `file` (`f`) | integer | `1` | Number of the MIDI file to play. `7` will select `midi7.mid`. |
| `track` (`tc`) | integer | `1` | Number of the track in the file to play, starting at 1. Empty tracks do not count. Any number smaller than 1 will be interpreted as one. If the number is too big, the last track in the file is played. |
| `clock` (`c`) | trigger | | Patch an external clock here and the MIDI file will be played according to that clock. In order to be modular-friendly, this is *not* a MIDI clock but one counting the sixteenth, which is typically the step resolution of analog sequencers. This clock is then internally multiplied in order to create the necessary resolution. Note: The input `speed` has no effect when using an external clock. |
| `reset` (`r`) | trigger | | A trigger here sets the play back position to the start. |
| `loop` (`lo`) | gate | `1` | When loop mode is active (set to `1`), the track will start over again immediately when it has reached its end. This is the default. Otherwise playback stops at the end of the track. |
| `end` (`e`) | integer | `☞ smart` | If you set this value, it defines the playing end of the track. This is set in quarters as counted from the start. Setting the end beyond the end of the track will insert some pause. When unpatched, the natural end of the track is used. |
| `speed` (`sp`) | CV | `1.0` | Change the relative speed of the playback with this setting. At `1` the speed is unchanged. `1.5` makes the speed 50% faster, `0.5` plays at half speed. At `0` the playing is completely frozen. Note: `speed` is being ignored when using the input `clock`. |
| `channel` (`ch`) | integer | `☞ smart` | Only execute / play commands from a certain MIDI channel. There are 16 MIDI channels. It ranges from 1 to 16. When unpatched, all channels are played. |
| `tuningmode` (`tm`) | gate | `off` | If set to `1`, all pitch outputs will go to the CV selected for `tuningpitch` (which defaults to 2 V), and all gate outputs will play gates at 120 BPM. This helps getting all attached voices tuned when working with many voices. |
| `tuningpitch` (`tp`) | pitch | `2V` | This pitch CV will be output while the tuning mode is active. |
| `transpose` (`tr`) | pitch | `0V` | Transposes all output pitches by this value by adding the value. So in order to transpose one octave down, set this input to `-1V` or `-0.1`. Changes in the transposition are immediately reflected, even for currently already active notes. |
| `holdvelocity` (`hv`) | gate | `0` | If this is set to `1`, the velocity output for a voice will not be affected by note off events. It's just altered at the beginning of new notes. The velocity is kept after the note ends. This way during the release phase of an envelope triggered by the gate, the original velocity still lasts on. In most cases the note off velocity is set to 0, which would immediately cut off the release phase when the velocity is patched into a VCA. |
| `pitchbendrange` (`pbr`) | pitch | `1/6 V` | Sets the value to the desired maximum that `pitchbend` should output, and likewise it's negative counterpart at its minimum value. At the middle position it always outputs 0. This defaults to 2/12 V (= 1/6 V), which corresponds to one whole tone. Note: setting this to a negative value is allowed and will invert pitch bend. |
| `bendpitch` (`bp`) | gate | `1` | When set to `1` (which is the default), the pitch bend will directly be applied to all output pitches. Alternatively you can set it to `0` and use the output `pitchbend`, for using it elsewhere. |
| `roundrobin` (`rr`) | gate | `0` | Normally when looking for a free output for playing the next note, this circuit will start from `pitch1` in its search. This way, if there are not more notes than outputs at any time, the notes played first will always be played at the lowest numbered outputs. This leads to a deterministic behaviour when it comes to playing things like chords. The same voice will always be used for the first note in the stream of MIDI events. When you switch `roundrobin` to `1`, this changes. Now the outputs are scanned in a round-robin fashion, like in a rotating switch. That way every output has the same chance to get a new note. Here it can even make sense to define multiple voices even if the track is monophonic. When you use envelopes with longer release times, you can transform such a melody into chords with simultaneous notes. Note: When all outputs are currently used by a note, `roundrobin` has no influence. Here `voiceallocation` selects which of the notes will be dropped. |
| `voiceallocation` (`va`) | integer | `0` | When the MIDI stream, at any given time, needs to play more notes than you have voices assigned, normally the "oldest" notes would be cancelled. This behaviour can be configured here by setting `voiceallocation` to one of the following values: `0` — the oldest note will be cancelled (default); `1` — the new note will not be played and simply be omitted; `2` — the lowest note will be cancelled; `3` — the highest note will be cancelled. |
| `notegap` (`ngp`) | CV | `0.0` | When your MIDI devices plays a note so "long" that it lasts exactly until the next note begins — or if due to a lack of used pitch outputs one currently played note has to be replaced with a new one, the `gate` output will have no time to go low for a sufficient time between the two notes. In effect it won't trigger any envelope for the new note but will play "legato". If you don't like this, you can use `notegap`. This input specifies a number of **milliseconds** that the gate will be forced down before the new note begins. This has the drawback of introducing some latency, of course! So I suggest that you start with `notegap = 1` and then check out if your envelope is fast enough to trigger. If not, increase the value. If you are using DROID's own `contour` circuit or trigger something else internally in your patch, you can use `notegap = 0.1`. That is sufficient and introduces barely any latency. A value of `0.0` keeps the default of the legato mode. Note: the `notegap` parameter does not affect the trigger outputs. |
| `ccnumber1 ... ccnumber4` (`cn`) | integer | `0` | You can listen to up to four CCs (control changes). For example if you are interested in the current value of CC#17, set `ccnumber1 = 17` and use the output `cc1` for getting the value of CC 17. |
| `lowestnote` (`ln`) | integer | `0` | With this input you can restrict the notes being played by setting a lower bound. In MIDI the notes range from 0 (C-2) to 127 (G9). By setting `lowestnote` to 24 (C0), all notes below this note are simply ignored. This allows for example for a keyboard split by using a second circuit with a `highestnote` of 23. Note gates are not being affected by this bound. |
| `highestnote` (`hn`) | integer | `127` | Sets an upper limit to the note being played, similar to `lowestnote`. The "Notegates" are not being affected by this bound. |
| `note1 ... note16` (`n`) | integer | `☞ smart` | Selects up to 16 individual notes for which you can get a dedicated gate signal. Per default these values are set to 0 for `note1` (meaning C-2), 1 for `note2` (meaning C♯-2) and so on. For each of these notes you get a corresponding gate output (see `notegate1`, `notegate2`, etc.). These gates are high as long as the selected notes are being hold. One application is to use just one `midifileplayer` or `midiin` circuit for sequencing up to 16 drum voices. Another application is to use a MIDI keyboard or controller as a button expander — just like a P2B8 or B32. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `clockout` (`co`) | trigger | Outputs a steady clock of 1 tick per 16ᵗʰ note. |
| `midiclock` (`mc`) | trigger | Outputs a steady MIDI clock, i.e. 24 ticks per quarter note of the tune. This is 6 times faster than `clock`. |
| `endoftrack` (`et`) | trigger | Outputs a trigger when the end of the track is reached. |
| `error` (`er`) | CV | This output will be set to a value other than zero in case of an error while loading and parsing the MIDI file. This is intended for wiring it to one of the R registers. Here different errors will be displayed as different colors. Possible values of `error`: `0` — black (Everything is fine); `-1` — white (The SD card or MIDI file is missing); `1` — magenta (The file is corrupted, garbled or no MIDI file); `0.75` — orange (The file does not contain any non-empty track); `0.25` — cyan (The track is too long — max 6000 bytes are allowed). |
| `pitch1 ... pitch8` (`p`) | pitch | Pitch outputs. Since MIDI tracks can be polyphonic — i.e. play several notes at the same time — you can assign up to eight outputs here. The notes will be distributed to the defined outputs according to the settings `roundrobin` and `voiceallocation`. |
| `velocity1 ... velocity8` (`v`) | 0..1 | For each voice there is an optional velocity output, which translates the MIDI velocity into values from 0 to 1. |
| `pressure1 ... pressure8` (`pr`) | 0..1 | MIDI provides two different messages for sending "after-touch" information, i.e. information about how strong a key is pressed down after the initial hit. Some keyboards just have one pressure sensor in total and send the current maximum pressure information of all keys in one message ("channel pressure"). Others have one pressure sensor per key and send "polyphonic key pressure" messages. This circuit maps both to a `pressure` output per note that is being played. So if your keyboard (or sequencer or DAW or whatever) sends polyphonic key pressure events and you use multiple `pitchX` outputs, wire the individual `pressureX` outputs to wherever you like. Otherwise you can simply use `pressure1` for all notes (which can be abbreviated with `pressure`), since it is the same for all note outputs anyway. `pressure` outputs a value from 0 to 1. |
| `gate1 ... gate8` (`g`) | gate | Gate outputs for the up to eight simultaneous note outputs. |
| `trigger1 ... trigger8` (`t`) | trigger | Trigger outputs for the up to eight simultaneous note outputs. The difference to the gate outputs is, that these just send a short trigger of 5 ms at the start of the note. This can be interesting in situations where the notes have no *gaps* in between so that gate will never go low. |
| `cc1 ... cc4` () | 0..1 | Outputs the current value of the four CC number that are defined with the inputs `ccnumber1 ... ccnumber4`. CCs have a range from 0 to 127, but this is converted in the range 0.0 .. 1.0 here, in order to make it easier to use that as a CV. If you need the raw number, multiply the output with 127. Note: as long as no CC message with the selected number happened, this output will be set to 0. |
| `cctrigger1 ... cctrigger4` (`ct`) | trigger | These outputs send a trigger whenever a CC event matching the corresponding `ccnumber` is processed. Some devices uses triggers in such a way — as events rather then indicating the change of a continuous value. So if you set `ccnumber2 = 17`, the output `cctrigger2` sends a trigger whenever CC#17 is received. |
| `notegate1 ... notegate16` (`ng`) | gate | Outputs a high gate whenever the corresponding note (which is selected by `note1` through `note16`) is currently being played. |
| `notegatevelocity1 ... notegatevelocity16` (`ngv`) | gate | When a note on event is sent using `notegate…`, the corresponding velocity is available at this output. A note off event resets the output velocity to 0. |
| `pitchbend` (`pb`) | CV | Outputs the current pitch bend value as a bipolar voltage. The range can be set with `pitchbendrange`. |
| `programchange` (`pc`) | trigger | Sends a trigger whenever a *MIDI program change* message arrives. Just before sending the trigger sets `program` to the new program number (something from 0 to 127). Note: This trigger is also being output when the program change messages sends the same program number as previously, i.e. if there is no actual *change*. |
| `program` (`pm`) | integer | The number of the last program change. This starts at `0`. |
| `bank` (`ba`) | integer | Outputs the number of the currently selected bank — from 0 to 16384. MIDI defines the MSB of the bank to be changed with CC#0 and the LSB with CC#32. That means if you just use CC#0, you will only be able to select the banks 0, 128, 256, and so on. As long as no bank select CC has been received, `bank` will output 0. |
| `modwheel` (`w`) | 0..1 | Output the current state of the mod wheel level — within the range from 0.0 to 1.0. The mod wheel is changed by MIDI control change 1. |
| `volume` (`vo`) | 0..1 | Outputs the current global volume as set by MIDI control change 7. |
| `portamento` (`po`) | gate | This output gives you access to the current state of the "portamento pedal" (MIDI CC 65). You can use it to enable an external slew circuit for creating portamento effects. |
| `soft` (`so`) | gate | This output gives you access to the current state of the "soft pedal" (MIDI CC 67). It is `1` while the pedal is hold and `0` otherwise. |

## See also

- [`midiin`](midiin.md) — plays a live MIDI stream instead of a file; shares
  most of the same voice, note-gate and controller outputs.
- [`midiout`](midiout.md) — sends MIDI events out of the DROID.
