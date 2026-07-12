---
circuit: bernoulli
title: Random gate distributor
obsolete: false
ram_bytes: 32
manual_pages: [151]
category: random
tags: [random, bernoulli, gate, distributor, probability, coin-toss, routing]
see_also: []
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: "Trivial coin-toss router that copies the input to one of two outputs per trigger edge."
verification_note: "Headless: verify the chosen output receives an exact copy and the other is 0 V; routing is random so validate the split statistically against the distribution parameter."
---

# bernoulli — Random gate distributor

This circuit implements a "bernoulli gate". For each gate or trigger received at
`input` there is made a random decision of whether to forward that gate to
`output1` or `output2`. The probability for each of the outputs can be shifted
with the parameter `distribution`. It determines the probability of a gate
signal to go to `output1`.

## Example

```droid
[bernoulli]
    input        = G1
    distribution = P1.1
    output1      = G2
    output2      = G4
```

Note: each time a positive trigger edge is seen at `input` a new random decision
is made for which output to use. From now on that chosen output gets an exact
copy of the input signal – even if it is not a simple trigger signal but
something more complex like an envelope. The other output will send 0 V.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | gate | `0` | Send gate or trigger signals here. |
| `distribution` (`di`) | 0..1 (mid 0.5) | `0.5` | This controls the probability of a gate to be forwarded to `output1`. A value of `0.5` means 50%. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output1` (`o1`) | gate | Gates from input are forwarded here if the random decision was in favour of output 1. |
| `output2` (`o2`) | gate | Gates from input are forwarded here if the random decision was in favour of output 2. |
