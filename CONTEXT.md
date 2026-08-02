# vcvoid — domain glossary

The words this project uses, and what they mean *here*. DROID's own vocabulary
(circuit, patch, register, controller, master) is defined by the hardware and
documented in [`manual/`](manual/README.md); this file records the terms whose
meaning is specific to vcvoid, plus the DROID terms we have found people
routinely confuse.

## vcvoid terms

**Experimental circuit** — a circuit implemented in vcvoid that does not exist
in any DROID firmware and is unknown to the Droid Forge. A patch using one will
not run on hardware. Experimental circuits are declared in the overlay,
documented under `manual/circuits/experimental/`, and refused at patch load
unless experimental mode is on. See
[ADR 0001](docs/adr/0001-experimental-circuits.md).

**Experimental mode** — the per-module opt-in ("Allow experimental circuits" in
a master's context menu) that allows a patch containing experimental circuits to
load. Off by default, saved with the Rack patch. Distinct from *ignore memory
limits*, the other opt-out from hardware fidelity.

**Overlay** — `engine/experimental.json`: the repo-owned declaration of
experimental circuits, written in the Forge's own firmware-file schema. One file
feeds both consumers (the engine's generated jack tables and droidcheck's
embedded firmware), so the two can never disagree about what exists.

**Hi-res CC pair** — the two MIDI messages that carry one 14-bit continuous
controller: a *coarse* byte on controller `n` and a *fine* byte on controller
`n + 32`, defined for `n` = 0…31 only. In vcvoid the pair is also a unit of
emission: both bytes leave in one update, adjacent on the wire, never
interleaved with another controller's. "MSB/LSB" is the same distinction in
MIDI's own words. See
[`midihirescc`](manual/circuits/experimental/midihirescc.md).

**Golden** — a `.gold` file: patch text plus expected register values at
specific engine ticks. The project's primary test seam; assertions are made at
the patch boundary, never against circuit internals.

**Ledger** — `circuits-status.yaml`: the per-circuit implementation record
(status, difficulty, verification, notes), generated from manual frontmatter
with human notes preserved.

**SPEC-GAP** — a place where the DROID manual is silent or imprecise and the
engine had to choose a literal reading. Recorded in the circuit's source header
and its ledger notes, so the choice is visible and revisable if hardware
behaviour is ever characterised.

## DROID terms worth pinning

**Chaining vs. linking** — not interchangeable, and the distinction has already
caused one design misunderstanding:

- **Chaining** (`chaintonext`, as on [`sequencer`](manual/circuits/sequencer.md)
  and `trigseq`) **concatenates** instances into one *longer* sequence. The head
  owns the transport and outputs; followers contribute only steps.
- **Linking** (`linktonext`, as on
  [`motoquencer`](manual/circuits/motoquencer.md)) runs instances **in
  parallel** on the *same* step number, giving each step more outputs.

"Longer sequence" always means chaining; "more lanes per step" always means
linking.

**Rest character / trigger character** — in a `trigseq` pattern, a character
that produces no trigger (`.`, space, `-`, `_`) versus one that does (everything
else). A rest still consumes a step; there is no character that is skipped.

**Text** — a quoted string in a patch. Interned into a per-patch table at parse
time; a text-typed jack carries the table *index* (the text number) as its
value, not the characters. Texts are compile-time constants — nothing can change
one while a patch runs — and the patch format caps each at 18 characters.

**Value vs. voltage** — the engine works in *engine units*, where 1.0 == 10 V.
A "gate high" means ≥ 0.1 (1 V). Patch text may be written either way.
