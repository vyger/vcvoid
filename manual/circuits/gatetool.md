---
circuit: gatetool
title: Operate on triggers and gates, modify gatelength
obsolete: false
ram_bytes: 56
manual_pages: [263, 264, 265]
category: clock-timing
tags: [gate, trigger, edge, gatelength, gatestretch, clock-divider, taptempo, ping]
see_also: [clocktool]
impl_difficulty: moderate
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Gate/trigger/edge conversions plus gatelength control via seconds, gatestretch, min/max, and taptempo clock-relative timing; several interacting timing paths.
verification_note: "All paths are deterministic timing math; verify output gate lengths, edge toggling, and taptempo-relative fractions headlessly at a fixed sample rate against computed references."
---

# gatetool — Operate on triggers and gates, modify gatelength

This utility works with triggers, gates and edge-triggers, can convert each type
into each other type and can change the length of gates in flexible ways.

`gatetool` has three different types of inputs. Usually you would patch only one
of these:

- `inputgate` expects a gate signal and honors the time span during which the
  gate is high. It takes into account the length of the input gate.
- `inputtrigger` expects triggers signals. Here the time span during which the
  input is high is not relevant. Just the start of the trigger counts. If you
  patch a "normal" gate signal here, the length of it is ignored (which could be
  just what you wanted).
- `inputedge` looks for transitions between low and high or high and low. Such
  transitions are called "edge". Each time the input level swaps is considered
  as a trigger. So patching a normal gate signal here will count as a trigger
  when the gate goes high and another trigger when it goes low again.

For each input gate, trigger or edge, the `gatetool` outputs an output gate *and*
an output trigger *and* an output edge:

- `outputgate` goes high on an input gate, trigger or edge. The length of the
  output gate can be modified in various ways (see below).
- `outputtrigger` outputs a short trigger of 10 ms on an input gate, trigger or
  edge.
- `outputedge` toggles between 0 and 1 on each input gate, trigger or edge.

## Modifying the gate length

The length of the output gate on `outputgate` can be specified in various ways.
First let's assume that you use the `inputtrigger` or the `inputedge` input. In
this case there is no "input gate length". The length of the output gate is set
by `gatelength`, which is a number in seconds:

```droid
[gatetool]
    inputtrigger = I1
    outputgate = O1
    gatelength = 0.5 # 500 ms
```

As an option, you can set the gate length in relation to a reference clock
(please see the manual ([basics](../basics.md)) for details on using `taptempo`
inputs). As soon as you patch `taptempo`, the `gatelength` parameter is in
relation to one input clock tick (in DROID language 0.3 is just the same as
30%):

```droid
[gatetool]
    inputtrigger = I1
    taptempo = I2 # some steady clock
    gatelength = 0.3 # 30% of one clock tick
    outputgate = O1
```

Note: The `taptempo` input has the one and only purpose of setting a time
reference to `gatelength`.

Now let's assume that you have an input gate signal, that has a specific length
and you want to keep that or work on that. For that purpose use the `inputgate`
and the `outputgate`:

```droid
[gatetool]
    inputgate = I1
    outputgate = O1 # keep gate length
```

This example is not very useful, though, since it just copies the input gate to
the output without changing the gate length. Use the `gatelength` parameter to
switch the behaviour to that of `triggerinput`: the input gate length is ignored
and overruled by that parameter:

```droid
[gatetool]
    inputgate = I1
    outputgate = O1
    gatelength = 0.5 # 500 ms
```

More interesting is `gatestretch`. This is the first time the length of the input
gate is honored and has any relevance: Here you specify a percentage by that the
gate should be made *longer*:

```droid
[gatetool]
    inputgate = I1
    outputgate = O1
    gatestretch = 0.3 # make gate 30% longer
```

For obvious reasons you cannot make a gate shorter by a percentage since nobody
– not even Droid – can look into the future...

Note: `gatestretch` obviously only makes sense if you don't use `gatelength`!

If you want to keep the gate length within certain bounds, you can make use of
`mingatelength` and `maxgatelength`. They set a lower or upper limit on the
length of the output gate. They only are effective when `gatelength` is not used.
Both parameter are in seconds or – if `taptempo` is used – in fractions of one
clock tick.

The following example forwards the input gates unchanged to the output, but makes
sure that the length is never shorter than 10% and never longer that 90% of a
clock tick:

```droid
[gatetool]
    inputgate = I1
    taptempo = I2 # steady clock
    outputgate = O1
    mingatelength = 0.1
    maxgatelength = 0.9
```

## Building a clock divider

The edge triggers can help you building a clock divider that divides by two. Of
course you could do that with [`clocktool`](clocktool.md), as well. But this
example illustrates a bit, how the edge triggers work. Consider the following
example:

```droid
[gatetool]
    inputtrigger = I1
    outputedge = O1
```

Now for every trigger in `I1`, the edge output *changes* it's level. So in order
to go from low to high and low again, you need two input triggers. The output
signal at `O1` then just outputs one gate signal in that time. So two triggers
are converted into one.

## Use edges for pinging filters

Another application of edges is pinging filters with a *zero length* impulse. Use
the same patch snippet as above and patch `O1` to the input of a resonant filter.
By just using the edge, you really get exactly *one* ping. A trigger – regardless
how short – always has two edges and thus pings the filter twice, which can sound
unclean.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `inputgate` (`ig`) | gate | | Input gate. Use this if the length of the input gate is relevant. |
| `inputtrigger` (`it`) | trigger | | Input trigger. Use this if the length of the input gate should be ignored. |
| `inputedge` (`ie`) | gate | | Input edge: Use this if every low/high or high/low transition should count as a trigger. |
| `gatelength` (`gl`) | CV | | Sets the length of the gate of `outputgate` in seconds. If you use `taptempo` the length is in fractions of a clock tick, instead. |
| `gatestretch` (`s`) | CV | `0.0` | Makes the output gate longer than the input gate by the given percentage. This parameter is ignored if `gatelength` is used. |
| `mingatelength` (`m`) | CV | `0.01` | Defines a minimum length of the output gate in seconds or clock ticks. |
| `maxgatelength` (`x`) | CV | | Defines a maximum length of the output gate in seconds or clock ticks. |
| `taptempo` (`tt`) | gate | | If you patch a reference clock here, `gatelength`, `mingatelength` and `maxgatelength` are fractions of one clock tick, not seconds anymore. Please see the manual ([basics](../basics.md)) for details on using `taptempo` inputs. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `outputgate` (`og`) | gate | Outputs a gate with controllable length for every gate, trigger or edge event. |
| `outputtrigger` (`ot`) | trigger | Outputs a 10 ms trigger for every gate, trigger or edge event. |
| `outputedge` (`oe`) | gate | Toggle between 0 and 1 at every gate, trigger or edge event. |

## See also

- [`clocktool`](clocktool.md) — more comprehensive clock manipulation, including clock division.
