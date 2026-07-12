---
circuit: sequencer
title: Simple eight step sequencer
obsolete: false
ram_bytes: 168
manual_pages: [382, 383, 384, 385]
category: sequencer
tags: [sequencer, metropolis, pitch, gate, cv, steps, stages, repeat, slew, chain, chaintonext]
see_also: [quantizer, minifonion]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Eight-stage pitch/gate/cv sequencer with repeats, per-step slew, stages/steps limits and multi-instance chaining — moderate state, and the cross-circuit chaintonext chaining is the tricky part.
verification_note: "Headless: clock the sequencer and assert pitch/gate/cv per step under repeats, slew, gatelength and stages/steps, including a chaintonext chain spanning multiple instances."
---

# sequencer — Simple eight step sequencer

This circuit implements a sequencer that is a bit similar to the widely known
Metropolis sequencer by Intellijel. It lacks a couple of its features – but most
of these can be patched externally by use of other circuits.

On the other hand it is not limited to 8 stages since you can chain multiple
instances of this sequencer together to form one large sequencer very easily.

Since everything in the DROID is controllable via CV, of course pitch and gate
signals are included, which makes the circuit much more versatile than it may
seem at a first look.

Here is a small example of a CV sequencer that is playing four voltages in a
turn (it needs a clock into `I1`):

```droid
[sequencer]
    clock       = I1
    pitchoutput = O1
    pitch1      = 1V
    pitch2      = 3.5V
    pitch3      = 8V
    pitch4      = -2V
```

If you set the `outputscale` parameter to 1/12 V (which is the same as the
number 1/120), you can specify pitches directly in semitones:

```droid
[sequencer]
    clock       = I1
    pitchoutput = O1
    outputscale = 1/120
    pitch1      = 0
    pitch2      = 12
    pitch3      = 10
    pitch4      = 7
    pitch5      = 5
    pitch6      = 3
    pitch7      = 5
    pitch8      = 7
```

The following example uses four expander buttons for turning the steps on or off
and four pots, which are scaled down to a range of 0V ... 3V.

```droid
[p2b8]
[p2b8]

[lfo]
    hz = 4
    square = _CLOCK

[button]
    button = B1.1
    led    = L1.1

[button]
    button = B1.2
    led    = L1.2

[button]
    button = B1.3
    led    = L1.3

[button]
    button = B1.4
    led    = L1.4

[sequencer]
    clock       = _CLOCK
    pitchoutput = O1
    gateoutput  = O2
    pitch1      = P1.1 * 3V
    pitch2      = P1.2 * 3V
    pitch3      = P2.1 * 3V
    pitch4      = P2.2 * 3V
    gate1       = L1.1
    gate2       = L1.2
    gate3       = L1.3
    gate4       = L1.4
```

Note: the pitch values you dial in with the pots are not quantized, so it's a
bit hard to hit a musical pitch. Please have a look at the circuits
[`quantizer`](quantizer.md) and [`minifonion`](minifonion.md) for how to
quantize pitch values.

## Making longer sequences

The sequencer circuit is limited to 8 steps. But: you can easily chain a large
number of these circuits together to form longer sequences. This is super easy.
Just set the jack `chaintonext` to 1 and place another sequencer circuit with
more steps after that. Here is an example for a 12 step sequencer:

```droid
[p2b8]

[lfo]
    hz = P1.1 * 30
    output = _CLOCK

[sequencer]
    clock = _CLOCK
    reset = B1.1
    pitchoutput = O1
    gateoutput = O2
    outputscaling = 1/120
    pitch1 = 1
    pitch2 = 8
    pitch3 = 13
    pitch4 = 25
    pitch5 = 4
    pitch6 = 11
    pitch7 = 7
    pitch8 = 21
    chaintonext = 1 # continue at next sequencer

[sequencer]
    pitch1 = 2
    pitch2 = 9
    pitch3 = 14
    pitch4 = 26
```

You can make the chain longer by adding more sequencer circuits. All but the
last must have `chaintonext` set to 1. Here comes a 19 step sequencer:

