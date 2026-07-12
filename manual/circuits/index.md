# DROID circuit reference — index

Searchable index of all **76** circuits in chapter 16 of the DROID manual (firmware blue-7). Each circuit has its own file in this directory.

- Search by **name** or **keyword**: use the master table below (Ctrl-F on tags).
- Browse by **function**: see the category sections.
- `RAM` = bytes of the master's ~110 000 free bytes consumed by one instance.

## Jack type legend

Every circuit's Inputs/Outputs tables use these `Type` tokens (the manual prints them as glyphs):

| Token | Meaning |
|-------|---------|
| `CV` | Continuous control voltage, full range −10 V … +10 V. |
| `pitch` | Precise 1 V/octave pitch (for V/Oct in/outputs). |
| `0..1` | Fraction/percentage 0.0 … 1.0 (values are clamped). |
| `0..1 (mid 0.5)` | Same 0.0 … 1.0 range but with 0.5 as the neutral/rest value. |
| `integer` | Whole numbers (non-integers are rounded). |
| `stepped` | CV that only takes discrete steps. |
| `gate` | On/off: 0 (0 V) or 1 (10 V). |
| `trigger` | Brief pulse; only the rising edge matters. |
| `text` | Text parameter in double quotes (ASCII only). |

In a **Default** column, `☞ smart` marks an intelligent automatic behaviour that applies when the jack is left unpatched (explained in that jack's description).

## Frontmatter field legend

Each `circuits/<name>.md` opens with YAML frontmatter. The **descriptive** fields
document the circuit as-is; the **implementation-assessment** fields rate how hard
each circuit is to emulate in the `droid-master` VCV Rack module and how its
implementation can be verified.

**Descriptive fields**

| Field | Meaning |
|-------|---------|
| `circuit` | The circuit's `[name]` as written in a patch. |
| `title` | Human-readable name (matches the manual heading). |
| `obsolete` | `true` if deprecated (⚠️); avoid in new patches. |
| `ram_bytes` | Bytes of the master's ~110 000 free bytes one instance consumes. |
| `manual_pages` | Source-PDF page numbers for this circuit. |
| `category` | Functional grouping (see *By function* below). |
| `tags` | Searchable keywords. |
| `see_also` | Related circuits. |

**Implementation-assessment fields** (for the VCV Rack emulation)

`impl_difficulty` — how hard to emulate the circuit faithfully:

| Value | Meaning |
|-------|---------|
| `not-feasible` | Doesn't make sense in VCV Rack or can't be done (e.g. controls external hardware). Has **no** `verification`, `verification_note`, or `spec_gap` fields — only `difficulty_note`. |
| `easy` | Spec is complete and straightforward; implementation is direct. |
| `moderate` | Implementable and testable but non-trivial — breadth, internal state, or moderate spec gaps. |
| `hard` | Complex, or difficult to verify: incomplete spec, sparse reference data, or hardware behaviour with no VCV analog. |

`controller_binding` — what hardware the circuit needs beyond the master:

| Value | Meaning |
|-------|---------|
| `master-only` | Runs and can be tested with just the master module. |
| `x7-required` | Needs the **X7** MIDI/USB expander. Called out separately from other controllers because it is the MIDI/USB bridge — the likely home for MIDI functionality in the emulation — and behaves unlike the pot/button/fader controllers. |
| `controller-required` | Needs a specific UI controller/expander (M4, E4, DB8E, P-/B-/S- controllers, G8, …) to function at all. |
| `controller-enhanced` | Runs master-only but gains features when a controller is present. |

`verification` — how the implementation can be checked (omitted when `not-feasible`):

| Value | Meaning |
|-------|---------|
| `headless` | An agent can verify independently, without running VCV Rack (compute expected outputs and compare). |
| `rack-automated` | Needs Rack, but the [vcv-rack MCP server] can drive inputs and read outputs with little or no human involvement. |
| `requires-human` | Needs a person running Rack — haptics, subjective feel, or unclear specs / missing reference data. `verification_note` says which. |

`spec_gap` — `true` when faithful implementation **or** verification is currently
blocked or degraded by *unclear specs or missing reference data* (as opposed to an
inherent hardware limit). This class of gap is often independently solvable by
gathering reference data from real hardware, so it is flagged separately to be
queryable. `false` when the manual is sufficient. Omitted when `not-feasible`.

`difficulty_note` — one line on why that difficulty tier was chosen.

`verification_note` — how to verify the circuit; notes **partial** coverage (e.g.
"90% headless, one edge case needs Rack") and, where relevant, restates the
`spec_gap` reasoning (what data would close the gap).

## All circuits (alphabetical)

| Circuit | Function | Category | Impl | RAM | Pages | Keywords |
|---------|----------|----------|------|-----|-------|----------|
| [`adc`](adc.md) | AD Converter with 12 bits | conversion | easy | 56 | 125–126 | converter, binary, bits, gate, analog-to-digital, resolution |
| [`algoquencer`](algoquencer.md) | Algorithmic sequencer | sequencer | hard | 888 | 127–139 | sequencer, drum, trigger, turing-machine, random, algorithmic, accents, fills |
| [`arpeggio`](arpeggio.md) | Arpeggiator – pattern based melody generator | sequencer | moderate | 144 | 140–150 | arpeggiator, melody, pattern, pitch, scale, root, degree, interval |
| [`bernoulli`](bernoulli.md) | Random gate distributor | random | easy | 32 | 151 | random, bernoulli, gate, distributor, probability, coin-toss, routing |
| [`burst`](burst.md) | Generate burst of pulses | clock-timing | easy | 40 | 152–153 | burst, pulses, triggers, clock, taptempo, trigger-delay, ratchet, count |
| [`button`](button.md) | Does all sorts of useful things with buttons | controller-ui | moderate | 104 | 154–159 | button, toggle, momentary, longpress, shortpress, double-click, led, states |
| [`buttongroup`](buttongroup.md) | Connected buttons | controller-ui | moderate | 440 | 160–163 | buttons, radio-buttons, led, user-interface, toggle, select, layers, overlay |
| [`calibrator`](calibrator.md) | VCO Calibrator | quantizer-pitch | hard | 232 | 164–167 | vco, calibration, tuning, tracking, pitch, correction, octave, nudge |
| [`case`](case.md) | Switch choosing from inputs via conditions | switch-routing | easy | 88 | 168 | switch, select, condition, priority, precedence, clock-source, routing, fallback |
| [`chord`](chord.md) | Chord generator | chord-harmony | moderate | 144 | 169–176 | chord, harmony, pitch, voices, scale, root, degree, inversion |
| [`clockedtrigger`](clockedtrigger.md) | Delay a trigger until the next clock tick | clock-timing | easy | 40 | 177 | trigger, clock, sync, sample-and-hold, delay, quantize, button, pending |
| [`clocktool`](clocktool.md) | Clock divider / multiplier / shifter | clock-timing | moderate | 96 | 178–180 | clock, divider, multiplier, duty-cycle, gate-length, delay, shift, timing |
| [`compare`](compare.md) | Compare two values | logic-math | easy | 32 | 181–182 | compare, comparator, less, greater, equal, precision, decision, switch |
| [`contour`](contour.md) | Contour generator | envelope-lfo | moderate | 112 | 183–187 | envelope, adsr, contour, attack, decay, sustain, release, predelay |
| [`copy`](copy.md) | Copy a signal, while applying attenuation and offset | utility | easy | 24 | 188–188 | copy, attenuate, offset, scaling, mixer, adder, precision-adder, utility |
| [`crossfader`](crossfader.md) | Morph between 8 inputs | mixer-cv | easy | 40 | 189–189 | crossfader, crossfade, morph, mix, fade, blend, eight-inputs, utility |
| [`cvlooper`](cvlooper.md) | Clocked CV looper | sample-record | moderate | 17336 | 190–192 | looper, cv-looper, loop, record, overdub, overlay, tape, clock |
| [`dac`](dac.md) | DA Converter with 12 bits | conversion | easy | 56 | 193–194 | converter, binary, bits, digital-to-analog, resolution, gate, msb, lsb |
| [`delay`](delay.md) | A tape delay for CVs, gates and numbers | utility | moderate | 1672 | 195–197 | delay, tape, cv, gate, number, trigger-delay, clock, virtual-tape |
| [`detune`](detune.md) | Detune multiple voices in a most disharmonic way | quantizer-pitch | moderate | 56 | 198 | detune, disharmonic, voices, pitch, chord, sinfonion, tuning |
| [`display`](display.md) | Display texts and change parameters on a DB8E | controller-ui | moderate | 56 | 199–204 | display, db8e, text, value, oled, header, screensaver, takeovercontrols |
| [`droid`](droid.md) | General DROID controls | utility | moderate | 88 | 205–207 | configuration, led-brightness, motor-faders, m4, calibration, firmware-upgrade, low-pass-filter, smoothing |
| [`encoder`](encoder.md) | Provide access to a knob on the E4 or DB8E controller | controller-ui | moderate | 184 | 213–222 | encoder, e4, db8e, knob, led-ring, virtual-value, discrete, infinity |
| [`encoderbank`](encoderbank.md) | Create bank of up to 8 virtual input knobs from E4 encoders | controller-ui | moderate | 744 | 208–212 | encoder, e4, virtual-knob, led-ring, db8e, display, notch, snapto |
| [`encoquencer`](encoquencer.md) | Performance sequencer using E4 encoders | sequencer | hard | 1352 | 223–242 | sequencer, encoder, e4, performance, motoquencer, ratchets, repeats, gate-pattern |
| [`euklid`](euklid.md) | Euclidean rhythm generator | clock-timing | easy | 48 | 243–244 | euclidean, rhythm, trigger, pattern, beats, offbeats, clock, gate |
| [`explin`](explin.md) | Exponential to linear converter | conversion | easy | 32 | 245–246 | exponential, linear, converter, envelope, curve, mix, startvalue, endvalue |
| [`faderbank`](faderbank.md) | Create multiple virtual faders in M4 controller | controller-ui | hard | 688 | 247–249 | motor fader, m4, virtual faders, bank, notches, presets, db8e, touch button |
| [`fadermatrix`](fadermatrix.md) | Matrix of up to 4x4 virtual motor faders | controller-ui | hard | 712 | 250–255 | motor fader, m4, matrix, row, column, rowcolumn, notches, presets |
| [`firefacecontrol`](firefacecontrol.md) | Control a RME Fireface interface (experimental) | midi | not-feasible | 1088 | 256–258 | rme, fireface, audio interface, x7, midi, mackie control, mixer, panning |
| [`flipflop`](flipflop.md) | Simple flip flop | logic-math | easy | 40 | 259 | flip flop, toggle, set, reset, clock divider, bit, latch |
| [`fold`](fold.md) | CV folder – keep (pitch) CV within certain bounds | utility | easy | 32 | 260–261 | fold, cv, pitch, octave, range, bounds, minimum, maximum |
| [`fourstatebutton`](fourstatebutton.md) ⚠️ | Button switching through 4 states | controller-ui | easy | 40 | 262 | button, state, switch, toggle, led, octave, obsolete |
| [`gatetool`](gatetool.md) | Operate on triggers and gates, modify gatelength | clock-timing | moderate | 56 | 263–265 | gate, trigger, edge, gatelength, gatestretch, clock-divider, taptempo, ping |
| [`ifequal`](ifequal.md) | Check if two values are equal | logic-math | easy | 32 | 266 | equal, compare, condition, if, else, logic |
| [`lfo`](lfo.md) | Low frequency oscillator (LFO) | envelope-lfo | easy | 216 | 267–272 | lfo, oscillator, waveform, sine, triangle, sawtooth, ramp, square |
| [`logic`](logic.md) | Logic operations utility | logic-math | easy | 56 | 273–275 | logic, and, or, xor, nand, nor, negate, gate |
| [`math`](math.md) | Math utility circuit | logic-math | easy | 64 | 276–278 | math, sum, difference, product, quotient, modulo, power, sine |
| [`matrixmixer`](matrixmixer.md) | Matrix mixer for CVs | mixer-cv | moderate | 176 | 279–281 | matrix, mixer, cv, buttons, leds, unity-gain, maximum, cascade |
| [`midifileplayer`](midifileplayer.md) | MIDI file player | midi | moderate | 6416 | 282–288 | midi, sd-card, file, playback, polyphony, pitch, gate, velocity |
| [`midiin`](midiin.md) | MIDI to CV converter | midi | moderate | 592 | 289–296 | midi, cv, gate, trigger, note, velocity, pitchbend, clock |
| [`midiout`](midiout.md) | CV to MIDI converter | midi | moderate | 664 | 297–305 | midi, cv-to-midi, note, velocity, pitchbend, cc, clock, polyphony |
| [`midithrough`](midithrough.md) | Forward MIDI events from input to one or more outputs | midi | easy | 240 | 306–307 | midi, forward, route, merge, trs, usb, x7, master18 |
| [`minifonion`](minifonion.md) | Musical quantizer | quantizer-pitch | moderate | 112 | 308–313 | quantizer, scale, pitch, 1v/oct, root, degree, sinfonion, harmonic-shift |
| [`mixer`](mixer.md) | CV mixer | mixer-cv | easy | 48 | 314 | mixer, sum, add, minimum, maximum, average, offset, attenuate |
| [`motoquencer`](motoquencer.md) | Motor fader sequencer | sequencer | hard | 1184 | 315–342 | sequencer, motor faders, m4, motorfader, steps, pages, gate, cv |
| [`motorfader`](motorfader.md) | Create virtual fader in M4 controller | controller-ui | hard | 120 | 343–346 | motor-fader, m4, preset, force-feedback, notches, pitch-bend, haptic, db8e |
| [`multicompare`](multicompare.md) | Compare in input with up to eight possible values | logic-math | easy | 88 | 347 | compare, select, lookup, ifequal, else, switch, signal-selection |
| [`notchedpot`](notchedpot.md) ⚠️ | Helper circuit for pots | controller-ui | easy | 40 | 348 | pot, notch, center, bipolar, hemisphere, potentiometer, obsolete |
| [`notebuttons`](notebuttons.md) | Note Selection Buttons | controller-ui | moderate | 136 | 349–351 | buttons, note, selector, radio-buttons, semitone, keyboard, leds, db8e |
| [`nudge`](nudge.md) | Modify a value in steps using two buttons | controller-ui | moderate | 152 | 352–354 | nudge, buttons, increment, decrement, step, cv-source, octave-switch, wrap |
| [`octave`](octave.md) | Multi-VCO octave animator | quantizer-pitch | moderate | 32 | 355–356 | octave, vco, oscillator, detune, spread, fifths, fat, animation |
| [`once`](once.md) | Output one trigger after the Droid has started | clock-timing | easy | 24 | 357 | trigger, startup, once, delay, cold-start, initialization |
| [`outputcalibrator`](outputcalibrator.md) | Tune the calibration of your CV outputs | utility | not-feasible | 48 | 358–359 | calibration, master18, cv-output, voltmeter, tuning, nudge, reference-voltage, db8e |
| [`polytool`](polytool.md) | Change number of voices in polyphonic setups | switch-routing | moderate | 240 | 360–361 | polyphony, voices, voice-allocation, roundrobin, midi, cv-gate, transformer, chord |
| [`pot`](pot.md) | Helper circuit for pots | controller-ui | moderate | 128 | 362–369 | pot, potentiometer, notch, slope, virtual-pot, preset, led-gauge, bipolar |
| [`quantizer`](quantizer.md) | Non-musical quantizer | quantizer-pitch | easy | 48 | 370–371 | quantizer, quantize, steps, semitones, sample-and-hold, histeresis, hysteresis, direction |
| [`queue`](queue.md) | Clocked CV shift register | sequencer | easy | 312 | 372 | shift-register, queue, delay, clock, cv, cells, melody-delay |
| [`random`](random.md) | Random number generator | random | easy | 32 | 373 | random, noise, sample-and-hold, clock, steps, minimum, maximum, voltage |
| [`recorder`](recorder.md) | Record and playback CVs and gates | sample-record | moderate | 1712 | 374–379 | recorder, tape, record, playback, cv, gate, loop, scrub |
| [`sample`](sample.md) | Sample & Hold Circuit | sample-record | easy | 40 | 380 | sample-and-hold, sh, sample, hold, trigger, gate, bypass, timewindow |
| [`select`](select.md) | Copy a signal if selected | switch-routing | easy | 24 | 381 | select, copy, led, menu-page, gate, overlay, selectat |
| [`sequencer`](sequencer.md) | Simple eight step sequencer | sequencer | moderate | 168 | 382–385 | sequencer, metropolis, pitch, gate, cv, steps, stages, repeat |
| [`sinfonionlink`](sinfonionlink.md) | Sync harmonic state from Sinfonion | chord-harmony | not-feasible | 56 | 386–387 | sinfonion, harmonic-sync, root, degree, scale, transpose, chaotic-detune, harmonic-shift |
| [`slew`](slew.md) | Slew limiter | utility | easy | 48 | 388–389 | slew, glide, portamento, exponential, linear, s-curve, slew-limiter, cv |
| [`spring`](spring.md) | Physical spring simulation | envelope-lfo | moderate | 56 | 390–391 | spring, physics, simulation, mass, velocity, position, gravity, friction |
| [`superjust`](superjust.md) | Perfect intonation of up to eight voices | quantizer-pitch | moderate | 64 | 392–393 | just-intonation, pure-intonation, tuning, chord, beatings, intervals, vco, calibration |
| [`switch`](switch.md) | Adressable/clockable switch | switch-routing | moderate | 112 | 394–395 | switch, multiplexer, router, sequential-switch, forward, backward, offset, shuffle |
| [`switchedpot`](switchedpot.md) ⚠️ | Overlay pot with multiple functions | controller-ui | moderate | 88 | 396–397 | pot, potentiometer, virtual-pot, overlay, switch, bipolar, obsolete, pickup |
| [`timing`](timing.md) | Shuffle/swing and complex timing generator | clock-timing | moderate | 56 | 398–399 | shuffle, swing, groove, clock, timing, delay, beat, quantize |
| [`togglebutton`](togglebutton.md) ⚠️ | Create on/off buttons | controller-ui | easy | 48 | 400–401 | button, toggle, on-off, latch, led, switch, layers, obsolete |
| [`transient`](transient.md) | Transient generator | envelope-lfo | easy | 56 | 402–403 | transient, ramp, slew, envelope, slow, loop, pingpong, phase |
| [`triggerdelay`](triggerdelay.md) | Trigger Delay with multi tap and optional clocking | clock-timing | moderate | 248 | 404–405 | trigger, delay, gate, multi-tap, repeat, clock, gatelength, overflow |
| [`unusedfaders`](unusedfaders.md) | Declare unused motor faders | controller-ui | easy | 32 | 406 | motor-fader, unused, disable, select, menu, faderbank |
| [`vcotuner`](vcotuner.md) | measure frequency and tuning of a VCO | conversion | moderate | 72 | 407–410 | tuner, vco, frequency, pitch, tuning, cents, master18, db8e |
| [`watch`](watch.md) | Watch how a signal evolves | utility | moderate | 72 | 411–413 | change-detection, edge-detection, slope, strumming, trigger, movement, hysteresis, slew |

⚠️ = obsolete (kept for reference; avoid in new patches).

## By function

### Sequencers

- [`algoquencer`](algoquencer.md) — Algorithmic sequencer
- [`arpeggio`](arpeggio.md) — Arpeggiator – pattern based melody generator
- [`encoquencer`](encoquencer.md) — Performance sequencer using E4 encoders
- [`motoquencer`](motoquencer.md) — Motor fader sequencer
- [`queue`](queue.md) — Clocked CV shift register
- [`sequencer`](sequencer.md) — Simple eight step sequencer

### Clock & timing

- [`burst`](burst.md) — Generate burst of pulses
- [`clockedtrigger`](clockedtrigger.md) — Delay a trigger until the next clock tick
- [`clocktool`](clocktool.md) — Clock divider / multiplier / shifter
- [`euklid`](euklid.md) — Euclidean rhythm generator
- [`gatetool`](gatetool.md) — Operate on triggers and gates, modify gatelength
- [`once`](once.md) — Output one trigger after the Droid has started
- [`timing`](timing.md) — Shuffle/swing and complex timing generator
- [`triggerdelay`](triggerdelay.md) — Trigger Delay with multi tap and optional clocking

### Envelopes & LFOs

- [`contour`](contour.md) — Contour generator
- [`lfo`](lfo.md) — Low frequency oscillator (LFO)
- [`spring`](spring.md) — Physical spring simulation
- [`transient`](transient.md) — Transient generator

### Quantizers & pitch

- [`calibrator`](calibrator.md) — VCO Calibrator
- [`detune`](detune.md) — Detune multiple voices in a most disharmonic way
- [`minifonion`](minifonion.md) — Musical quantizer
- [`octave`](octave.md) — Multi-VCO octave animator
- [`quantizer`](quantizer.md) — Non-musical quantizer
- [`superjust`](superjust.md) — Perfect intonation of up to eight voices

### Chords & harmony

- [`chord`](chord.md) — Chord generator
- [`sinfonionlink`](sinfonionlink.md) — Sync harmonic state from Sinfonion

### Logic & math

- [`compare`](compare.md) — Compare two values
- [`flipflop`](flipflop.md) — Simple flip flop
- [`ifequal`](ifequal.md) — Check if two values are equal
- [`logic`](logic.md) — Logic operations utility
- [`math`](math.md) — Math utility circuit
- [`multicompare`](multicompare.md) — Compare in input with up to eight possible values

### Random

- [`bernoulli`](bernoulli.md) — Random gate distributor
- [`random`](random.md) — Random number generator

### Mixers & CV processing

- [`crossfader`](crossfader.md) — Morph between 8 inputs
- [`matrixmixer`](matrixmixer.md) — Matrix mixer for CVs
- [`mixer`](mixer.md) — CV mixer

### Switches & routing

- [`case`](case.md) — Switch choosing from inputs via conditions
- [`polytool`](polytool.md) — Change number of voices in polyphonic setups
- [`select`](select.md) — Copy a signal if selected
- [`switch`](switch.md) — Adressable/clockable switch

### Controllers & UI

- [`button`](button.md) — Does all sorts of useful things with buttons
- [`buttongroup`](buttongroup.md) — Connected buttons
- [`display`](display.md) — Display texts and change parameters on a DB8E
- [`encoder`](encoder.md) — Provide access to a knob on the E4 or DB8E controller
- [`encoderbank`](encoderbank.md) — Create bank of up to 8 virtual input knobs from E4 encoders
- [`faderbank`](faderbank.md) — Create multiple virtual faders in M4 controller
- [`fadermatrix`](fadermatrix.md) — Matrix of up to 4x4 virtual motor faders
- [`fourstatebutton`](fourstatebutton.md) ⚠️ — Button switching through 4 states
- [`motorfader`](motorfader.md) — Create virtual fader in M4 controller
- [`notchedpot`](notchedpot.md) ⚠️ — Helper circuit for pots
- [`notebuttons`](notebuttons.md) — Note Selection Buttons
- [`nudge`](nudge.md) — Modify a value in steps using two buttons
- [`pot`](pot.md) — Helper circuit for pots
- [`switchedpot`](switchedpot.md) ⚠️ — Overlay pot with multiple functions
- [`togglebutton`](togglebutton.md) ⚠️ — Create on/off buttons
- [`unusedfaders`](unusedfaders.md) — Declare unused motor faders

### MIDI

- [`firefacecontrol`](firefacecontrol.md) — Control a RME Fireface interface (experimental)
- [`midifileplayer`](midifileplayer.md) — MIDI file player
- [`midiin`](midiin.md) — MIDI to CV converter
- [`midiout`](midiout.md) — CV to MIDI converter
- [`midithrough`](midithrough.md) — Forward MIDI events from input to one or more outputs

### Conversion

- [`adc`](adc.md) — AD Converter with 12 bits
- [`dac`](dac.md) — DA Converter with 12 bits
- [`explin`](explin.md) — Exponential to linear converter
- [`vcotuner`](vcotuner.md) — measure frequency and tuning of a VCO

### Sampling & recording

- [`cvlooper`](cvlooper.md) — Clocked CV looper
- [`recorder`](recorder.md) — Record and playback CVs and gates
- [`sample`](sample.md) — Sample & Hold Circuit

### Utility

- [`copy`](copy.md) — Copy a signal, while applying attenuation and offset
- [`delay`](delay.md) — A tape delay for CVs, gates and numbers
- [`droid`](droid.md) — General DROID controls
- [`fold`](fold.md) — CV folder – keep (pitch) CV within certain bounds
- [`outputcalibrator`](outputcalibrator.md) — Tune the calibration of your CV outputs
- [`slew`](slew.md) — Slew limiter
- [`watch`](watch.md) — Watch how a signal evolves

## Obsolete circuits

These remain documented but should be avoided in new patches:

- [`fourstatebutton`](fourstatebutton.md) — Button switching through 4 states
- [`notchedpot`](notchedpot.md) — Helper circuit for pots
- [`switchedpot`](switchedpot.md) — Overlay pot with multiple functions
- [`togglebutton`](togglebutton.md) — Create on/off buttons
