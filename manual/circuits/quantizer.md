---
circuit: quantizer
title: Non-musical quantizer
obsolete: false
ram_bytes: 48
manual_pages: [370, 371]
category: quantizer-pitch
tags: [quantizer, quantize, steps, semitones, sample-and-hold, histeresis, hysteresis, direction, bypass]
see_also: [minifonion]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Step quantization with direction, hysteresis, bypass and a triggered sample-and-hold mode — all simple deterministic math.
verification_note: "Headless: sweep input voltages and assert quantized output for each steps/direction/histeresis setting; verify triggered-mode freeze and the 10 ms changed trigger."
---

# quantizer — Non-musical quantizer

This quantizer circuit is very simple. It reads an input voltage, quantizes it to
the next discrete step that you configured and outputs it.

You *can* use it for musical purposes by setting the number of steps to 12 per
Volt (which is the default). It will quantize the input to semitones.

The following example scales down a pot `P1.1` to 1 V (i.e. one octave) and then
quantizes it to semitones. Since 12 is the default value for `steps` this
parameter can be omitted here:

```droid
[quantizer]
    input  = P1.1 * 1V
    output = O1
```

Note¹: In fact you can select *13* semitones here because if you turn the pot
fully CW it will output 1, which will be scaled to 1 V and then quantized to 1 V
– which is the 13th semitone above the lowest possible note.

Note²: if you are looking for a more musical quantizer then have a look at the
[`minifonion`](minifonion.md) circuit.

You can use the Quantizer circuit as a sample & hold circuit if you set `steps`
to 0 and use the trigger input:

```droid
[quantizer]
    input   = I1
    steps   = 0
    trigger = I2
    output = O1
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | CV | `0.0` | Patch the unquantized input voltage here. |
| `trigger` (`t`) | trigger | | This jack is optional. If you patch it, the quantizer will work in triggered mode. Here the output pitch is always frozen until the next trigger happens. |
| `steps` (`s`) | integer | `12` | Number of steps that one Volt should be divided in. The default is `12` and will quantize the input voltage to semitones. The number of steps is related to a value of 1 V which means `0.1`. It *is* allowed to use a fractional number here. E.g. the value `1.2` will quantize to 12 steps per 10 V (which means 12 steps per `1.0`), which can make sense. A value of `0` (or lower) will basically mean an *infinite* number of steps and thus practically disable quantization. |
| `bypass` (`b`) | gate | `0` | If you set this gate input to `1` then quantization is bypassed and the input voltage is directly copied to the output. |
| `direction` (`d`) | integer | `1` | This parameter selects which value is chosen if the input value lies between two allowed quantized values (which is almost always the case). The default is to choose the nearest value. `0` = quantize down, `1` = quantize to nearest allowed value, `2` = quantize up. |
| `histeresis` (`h`) | CV | | This parameter helps in situations where there is some noise on the input signal. You can set a minimum threshold the input signal must *change* in order to be requantized. As long as the input signal stays in the range *previous input +/- histeresis* it is ignored and the previous value is output. So when your noise level is not more than 0.01V, set `histeresis = 0.01V`. This will prevent requantization because of noise on the input and thus avoid random jumps of the output signal. `histeresis` was introduced to tackle the jitter on the signal of a theremin in a situation where it's used to create a quantized note pitch. It avoids random note jumps when the hand's position is in the middle between two semitones. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | stepped | Here comes your quantized output voltage. |
| `changed` (`c`) | trigger | Whenever the quantization changes to a new output value a trigger with the duration 10 ms is output here. No trigger is output in bypass mode. |

## See also

- [`minifonion`](minifonion.md) — a musical quantizer that snaps to scales.