```droid
[p2b8]

[lfo]
    hz = P1.1 * 30
    output = _CLOCK

[sequencer]
    clock = _CLOCK
    reset = B1.1
    pitchoutput = O1
    gateoutput = O2
    outputscaling = 1/120
    pitch1 = 1
    pitch2 = 8
    pitch3 = 13
    pitch4 = 25
    pitch5 = 4
    pitch6 = 11
    pitch7 = 7
    pitch8 = 21
    chaintonext = 1 # continue at next sequencer

[sequencer]
    pitch1 = 2
    pitch2 = 9
    pitch3 = 14
    pitch4 = 26
    pitch5 = 2
    pitch6 = 9
    pitch7 = 14
    pitch8 = 26
    chaintonext = 1 # continue at next sequencer

[sequencer]
    pitch1 = 3
    pitch2 = 10
    pitch3 = 15
```

Notes:

- Define all the input and output jacks like `clock`, `pitchoutput` etc. just
  for the first sequencer. All subsequent ones just have `pitch`, `gate`,
  `repeat`, `slew` and `cv` definitions.
- The parameter `chaintonext` is dynamic. You could make or break the chain with
  a toggle button or something else if you like.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `clock` (`c`) | trigger | | Each trigger into this jack advances the sequence by one step. |
| `reset` (`r`) | trigger | | A trigger here resets the sequence to the first step. |
| `stages` (`sg`) | integer | ☞ smart | Number of inputs of `pitch..`, `gate..`, `slew..`, `cv` and `repeats` that should be used. If you set stages to a number higher than the number of used inputs, all inputs will be used. If you omit this parameter, all used inputs will be used. |
| `steps` (`s`) | integer | `0` | With this input you can force the sequencer to begin from start after a certain number of clock cycles. If you omit the parameter or if it is set to 0, the sequencer will play all stages with all repeats until it resets to the beginning. |
| `transpose` (`tr`) | CV | `0.0` | This voltage is added to the pitch output. |
| `outputscaling` (`os`) | CV | `1.0` | The output pitch is multiplied by this parameter. |
| `gatelength` (`gl`) | 0..1 | ☞ smart | The length of the output gates. If it is unpatched, the original input clock is fed through 1:1 (with its own duty cycle). When used, it is a ratio from 0.0 to 1.0 and relative to the cycle of the input clock. Setting the `gatelength` to 1.0 merges two adjacent gates together since there is not time left for a low gate before the next step begins. |
| `pitch1 … pitch8` (`p`) | CV | `0.0` | These are the pitches of the various steps. You can put fixed numbers here but also of course pots or variable inputs. Note: The number of used input jacks defines the length of the sequence, unless you override that with `stages`. |
| `cv1 … cv8` | CV | `0.0` | Each step has an optional CV assigned. You can use that CV for modulating something or even outputting a second pitch information. |
| `gate1 … gate8` (`g`) | gate | `1` | The gate inputs should be 0 (off) or 1 (on). For stages with a 0-gate no output gate is produced and the pitch information is kept at the previous state. Unpatched gates are considered to be on! |
| `slew1 … slew8` (`sw`) | CV | `0.0` | Enables slew limiting for that stage. The input is not binary but you can set the amount of slew here – individually for each step. 0.0 switches the slew off, higher values create slower slews. |
| `repeat1 … repeat8` (`rp`) | CV | `1.0` | Set this to a positive integer number like 1, 2, and so on. It sets the number of times this stage should be repeated until the next stage will be approached. It is currently not allowed to have 0 repeats – although this would make sense in a future version. |
| `chaintonext` (`cn`) | gate | ☞ smart | If you set this input to 1, the next sequencer circuit's `pitch` and other step inputs will be added to this sequencer. See the general circuit notes for details. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `pitchoutput` (`po`) | CV | The pitch output. It is unquantized. |
| `cvoutput` (`co`) | CV | The optional CV output, in case you use the `cv1 … cv8` inputs. |
| `gateoutput` (`go`) | gate | The gate output. |

## See also

- [`quantizer`](quantizer.md) — quantize the unquantized pitch output to a scale.
- [`minifonion`](minifonion.md) — musical quantizer for the pitch output.
