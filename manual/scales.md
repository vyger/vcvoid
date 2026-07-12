# DROID Manual — Musical Scales (Chapter 15)

## 15. Musical scales

Here you find all possible 108 values that you can use in the `degree` input of
various circuits like [`minifonion`](circuits/minifonion.md) (see page 308) or
[`chord`](circuits/chord.md) (see page 169). This table might indeed look
strange. The reason is that it reflects the internal scale structure of the
Sinfonion. Every 12 scales refer to one mode of the Sinfonion. This is also the
reason why some scales appear twice. All scales are noted in the assumption that
`root` is set to `0`, which is a C.

| Nr | Scale | I | II | III | IV | V | VI | VII | fill1 | fill2 | fill3 | fill4 | fill5 |
|----|-------|---|----|-----|----|----|----|-----|-------|-------|-------|-------|-------|
| 0 | C Lydian | C | D | E | F♯ | G | A | B | C♯ | D♯ | F | G♯ | A♯ |
| 1 | C Ionian | C | D | E | F | G | A | B | C♯ | D♯ | F♯ | G♯ | A♯ |
| 2 | C Mixolydian | C | D | E | F | G | A | A♯ | C♯ | D♯ | F♯ | G♯ | B |
| 3 | C Mixolydian sus4 | C | D | F | E | G | A | A♯ | C♯ | D♯ | F♯ | G♯ | B |
| 4 | C Altered | C | C♯ | E | D♯ | G♯ | F♯ | A♯ | D | F | G | A | B |
| 5 | C Spanish | C | C♯ | E | F | G | G♯ | A♯ | D | D♯ | F♯ | A | B |
| 6 | C Dorian | C | D | D♯ | F | G | A | A♯ | C♯ | E | F♯ | G♯ | B |
| 7 | C Aeolian | C | D | D♯ | F | G | G♯ | A♯ | C♯ | E | F♯ | A | B |
| 8 | C Harmonic major | C | D | D♯ | F | G | G♯ | B | C♯ | E | F♯ | A | A♯ |
| 9 | C Phrygian | C | C♯ | D♯ | F | G | G♯ | A♯ | D | E | F♯ | A | B |
| 10 | C Diminished | C | D | D♯ | F | F♯ | G♯ | A | B | C♯ | E | G | A♯ |
| 11 | C Augmented | C | D | E | F♯ | G♯ | C♯ | A♯ | D♯ | F | G | A | B |
| 12 | C Mixolydian with ♯11 | C | D | E | F♯ | G | A | A♯ | C♯ | D♯ | F | G♯ | B |
| 13 | C Mixolydian with ♯11 | C | D | E | F♯ | G | A | A♯ | C♯ | D♯ | F | G♯ | B |
| 14 | C Mixolydian with ♭13 | C | D | E | F | G | G♯ | A♯ | C♯ | D♯ | F♯ | A | B |
| 15 | C Mixolydian with ♭13 | C | D | E | F | G | G♯ | A♯ | C♯ | D♯ | F♯ | A | B |
| 16 | C Mixo sus 4 ♭9 | C | C♯ | F | E | G | A | A♯ | D | D♯ | F♯ | G♯ | B |
| 17 | C Mixo sus 4 ♭9 | C | C♯ | F | E | G | A | A♯ | D | D♯ | F♯ | G♯ | B |
| 18 | C Mixolydian with ♭9 | C | C♯ | E | D♯ | G | F♯ | A♯ | A | D | F | G♯ | B |
| 19 | C Mixolydian with ♭9 | C | C♯ | E | D♯ | G | F♯ | A♯ | A | D | F | G♯ | B |
| 20 | C Melodic minor | C | D | D♯ | F | G | A | B | C♯ | E | F♯ | G♯ | A♯ |
| 21 | C Melodic minor | C | D | D♯ | F | G | A | B | C♯ | E | F♯ | G♯ | A♯ |
| 22 | C Major 7 ♯5 ♯11 | C | D | E | F♯ | G♯ | A | B | C♯ | D♯ | F | G | A♯ |
| 23 | C Locrian | C | C♯ | D♯ | F | F♯ | G♯ | A♯ | D | E | G | A | B |
| 24 | Slashchord D♭♯11/C | C | D♯ | C♯ | G | F | A♯ | G♯ | D | E | F♯ | A | B |
| 25 | Slashchord D/C | C | E | D | G | F♯ | B | A | C♯ | D♯ | F | G♯ | A♯ |
| 26 | Slashchord E♭/C | C | D | D♯ | F | G | G♯ | A♯ | C♯ | E | F♯ | A | B |
| 27 | Slashchord E♭♯11/C | C | D | D♯ | F | G | A | A♯ | C♯ | E | F♯ | G♯ | B |
| 28 | Slashchord E/C | C | D | E | F | G♯ | A | B | C♯ | D♯ | F♯ | G | A♯ |
| 29 | Slashchord G/C | C | F | G | A | B | E | D | C♯ | D♯ | F♯ | G♯ | A♯ |
| 30 | Slashchord B♭/C | C | A | A♯ | E | D | G | F | C♯ | D♯ | F♯ | G♯ | B |
| 31 | Slashchord D minor/C | C | E | D | G | F | A♯ | A | C♯ | D♯ | F♯ | G♯ | B |
| 32 | Slashchord E♭ minor/C | C | C♯ | D♯ | F | F♯ | G♯ | A♯ | D | E | G | A | B |
| 33 | Slashchord E minor/C | C | D | E | F | G | A | B | C♯ | D♯ | F♯ | G♯ | A♯ |
| 34 | Slashchord G minor/C | C | F | G | A | A♯ | E | D | C♯ | D♯ | F♯ | G♯ | B |
| 35 | Slashchord B♭ aug / C | C | A | A♯ | E | D | G | F♯ | C♯ | D♯ | F | G♯ | B |
| 36 | C Major | C | D | E | F | G | A | B | C♯ | D♯ | F♯ | G♯ | A♯ |
| 37 | C♯ Major | C♯ | D♯ | F | G | G♯ | A♯ | C | D | E | F♯ | A | B |
| 38 | D Dorian minor | D | E | F | G | A | B | C | D♯ | F♯ | G♯ | A♯ | C♯ |
| 39 | E♭ Mixolydian | D♯ | F | G | G♯ | A♯ | C | C♯ | E | F♯ | A | B | D |
| 40 | E Phrygian minor | E | F | G | A | B | C | D | F♯ | G♯ | A♯ | C♯ | D♯ |
| 41 | F Lydian major | F | G | A | B | C | D | E | F♯ | G♯ | A♯ | C♯ | D♯ |
| 42 | F♯ Altered | F♯ | G | A♯ | A | D | C | E | G♯ | B | C♯ | D♯ | F |
| 43 | G Mixolydian | G | A | B | C | D | E | F | G♯ | A♯ | C♯ | D♯ | F♯ |
| 44 | G♯ Altered | G♯ | A | C | B | E | D | F♯ | A♯ | C♯ | D♯ | F | G |
| 45 | A Aeolian minor | A | B | C | D | E | F | G | A♯ | C♯ | D♯ | F♯ | G♯ |
| 46 | B♭ Major | A♯ | C | D | E | F | G | A | B | C♯ | D♯ | F♯ | G♯ |
| 47 | B Locrian | B | C | D | E | F | G | A | C♯ | D♯ | F♯ | G♯ | A♯ |
| 48 | C Minor from I | C | D | D♯ | F | G | G♯ | A♯ | C♯ | E | F♯ | A | B |
| 49 | C♯ Minor from I | C♯ | D♯ | F | G | G♯ | A♯ | C | D | E | F♯ | A | B |
| 50 | C Minor from II | D | D♯ | F | G | G♯ | A♯ | C | E | F♯ | A | B | C♯ |
| 51 | C Minor from III | D♯ | F | G | G♯ | A♯ | C | D | E | F♯ | A | B | C♯ |
| 52 | E major | E | F♯ | G♯ | A♯ | B | C♯ | D♯ | F | G | A | C | D |
| 53 | C Minor from IV | F | G | G♯ | A♯ | C | D | D♯ | F♯ | A | B | C♯ | E |
| 54 | F♯ Altered | F♯ | G | A♯ | A | D | C | E | G♯ | B | C♯ | D♯ | F |
| 55 | C Minor from V | G | G♯ | B | C | D | D♯ | F | A | A♯ | C♯ | E | F♯ |
| 56 | C Minor from VI | G♯ | A♯ | C | D | D♯ | F | G | A | B | C♯ | E | F♯ |
| 57 | A Altered | A | A♯ | C♯ | C | F | D♯ | G | B | D | E | F♯ | G♯ |
| 58 | C Minor from VII | A♯ | C | D | D♯ | F | G | G♯ | B | C♯ | E | F♯ | A |
| 59 | B diminished | B | C♯ | D | E | F | G | G♯ | C | D♯ | F♯ | A | A♯ |
| 60 | C Melodic minor from I | C | D | D♯ | F | G | A | B | C♯ | E | F♯ | G♯ | A♯ |
| 61 | C♯ Altered | C♯ | D | F | E | A | G | B | D♯ | F♯ | G♯ | A♯ | C |
| 62 | C Melodic minor from II | D | D♯ | F | G | A | B | C | E | F♯ | G♯ | A♯ | C♯ |
| 63 | C Melodic minor from III | D♯ | F | G | A | B | C | D | E | F♯ | G♯ | A♯ | C♯ |
| 64 | E major | E | F♯ | G♯ | A♯ | B | C♯ | D♯ | F | G | A | C | D |
| 65 | C Melodic minor from IV | F | G | A | B | C | D | D♯ | F♯ | G♯ | A♯ | C♯ | E |
| 66 | F♯ Altered | F♯ | G | A♯ | A | D | C | E | G♯ | B | C♯ | D♯ | F |
| 67 | C Melodic minor from V | G | A | B | C | D | D♯ | F | G♯ | A♯ | C♯ | E | F♯ |
| 68 | C Minor from VI | G♯ | A♯ | C | D | D♯ | F | G | A | B | C♯ | E | F♯ |
| 69 | C Melodic minor from VI | A | B | C | D | D♯ | F | G | A♯ | C♯ | E | F♯ | G♯ |
| 70 | C Minor from VII | A♯ | C | D | D♯ | F | G | G♯ | B | C♯ | E | F♯ | A |
| 71 | C Melodic minor from VII | B | C | D | D♯ | F | G | A | C♯ | E | F♯ | G♯ | A♯ |
| 72 | C Harmonic minor from I | C | D | D♯ | F | G | G♯ | B | C♯ | E | F♯ | A | A♯ |
| 73 | C♯ Altered | C♯ | D | F | E | A | G | B | D♯ | F♯ | G♯ | A♯ | C |
| 74 | C Harmonic minor from II | D | D♯ | F | G | G♯ | B | C | E | F♯ | A | A♯ | C♯ |
| 75 | C Harmonic minor from III | D♯ | F | G | G♯ | B | C | D | E | F♯ | A | A♯ | C♯ |
| 76 | E major | E | F♯ | G♯ | A♯ | B | C♯ | D♯ | F | G | A | C | D |
| 77 | C Harmonic minor from IV | F | G | G♯ | B | C | D | D♯ | F♯ | A | A♯ | C♯ | E |
| 78 | F♯ Altered | F♯ | G | A♯ | A | D | C | E | G♯ | B | C♯ | D♯ | F |
| 79 | C Harmonic minor from V | G | G♯ | B | C | D | D♯ | F | A | A♯ | C♯ | E | F♯ |
| 80 | C Harmonic minor from VI | G♯ | B | C | D | D♯ | F | G | A | A♯ | C♯ | E | F♯ |
| 81 | A Altered | A | A♯ | C♯ | C | F | D♯ | G | B | D | E | F♯ | G♯ |
| 82 | C Minor from VII | A♯ | C | D | D♯ | F | G | G♯ | B | C♯ | E | F♯ | A |
| 83 | C Harmonic minor from VII | B | C | D | D♯ | F | G | G♯ | C♯ | E | F♯ | A | A♯ |
| 84 | C Harmonic major from I | C | D | E | F | G | G♯ | B | C♯ | D♯ | F♯ | A | A♯ |
| 85 | C♯ Major | C♯ | D♯ | F | G | G♯ | A♯ | C | D | E | F♯ | A | B |
| 86 | C Harmonic major from II | D | E | F | G | G♯ | B | C | D♯ | F♯ | A | A♯ | C♯ |
| 87 | C Harmonic minor from III | D♯ | F | G | G♯ | B | C | D | E | F♯ | A | A♯ | C♯ |
| 88 | C Harmonic major from III | E | F | G | G♯ | B | C | D | F♯ | A | A♯ | C♯ | D♯ |
| 89 | C Harmonic major from IV | F | G | G♯ | B | C | D | E | F♯ | A | A♯ | C♯ | D♯ |
| 90 | F♯ Altered | F♯ | G | A♯ | A | D | C | E | G♯ | B | C♯ | D♯ | F |
| 91 | C Harmonic major from V | G | G♯ | B | C | D | E | F | A | A♯ | C♯ | D♯ | F♯ |
| 92 | C Harmonic major from VI | G♯ | B | C | D | E | F | G | A♯ | C♯ | D♯ | F♯ | A |
| 93 | A Aeolian minor | A | B | C | D | E | F | G | A♯ | C♯ | D♯ | F♯ | G♯ |
| 94 | B♭ Major | A♯ | C | D | E | F | G | A | B | C♯ | D♯ | F♯ | G♯ |
| 95 | C Harmonic major from VII | B | C | D | E | F | G | G♯ | C♯ | D♯ | F♯ | A | A♯ |
| 96 | C Double harmonic major from I | C | C♯ | E | F | G | G♯ | B | D | D♯ | F♯ | A | A♯ |
| 97 | C Double harmonic major from II | C♯ | E | F | G | G♯ | B | C | D | D♯ | F♯ | A | A♯ |
| 98 | C♯ Double harmonic major from II | D | E | F | G | G♯ | B | C | D♯ | F♯ | A | A♯ | C♯ |
| 99 | B Double harmonic major from III | D♯ | F | G | G♯ | B | C | C♯ | E | F♯ | A | A♯ | D |
| 100 | C Double harmonic major from III | E | F | G | G♯ | B | C | C♯ | F♯ | A | A♯ | D | D♯ |
| 101 | C Double harmonic major from IV | F | G | G♯ | B | C | C♯ | E | F♯ | A | A♯ | D | D♯ |
| 102 | B Double harmonic major from V | F♯ | G♯ | B | C | C♯ | E | F | G | A | A♯ | D | D♯ |
| 103 | C Double harmonic major from V | G | G♯ | B | C | C♯ | E | F | A | A♯ | D | D♯ | F♯ |
| 104 | C Double harmonic major from VI | G♯ | B | C | C♯ | E | F | G | A | A♯ | D | D♯ | F♯ |
| 105 | C♯ Double harmonic major from VI | A | B | C | C♯ | E | F | G | A♯ | D | D♯ | F♯ | G♯ |
| 106 | B Double harmonic major from VII | A♯ | C | C♯ | E | F | G | G♯ | B | D | D♯ | F♯ | A |
| 107 | C Double harmonic major from VII | B | C | C♯ | E | F | G | G♯ | D | D♯ | F♯ | A | A♯ |
