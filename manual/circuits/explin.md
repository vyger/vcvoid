---
circuit: explin
title: Exponential to linear converter
obsolete: false
ram_bytes: 32
manual_pages: [245, 246]
category: conversion
tags: [exponential, linear, converter, envelope, curve, mix, startvalue, endvalue]
see_also: []
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Stateless exp-to-linear transfer function driven by startvalue/endvalue/mix; pure math.
verification_note: "Doc gives worked input/output voltage pairs; verify the transfer curve and mix blend headlessly against a computed reference."
---

# explin — Exponential to linear converter

This circuit converts an exponential input curve into a linear output curve.
Image you have an analog envelope outputting an exponential curve. Such a curve
might start at 8 V and reach 0.5 V at about 500 ms later.

The following droid patch will convert this into a linear curve:

```droid
[explin]
    input   = I1
    output = O2
    startvalue = 8V
    endvalue = 0.5V
```

With the values `startvalue` and `endvalue` you configure how this translation
is scaled. The `startvalue` selects the voltage where the exponential input
curve and the linear output curve should be the same. If the input is an
envelope voltage then `startvalue` would be the start or maximum voltage of that
envelope.

A falling exponential curve will never reach 0 in theory. So with `endvalue` you
set a value (or voltage) in that you consider the curve to be low enough to be
inaudible. At that voltage the linear output will exactly be zero. This voltage
can be used to control the slope of the linear output curve. Smaller `endvalue`
voltages stretch the linear output out over a longer time, while larger values
make it reach zero sooner.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | CV | `0.0` | Patch an exponential envelope output or a similar signal here. This value must be positive or otherwise it will be set to `0.0`. |
| `startvalue` (`sv`) | CV | `1.0` | The assumed maximum value of the input signal (the start voltage from where it decays in an exponential way). |
| `endvalue` (`ev`) | CV | `0.01` | The value at which it is assumed to be zero (at which the linear output will be set to zero). This value must be positive. It is forced to be >= `0.001`. |
| `mix` (`m`) | `0..1` | `1.0` | Sets the mix between the "dry" and "wet" signal: At `0.0` the output is the same as the input. At `1.0` the output is the linear curve. At a value in between it is some average. You are even allowed to used values > `1.0`. A value of `2.0` will overcompensate and bend the curve beyond linearity into a curve some modularists would call *logarithmic*. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | Here comes the resulting linear output. |
