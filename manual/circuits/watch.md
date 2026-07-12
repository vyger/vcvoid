---
circuit: watch
title: Watch how a signal evolves
obsolete: false
ram_bytes: 72
manual_pages: [411, 412, 413]
category: utility
tags: [change-detection, edge-detection, slope, strumming, trigger, movement, hysteresis, slew, moving]
see_also: [slew]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Four independent detectors (change with hysteresis, hard edge, step/strum crossings, slope analysis via a slew filter) all producing triggers/gates on output registers, no LED or display of its own.
verification_note: "Detection logic is deterministic and outputs to gate/trigger registers, so verify headless against computed references. Note edge detection is defined per internal loop cycle (0.1-2 ms) and slope response reuses slew's exponential filter, so exact edge timing depends on the emulated loop rate."
---

# watch — Watch how a signal evolves

This circuit watches how an input signal evolves over time. It outputs various
information and triggers that reflect this input's movements. There are four
different operation modes (which can be used all at the same time if you want).

## 1. Change detection

The simplest mode is watching the input signal for a change. When the circuit
starts its life it takes a sample of the input. When at a later time the input
value has changed, you get an output trigger at `changed`.

In order to avoid spurious triggers if the input signal has noise, the change
needs to exceed the value set by `histeresis` for a change to be detected. After
a change is detected, the new changed value is now used as the new sample for
change detection.

```droid
[watch]
    input = I1
    histeresis = 0.01V # 10 mV
    changed = G1
```

If you are interested in the direction of the change, you can use `changedup`
and `changeddown`.

## 2. Edge detection

A second mode is to detect hard *edges* in the input. An edge is a sudden jump
of the input value from one level to another level without any intermediate
slope. The result is a trigger output at `edge`. If you are interested in the
direction of the change, you can use `edgeup` or `edgedown`.

To be detected the input value needs to jump at least by the value set by
`edgesize` in one internal loop cycle of your master module. If you apply this
to external analog modules it's not guaranteed that they are *fast enough*.

```droid
[watch]
    input = _SOME_CV
    edgesize = 0.01V
    edgeup = G1
    edgedown = G2
```

While this looks similar to `changed`, it's not quite the same. While `changed`
also triggers at edges, if the input is a slow slope, `changed` will trigger,
`edge` doesn't.

## 3. Strumming

The third mode can be used to implement something like strumming by use of a
moving CV. First you set the position of the "strings" by evenly dividing up
your input CV range. This is done with `stepsize` and the default is 0.1 which
corresponds to one octave when you interprete the input CV as a 1V/oct signal.

Now every time the input signal moves from one division to another, a trigger is
output. For example if it moves from 1.9 V to 2.1 V, it crosses the 2 V border
and a trigger is created at `step`. Again, you have access to the direction if
you are using `stepup` or `stepdown`.

```droid
[watch]
    input = _SOME_CV
    stepsize = 0.0833333~V # 1 semitone
    step = G1
```

## 4. Slope analysis

The fourth and last mode is the most complex one. It analyses the *slope* of the
input, i.e. the speed with which it is currently moving up or down.

First the input signal is sent through a slew limiter. You set the speed of this
slew limiter with `response`. A value of `0` disables it. This means that the
slightest flicker in the input can be shown as a large change if it happens in a
very short time. Depending on how your input behaves, you need to experiment
which value for `response` works best for you. Small values make the circuit
react faster, large values slower. Slower means that a movement must persist
over a longer period of time to be detected.

The resulting slope is output at `slope`. A value of 1 means a change of 1.0
(= 10 V) *per second*. You can change this to relate to the clock by using
`taptempo`. In this case the output is 1.0 per clock tick. A negative value of
`slope` indicates that the value is currently falling.

As a last step, whenever the current slope exceeds the value set by `threshold`,
the output `moving` is set to `1`. You have access to the direction by using
`movingup` and `movingdown`.

```droid
[watch]
    input = I1
    threshold = 0.1V # per second
    movingup = G1
    movingdown = G2
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | CV | `0.0` | A value to watch. |
| `stepsize` (`ss`) | CV | `0.1` | Size of the steps for step detection. For example a value of 0.1 V slices the CV range into compartments of 100 mV. Whenever this input moves from one compartment into another, a trigger at `step` is sent. |
| `edgesize` (`es`) | CV | `0.01` | Here you set the difference the input CV value must have from its value in the last loop cycle for an edge to be detected. The length of your loop cycles depends on your patch size but usually is within the range of 0.1 … 2.0 msec. |
| `taptempo` (`tt`) | gate | `☞ smart` | Patch a steady input clock here to change the meaning of the `slope` output and the slope detection from 1.0/sec to 1.0 per clock tick. |
| `threshold` (`th`) | CV | `0.01` | If the current slope exceeds this value, the `moving` output is set to `1`. |
| `response` (`r`) | CV | `0.1` | Sets how fast we react to changes in the input value when slope detection is done. The behaviour of this value is exactly like the exponential slew limiting done by the circuit [`slew`](slew.md). A value of `0.0` disables the input filter so the circuit reacts super fast. `1.0` makes the detection infinitely slow, so this value basically freezes the input and does not make much sense. You might have to experiment which value gives you the best tradeoff between stability and detection speed. |
| `histeresis` (`h`) | CV | `0.01` | Here you set the value that the input must differ from the last detected change for a change to be detected. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `slope` (`sl`) | CV | Outputs the current slope of the input, i.e. how much it changes per second (or per clock tick if you use `taptempo`). |
| `moving` (`m`) | gate | Outputs 1 if the current slope is out of the range `−threshold` … `threshold`. |
| `movingup` (`mu`) | gate | Outputs 1 if the current slope is greater than `threshold`. |
| `movingdown` (`md`) | gate | Outputs 1 if the current slope is less than `−threshold`. |
| `step` (`s`) | trigger | Sends a trigger when the input moves into another compartment (whose size is set by `stepsize`). |
| `stepup` (`su`) | trigger | Sends a trigger when the input moves into a higher compartment (whose size is set by `stepsize`). |
| `stepdown` (`sd`) | trigger | Sends a trigger when the input moves into a lower compartment (whose size is set by `stepsize`). |
| `edge` (`e`) | trigger | Sends a trigger if an edge has been detected in the input signal. |
| `edgeup` (`eu`) | trigger | Sends a trigger if a positive edge has been detected in the input signal. |
| `edgedown` (`ed`) | trigger | Sends a trigger if a negative edge has been detected in the input signal. |
| `changed` (`c`) | trigger | Sends a trigger if the input signal has changed. |
| `changedup` (`cu`) | trigger | Sends a trigger if the input signal has changed upwards. |
| `changeddown` (`cd`) | trigger | Sends a trigger if the input signal has changed downwards. |

## See also

- [`slew`](slew.md) — the exponential slew limiter whose behaviour the `response` input mirrors.
