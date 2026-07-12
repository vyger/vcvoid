---
circuit: once
title: Output one trigger after the Droid has started
obsolete: false
ram_bytes: 24
manual_pages: [357]
category: clock-timing
tags: [trigger, startup, once, delay, cold-start, initialization]
see_also: []
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Fire one trigger a fixed delay after start; the cold-vs-warm-start distinction is the only wrinkle.
verification_note: "Headless: assert exactly one trigger fires `delay` seconds after startup; cold-start suppression checked by simulating a warm patch reload."
---

# once — Output one trigger after the Droid has started

This circuit outputs exactly one trigger after the Droid module has started. You
can set a delay for that to happen.

Example:

```droid
[once]
    delay        = 0.2 # 200 ms
    trigger      = _DO_ONCE
```

The applications are up to you. Maybe you want to automatically start something
when the Droid starts or update some MIDI data or whatever weird other idea you
have in mind.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `delay` (`dl`) | CV | `0.01` | Set a delay in seconds after the Droid's start before the trigger triggers. Note: the default value is 10 ms, not zero. This allows all attached controllers to have sent at least one update before and the real pot values etc. are available at the circuits. |
| `onlycoldstart` (`c`) | gate | `0` | If you set this input to 1, `once` just sends a trigger after a cold start, only. A cold start means that the Droid has been powered up. Pressing the button for loading a new patch does a warm start. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `trigger` (`t`) | trigger | The trigger is output here. |
