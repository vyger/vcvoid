# 3. DB8E custom display layouts as a tagged variant

Date: 2026-10-03
Status: Accepted
Issue: [#22](https://github.com/vyger/vcvoid/issues/22) (Group C)

## Context

`DisplayState` (`engine/src/controllerstate.hpp`) is the engine's symbolic model
of one DB8E's 128×64 OLED. Since [#19](https://github.com/vyger/vcvoid/issues/19)
it has been exactly one shape:

> `header + (text | value + numbermode/fontsize)`

with `bool isText` picking between the two bodies. That shape covers the
`[display]` circuit and the whole plain-value tier (`encoder`, `pot`,
`motorfader`, `nudge`, `buttongroup`, `algoquencer`) plus — after
[#89](https://github.com/vyger/vcvoid/pull/89) — the bank circuits, which pick
*which* value to show but still show a value.

`manual/hardware.md` §6.12 says plainly that this is not the whole story:

> Some circuits like `motoquencer`, `calibrator` or `vcotuner` even have a more
> fancy custom display layout. They do not simply display a *value* but
> something more complex. … In such a case the buttons 1, 2, 3, 4 and 6 don't
> have any function.

Eight circuits are in that group: `button`, `recorder`, `notebuttons`,
`encoquencer`, `motoquencer`, `calibrator`, `vcotuner`, `outputcalibrator`.
None of them fits `header + (text | value)`:

- `button` draws, for `states = 3`/`4`, a chain of `states` bubbles joined by
  short horizontal segments with the current state's bubble filled solid
  (measured on hardware, #19). The numeric state *is* available, but putting
  `2` on the screen where hardware draws a diagram is a different screen, not a
  rougher rendering of the same one.
- `recorder` shows the plain words **Recording** / **Playback** / **Bypass**.
  The body shape fits — it is text — but the mechanism does not: the engine's
  text table is interned *from the patch*, and those three literals never appear
  in a patch.
- The remaining six draw note names, graphs, tuner needles and box grids.

Two further constraints shape the answer.

**The goldens must stay symbolic.** `engine/circuits/display.cpp` states the
contract: "goldens here assert WHAT should be on screen, never HOW it looks".
Pixel geometry is Rack-side and human-verified. Whatever we add has to be
assertable as *content*, so a layout cannot be modelled as a bag of
pre-rendered pixels, a pre-formatted string, or a draw list.

**The layout set is versioned on the hardware.** hardware.md §6.13:

> if the master sends display contents to the DB8E that the latter cannot
> understand, it shows a screen like `update firmware`.

So the real protocol between master and DB8E is already a *tagged* one: the
master sends a layout identifier plus that layout's parameters, and a DB8E whose
firmware predates the tag renders the fallback screen. Our model should have the
same shape as the thing it emulates.

## Decision

Make `DisplayState` a **tagged variant**: one `DisplayLayout` enum tag plus a
small, named, per-layout payload. The tag says which layout the master is
sending; the payload carries that layout's *parameters*, symbolically.

```cpp
enum class DisplayLayout : uint8_t {
    Value   = 0,   // header + number (+ numbermode/fontsize)   [display], Groups A/B
    Text    = 1,   // header + one interned text                [display], recorder
    Bubbles = 2,   // header + a chain of `count` bubbles, `index` filled   button
};

struct DisplayState {
    bool active;
    int  headerText;              // shared by every layout: 0 = none
    DisplayLayout layout;
    // --- payloads, each read only when `layout` selects it ---
    float   value; uint8_t numbermode, fontsize;   // Value
    int     bodyText;                              // Text
    struct { uint8_t count, index; } bubbles;      // Bubbles
    ...                                            // owner / tier / linger
};
```

Four rules make this work and keep working:

1. **The tag replaces `isText`.** There is no "is it text?" boolean any more;
   every reader switches on `layout`. A reader that does not know a tag must
   render the DB8E's own "update firmware" fallback rather than guess — the
   hardware behaviour, for free, and it is what makes an unknown tag safe.
2. **The header stays outside the variant.** Every documented layout, custom
   ones included, sits under the ordinary derived-or-explicit header
   (hardware.md's `calibrator` screenshot, button.md's bubbles, motoquencer's
   "Pitch"). So `headerText` and the whole `header`/auto-header machinery from
   #19 and #89 are shared, and a new layout only has to describe its body.
3. **Payloads are parameters, never pixels.** `button` sends `(count, index)`,
   not two circle positions; `vcotuner` will send `(semitones, cents, hertz)`,
   not a needle angle. The golden asserts the parameters; `plugin/src/DB8E.cpp`
   decides radii, spacing and fonts. This is the #19 contract extended, not
   amended.
4. **Payload fields are flat and named, not a `union`.** The whole struct is
   tens of bytes, it crosses the Rack expander boundary as plain old data in
   `chain::DownstreamBlock`, and a union would buy nothing but aliasing hazards
   in the relay copy. The tag is the discipline; the storage is boring.

### The load-time seam for circuit-provided strings

`recorder`'s three words are not in the patch, so the text table has to grow a
second source. The engine already has the precedent: #19 derives auto-headers at
load and interns them into the same table, and #89 interns one per bank element.
This adds the third and last kind — strings the **circuit** provides:

```cpp
// circuit.hpp
using TextInterner = std::function<int(const std::string&)>;
virtual void internTexts(const TextInterner&) {}   // default: no-op
```

`Engine::load()` calls it once per circuit, right after `allocateSlots()` and the
auto-header derivation, handing a closure over the engine's `texts_`. A circuit
caches the returned text numbers in its own members and thereafter behaves like
any other text user — `textForNumber()` resolves them, `expectdisplay … text`
asserts them, and the Rack feed sends them as ASCII without knowing where they
came from. `internText` deduplicates, so a patch with five `recorder`s interns
"Recording" once.

Three reasons this is a load-time hook on `Circuit` rather than a built-in
string table in the engine:

- The strings belong to the circuit that shows them, next to the code that
  chooses between them — a central table would be a second place to keep in sync
  with `manual/circuits/*.md`.
- It costs nothing for the ~70 circuits that do not override it, and nothing per
  tick for the ones that do.
- `encoquencer` needs the identical seam for "silent"/"play" and the names it
  falls back to when `cvname`/`gatename` are unpatched ("CV", "Number", "Gate"),
  so the second user is already known.

### The helper split

`ui::showCircuitValue()` grew out of a single shape and wrote header,
arbitration and body in one function. It splits in two:

- `ui::claimCircuitScreen(c, s, autoHeaderText)` — resolve the target DB8E,
  arbitrate (owner / expired linger / same-tick equal-or-higher tier), stamp
  `active`, the header, owner, tier and tick, and return the `DisplayState*`
  on acceptance or `nullptr` on refusal. This is the part every layout shares,
  and it is where the precedence rule from hardware.md §6.12 lives.
- One thin writer per layout on top: `showValueWithHeader` / `showCircuitValue`
  (unchanged signatures, so every Group A and Group B call site is untouched),
  `showCircuitText`, `showStateBubbles`.

A new layout is therefore: an enum value, a payload field, a writer of three
lines, a `case` in the Rack renderer, and an `expectdisplay` field. The
arbitration, the header and the suppression rules are never re-implemented.

### What ships now

`button` and `recorder` only — the two the manual and hardware measurements pin
down precisely enough to golden.

- **`button`** uses `Bubbles` when `states` is 3 or 4 (button.md: "automatically
  displays it's state, whenever `states = 3` or `states = 4`"), writing
  `(count = states, index = state)` under its ordinary derived header. `states`
  1 and 2 write nothing at all — the manual restricts the feature, and a 2-bubble
  chain is a worse toggle indicator than the button's own LED.
- **`recorder`** uses `Text` on a transport-mode change: RECORD → "Recording",
  PLAY → "Playback", STOP → "Bypass". The third mapping is the manual's own
  wording for idle ("The circuit starts in idle / stopped mode and `L1.3` is lit.
  In that mode the input is bypassed to the output").

Both follow the tier's existing rules: select-gated, `display = 0` suppresses,
the first tick is swallowed so a merely loaded patch leaves the screen dark, and
a refused write stays pending and re-attempts (delay-not-discard).

### How the remaining six map on

Sketched here so the variant is judged against all of them, not just the two
being built. None of these is implemented; #22 stays open for them.

| Circuit | Tag | Payload | Notes |
|---|---|---|---|
| `notebuttons` | `NoteName` | `semitone` (int), `withOctave` (bool) | **Shipped** (after this ADR's first cut). notebuttons.md: "automatically displays the selected note". Header is the `output` target, or `semitone`'s if `output` is unpatched — a *second* auto-header source, implemented as the per-circuit `Circuit::autoHeaderFallbackJack()` rather than a hard-coded `output`. The note *spelling* is rendering: the payload is the number. `withOctave` was added when building it: notebuttons' number is a bare pitch class 0..11, while encoquencer's will be a pitch, and the screen cannot tell `0` = "C" from `0` = "C0" without being told. |
| `encoquencer` | `NoteName` / `Text` / `Value` | as above | encoquencer.md: the edited step's value, titled "CV"/"Number"/"Gate" or the patch's `cvname`/`gatename`. A gate step is the `Text` layout with "silent"/"play" (or "on"/"off" when `gatename` is set) — circuit-provided strings again, through the same `internTexts` seam. This is the circuit with *no* `header` jack, which the tag handles naturally: the title is per-edit, chosen by the circuit. |
| `motoquencer` | `NoteName` / `Text` / `Value` | as above | Same family and the same payloads; the harder half is deciding what counts as "edit something in the sequencer" across the circuit's many controls, not the screen model. |
| `calibrator` | `Graph` | `points[N]` (correction per octave), `cursorOctave`, `cursorValue`, `dottedBelow` | hardware.md §6.12 and calibrator.md: an envelope-like graph with the octave numbers along the bottom, a cross at the current input pitch, and a dotted segment where the correction is exactly zero. All of that is numbers; the curve, the ticks and the cross are drawing. `forcedisplay = 1` makes it write every tick instead of only on a nudge. |
| `vcotuner` | `Tuner` | `semitone`, `cents`, `hertz`, `inTune` (bool), `signal` (bool) | vcotuner.md: reference note, cents left or right, frequency in Hz, a pointer that thickens inside `precision`, and "no signal" when the probe loses lock. `inTune` is payload rather than a Rack-side comparison because `precision` is a patch input and the threshold is engine semantics. |
| `outputcalibrator` | `Boxes` | two rows of per-cell "differs from default" flags, the selected cell, the selected point's deviation, `unsaved` (bool) | outputcalibrator.md: two rows of boxes, one dotted, a deviation number and the word "CHANGED". The word is a rendering of `unsaved`, not a string in the payload. |

Nothing above needs a second tag dimension, a nested variant or a payload that
is not a fixed-size struct of numbers and interned text numbers. That is the
evidence the tag is at the right altitude.

### Firmware versioning

The DB8E's layout set is versioned (hardware.md §6.13). We do not model a DB8E
firmware version — vcvoid's DB8E is always current, and faking an out-of-date
one would be a toggle nobody wants. What we *do* adopt is the behaviour that
makes versioning safe: `plugin/src/DB8E.cpp` renders an explicit **"update
firmware"** screen for any tag it does not recognise, rather than falling
through to the value renderer. That turns an engine/plugin version skew (a
plugin half-upgraded, a `.vcvplugin` mismatched against a dylib) into exactly
the message the hardware would show, and it means an engine can ship a new tag
before the renderer catches up without drawing nonsense.

## Consequences

**Good.**

- The engine model now has the same shape as the master→DB8E protocol it
  emulates, including its failure mode.
- Goldens stay symbolic: `expectdisplay D1 bubbles 4 2` asserts a four-state
  button sitting on state 2, and says nothing about circles.
- Adding a layout no longer touches arbitration, headers, suppression or
  select-gating — they are in `claimCircuitScreen` once.
- Circuit-provided strings have a seam, which unblocks `encoquencer` and
  `motoquencer` as much as it does `recorder`.

**Costs.**

- `DisplayState` grows a few bytes per payload kind, and `chain::DownstreamBlock`
  carries them per DB8E on every relay frame. Measured in tens of bytes against
  a multi-KB block; not worth a union.
- Three places learn each new tag: the engine writer, the relay copy and the
  Rack renderer. The "update firmware" fallback keeps a forgotten third place
  from being silent, but it is still three.
- `isText` is gone from `DisplayState`, so `[display]` and the two plugin
  readers changed in this commit. A one-time cost, taken now while there are
  five readers rather than fifteen.

**Deliberately not done.**

- No per-layout formatting in the engine. "E♭1", "3.2 cents", "CHANGED" and the
  17-character header cut all live at the screen, where #19 put the header cut.
- No modelled DB8E firmware version or downgrade toggle.
- No layouts beyond `button` and `recorder` in the first cut (`notebuttons`
  followed). The rest sketched above are real work — measurement, not
  mechanism — and #22 stays open for them.
