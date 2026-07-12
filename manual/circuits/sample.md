---
circuit: sample
title: Sample & Hold Circuit
obsolete: false
ram_bytes: 40
manual_pages: [380]
category: sample-record
tags: [sample-and-hold, sh, sample, hold, trigger, gate, bypass, timewindow]
see_also: []
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Sample & hold with trigger, gate-tracking and a settle timewindow — simple deterministic logic.
verification_note: "Headless: assert output holds the input value at each trigger, tracks while gate/bypass is high, and re-samples during the timewindow before freezing."
---

# sample — Sample & Hold Circuit

This is a simple sample & hold circuit. Each time a positive trigger is seen at
the jack `sample` a new value is sampled from `input` and sent to the `output`.

## Example

```droid
[sample]
    input = I1
    sample = I2
    output = O1
```

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | CV | `0.0` | Input signal to be sampled. |
| `sample` (`sm`) | trigger | | A positive trigger here will read the current value from `input` and store it internally. |
| `gate` (`g`) | gate | | This is an alternative way of making the circuit take a sample from the input. Here it is sampling all the time while the gate is high. In that way it is a bit like `bypass`. But as soon as the gate goes low again, the output sticks to the last sample value just before that. |
| `timewindow` (`tw`) | CV | `0.0` | This optional parameter helps tackling a problem that many (non-analog) sequencers show: often their pitch CV is not at its final destination value at the time their gate is being output. Often you see a very short "slew" ramp of say 5 ms after the gate. During that time the pitch CV moves from its former to the new value. Now if you trigger the `sample` circuit with the sequencer's gate you will essentially sample the previous pitch CV instead of the new one. Or maybe something in between. Now the `timewindow` parameter introduces a short time window after the `sample` trigger. During that time period the sample & hold circuit will constantly adapt to a changed input CV (is essentially in bypass mode). When that time is over, the input is finally frozen. The `timewindow` parameter is in seconds. So when you set `timewindow` to say 0.005 (which means 5 ms), you give the input CV 5 ms time for settling to its final value after a trigger to `sample` before freezing it. |
| `bypass` (`b`) | gate | | While this gate input is high, the circuit is bypassed and `input` is copied to `output`. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | The most recently sampled value is sent here. |
