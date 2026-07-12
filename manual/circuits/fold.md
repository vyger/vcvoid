---
circuit: fold
title: CV folder – keep (pitch) CV within certain bounds
obsolete: false
ram_bytes: 32
manual_pages: [260, 261]
category: utility
tags: [fold, cv, pitch, octave, range, bounds, minimum, maximum, wrap]
see_also: [lfo]
impl_difficulty: easy
controller_binding: master-only
verification: headless
spec_gap: false
difficulty_note: Stateless CV folder adding/subtracting foldby until within min/max, including the two documented asymmetric anomalies.
verification_note: "Doc lists exact input/output voltage pairs and both anomaly cases (range < foldby, min > max); verify the fold algorithm headlessly against a computed reference."
---

# fold — CV folder – keep (pitch) CV within certain bounds

This circuit can keep an incoming CV within defined bounds, but not by limiting
to these bounds, but by *folding* it in case it exceeds these bounds.

The main application is keeping the pitch of a voice within a certain range by
octaving it up and down when necessary. Octaving keeps the actual note value.
Here is an example for that application:

```droid
[fold]
    input = I1
    output = O1
    foldby = 1V # one octave
    minimum = 1.2V
    maximum = 2.5V
```

If the input value at `I1` is going below 1.2 V, 1 V will be added over and over
until the output voltage is at least 1.2 V. So the upper example will convert as
follows:

- 0.7 V → 1.7 V
- 2.0 V → 2.0 V
- -4.3 V → 1.7 V
- 4.4 V → 2.4 V

If you apply that to a bass voice, you make sure that it never goes too low or
too high, which is helpful if that voice is the result of a combination of
sequences, random numbers, transpositions and other funny generative ideas.

Note: If you do not specify `minimum` or `maximum`, no folding will take place
at that boundary. If you specify neither of them, this circuit is completely
useless.

## Anomalies

Two anomalies can happen if the parameters are a bit "crazy". This first one
happens when the space between `minimum` and `maximum` is less than one
`foldby`. Consider the following example:

```droid
[fold]
    input = I1
    output = O1
    foldby = 1V
    minimum = 1.1V
    maximum = 1.3V
```

Now if the input voltage is e.g. 1.0 V, it will be folded up to 2.0 V, which is
then above the maximum range. But it will stay there, since there is no way to
fold it into the range anyway.

The second anomaly is if `minimum` is greater than `maximum`. Look:

```droid
[fold]
    input = I1
    output = O1
    foldby = 1V
    minimum = 2.5V
    maximum = 1.5V
```

Here any voltage below 2.5 V will be folded up until it is above that value. So
2.4 V will be folded to 3.4 V. Well, you could also argue that because 2.4 V is
also above the maximum value it should get folded down instead. While that is
true, `fold` behaves asymmetrical here and gives folding up the precedence.

But why would you set such strange parameters? Well, because they can be CVs of
course. Try the following patch and send the output `O1` to the pitch input of
a voice:

```droid
[p2b8]
[p2b8]

[lfo]
    hz = 2 * P1.1
    triangle = _CV

[lfo]
    hz = 2 * P1.2
    triangle = _MIN

[lfo]
    hz = 2 * P2.1
    triangle = _MAX

[lfo]
    hz = 2 * P2.2
    triangle = _FOLDBY
    level = 2V

[fold]
    input = _CV
    minimum = _MIN
    maximum = _MAX
    foldby = _FOLDBY
    output = O1

[lfo]
    rate = O1 * 0.2
    hz = 110
    output = O2
```

Here all four inputs are from slowly running LFOs and funny things happen. Play
with the four pots and you will get all sorts of very interesting random
patterns.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `input` (`i`) | CV | `0.0` | Input CV to be folded. |
| `foldby` (`f`) | CV | `0.1` | Amount to be added or substracted from the input CV if it is not within the allowed range. This CV must be positive. If it is negative or zero, no folding will be done. |
| `minimum` (`m`) | CV | `☞ smart` | Lower bound of the allowed range. If unpatched, no lower bound will be applied. |
| `maximum` (`x`) | CV | `☞ smart` | Upper bound of the allowed range. If unpatched, no upper bound will be applied. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `output` (`o`) | CV | Folded output voltage. |
