---
circuit: spring
title: Physical spring simulation
obsolete: false
ram_bytes: 56
manual_pages: [390, 391]
category: envelope-lfo
tags: [spring, physics, simulation, mass, velocity, position, gravity, friction, bounce, modulation, damped-sine]
see_also: [lfo]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: true
difficulty_note: Normalized mass-spring ODE with friction, flow resistance, shove, speed and reset; model is documented but the integration scheme is not.
verification_note: "Logic is deterministic and computable offline, but numeric integration (step size, method) is unspecified, so results can diverge from hardware. Needs reference position/velocity traces to validate exact behaviour."
---

# spring — Physical spring simulation

A physical simulation of a mass hanging from on an ideal spring which can create
interesting "bouncing" CV sources.

Consider the drawing on page 390: a mass hangs from a spring mounted to the
ceiling. The spring pulls the mass upwards with `springforce` while `gravity`
pulls it downwards, and the mass swings between position `0.00` (top) and beyond.

Without any further parameters the mass starts at position `0.00` and velocity
`0.00` and is accelerating downwards until the force of the spring equals the
gravity. At this point it decelerates until the velocity is zero. Now the mass is
being accelerated *upwards* until it reaches the top position at `0.00` again.
This results, in essence, to a damped sine wave.

The position and velocity are available at their respective outputs ready to be
used for modulation.

```droid
[spring]
    position = O1
    velocity = O2
```

Now, this could be done more easily with the LFO circuit (see
[`lfo`](lfo.md)). But it's getting interesting when you look at the other
parameters and the modulation possibilities. Please look at the table of jacks
for details.

## Friction

Per default the motion is without any friction and thus the mass will move up and
down forever. You can apply two different types of friction. `flowresistance` is
the type of friction a body has in a liquid or gas. Its force is relative to its
velocity. Whereas the normal `friction` force is constant.

When you use any type of friction, the spring will finally stop swinging. You
need to either *shove* it from time to time or reset it to its start with the
`reset` trigger input.

The following example will create a slowly decaying sine wave, which is restarted
whenever a trigger is sent to `reset`:

```droid
[spring]
    flowresistance = 0.5
    reset = I1
    position = O1
    velocity = O2
```

## Shoving

You also can *shove* the mass downwards or upwards. As long as you send a gate
signal into `shove` the mass will be shoved downwards. The exact force can be set
with `shoveforce` and defaults to being the same as the gravity. A negative value
will lift the mass upwards.

Setting `shove` to a constant `1` value will steadily apply `shoveforce`, which
can be interesting as that is itself a changing CV (some LFO, feedback loop or
whatever).

## The physical model

Please note that the physical model is normalized in a way such that every
parameter is 1. For example the mass is 1 kg and the gravity is 1 N/kg. The force
of the spring is 1 N/m.

In order to avoid anomalies or infinities, the velocity of the mass is limited to
±10 m/s and the position is limited to the range of ±10 m.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `mass` (`m`) | CV | `1.0` | The mass of the object on the spring. The heavier it is, the farther the spring will move up and down. |
| `gravity` (`g`) | CV | `1.0` | The gravity of the simulated planet the spring is mounted at. If you set the gravity to zero, the mass will move exactly around the zero position from positive to negative and back. But you need to shove it or set a start position other than 0, in order to get it started. |
| `springforce` (`f`) | CV | `1.0` | The force of the string per m it is stretched. In an ideal spring the force is proportional to the current elongation. |
| `flowresistance` (`fr`) | CV | `0.0` | Setting this to a value > 0 will dampen the oscillation in a way, that higher velocities will be damped more then slower ones. This means that impact of the friction will get less and less as time goes by and the movement slows down. |
| `friction` (`fi`) | CV | `0.0` | Setting this to a value > 0 will also dampen the oscillation, but in a way that is independent of the current speed of the mass. |
| `speed` (`sp`) | CV | `0.0` | This parameter speeds up or slows down the perceived time. It works on a 1V/Oct base. Every positive 1V (or 0.1) doubles the speed. So if you set `speed` to 2V or 0.2 it will speed up the movement by a factor of 4. An input of −1V will slow down the movement to the half. |
| `shove` (`sh`) | gate | `0` | While this gate input is logical 1, an extra force of 1 N is applied to the mass pointing downwards. You can change that force with `shoveforce`. |
| `shoveforce` (`sf`) | CV | `1.0` | This is the force being applied to the mass while `shove` is active. |
| `reset` (`r`) | trigger | | Resets the whole system to its start position. |
| `startvelocity` (`sv`) | CV | `0.0` | Sets the velocity the mass has when a reset is triggered. |
| `startposition` (`spo`) | CV | `0.0` | Sets the position the spring has when a reset is triggered. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `velocity` (`v`) | CV | Outputs the current velocity of the mass. |
| `position` (`p`) | CV | Output the current length of the string. If the string goes upwards (which is possible with certain modulations), this can be negative. |

## See also

- [`lfo`](lfo.md) — a simpler way to create a plain sine modulation.
