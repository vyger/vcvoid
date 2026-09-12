# 2. Circuit state belongs to the patch, not to the module

Date: 2026-09-12
Status: Accepted
Issue: [#42](https://github.com/vyger/vcvoid/issues/42)

## Context

DROID persists the result of manual interaction — button toggles, motorfader
positions, motoquencer sequences and their presets, calibration — so that a
patch comes back the way you left it. On hardware this lives in `DROIDSTA.BIN`
on the SD card, and it is reloaded into whatever patch is on that card by
matching circuits on **type + per-type ordinal**: the second `[button]`'s saved
state loads into the second `[button]` of the new patch. `manual/hardware.md`
§11.1 documents this plainly, including its failure mode — "if you change the
order of the circuits in your patch, circuits will get the *wrong* states".

vcvoid reproduced that rule faithfully (`Engine::saveState` /
`Engine::restoreState`), with the state blob living in the master module and
serialised into the Rack patch. What it could not reproduce is the hardware's
escape hatch: **one SD card per patch**. A Rack module has no card slot, so
every patch a master ever loads shares one state blob.

The hardware does have per-patch state, though — just not for `droid.ini`.
`manual/basics.md` §5.15 ("More than one patch on the memory card") lets a
card carry `droidXY.ini` patches selected by holding controller X's button Y
while pressing load, and each of those gets its own state file, `DSTAXY.BIN`:
"each patch has separate state", so an `algoquencer` in two such patches keeps
two separate pattern sets. Only the plain `droid.ini` shares the single
`DROIDSTA.BIN`. This ADR therefore extends the firmware's own multi-patch
model to every file a master loads, rather than inventing a new one, and the
file name is a key the firmware itself already uses.

The consequences are not subtle:

- Loading an unrelated patch into the same master injects the previous patch's
  state into every stateful circuit *by position*. A single stale value can kill
  a patch silently — a motorfader that gates a patch's internal clock comes back
  muted, so every transport LED and button responds while nothing advances; a
  motoquencer lane whose steps all arrive `skip`ped looks frozen next to
  neighbours that run.
- There is no way to keep state per patch. Alternating between two patches in
  one master means each load destroys the other's dialled-in work.
- Editing a patch (the hot-reload path, which is the normal authoring loop) is
  itself a structural change as soon as a circuit is added or removed, and the
  positional rule then smears state across circuits that have nothing to do with
  each other.

The honest framing is that fidelity here was being purchased with the user's
work. The hardware's rule is only defensible *because* the card is the unit of
identity, and we had dropped the card.

## Decision

Key circuit state on **what the patch structurally is**, keep every patch's
state, and be explicit about which of three things happened on each load.

- **Structural fingerprint.** `patchFingerprint()` hashes the ordered list of
  circuit types in the patch (FNV-1a, 16 hex characters). Comments, register
  labels, parameter values and controller declarations are excluded, so re-tuning
  a patch, relabelling its jacks or swapping a `p2b8` for a `p4b2` keeps the
  fingerprint — and therefore keeps the state — while inserting, deleting or
  reordering a circuit changes it.

- **A store, not a slot.** Each master keeps a `PatchStateStore`: one snapshot
  per fingerprint, carrying the patch's path, title, optional `# STATE:` tag and
  a save timestamp, serialised into the Rack patch under `circuitStateStore`.
  The store is **unbounded** — no LRU, no cap, nothing is ever evicted
  automatically. Dialled-in state is the user's work; silently dropping the
  oldest entry would lose a patch's state the moment someone cycled through
  enough others. Entries leave only when the user asks.

- **Three outcomes, named.** On load: an exact fingerprint hit **restores**;
  otherwise a snapshot sharing the patch's normalised file path, or its explicit
  `# STATE: <id>` header tag, is **migrated** into the new structure; otherwise
  the patch starts **fresh**. Unrelated patches never share state. The master's
  context menu (and the UAT bridge's `/master/status`) carries one line saying
  which happened.

- **Migration beats the hardware rule.** Each saved circuit carries a
  *signature*: the set of internal-cable and register names its **outputs** are
  bound to (falling back to what its inputs read when it binds no outputs).
  Migration pairs instances of a type by exact signature first, then pairs the
  leftovers positionally among themselves — the hardware rule applied to the
  residue — and leaves anything still unpaired at its defaults. Inserting a
  `[motoquencer]` ahead of an existing one therefore no longer steals its
  sequence: the cables it drives identify it.

- **The hardware rule is still the hardware rule.** `restoreState()` is
  untouched: for a patch that matches its own snapshot exactly, behaviour is
  byte-for-byte what it was, including the documented reorder hazard *within* a
  fingerprint. Migration is a separate entry point used only when the structure
  has actually changed.

Explicitly **not** decided: hooking Rack's Initialize / `onReset` to wipe state.
An Initialize that silently discarded a sequencer's contents would be the same
class of mistake this ADR is fixing.

## Consequences

**Good.**

- Switching patches in one master is non-destructive in both directions: each
  patch keeps its own state, and coming back finds it.
- The authoring loop survives structural edits. Adding a circuit to a patch
  under construction no longer scrambles the state of the circuits already
  dialled in.
- The failure that motivated this — a patch that loads, lights up, and does
  nothing because a foreign snapshot muted its clock — cannot happen between
  unrelated patches at all, and when it can (same file, edited) the status line
  says `state: migrated`, which is a place to start looking.
- `# STATE: <id>` gives a deliberate way to carry one instrument's state across
  renames, copies and forks of a patch file.

**Costs and risks.**

- The Rack patch grows with the number of distinct patch *structures* a master
  has run — including, during authoring, one per structural edit of the same
  file, since each new circuit list is a new fingerprint. A snapshot is a flat
  list of doubles per stateful circuit: ~200 bytes of JSON for a one-circuit
  patch, ~4 KB for `uat-overlays.ini`, ~72 KB for the 202-circuit MFPS patch
  (`uat-mfps.ini`, 18 024 values). Integral values are serialised as JSON
  integers rather than 17-digit reals, which alone is most of a 5× saving on
  that last figure. This is the deliberate price of not evicting; the
  alternative is losing state. If it ever becomes a real problem the answer is a
  user-visible "forget this patch's state" affordance, not an automatic cap.
- The fingerprint is a hash of circuit *types* only, so two genuinely different
  patches with an identical circuit list (same types, same order, different
  wiring) share a fingerprint and will restore each other's state. That is the
  same class of collision the hardware has — and it is exactly the case where
  positional restore is most likely to be harmless — but it is a real limit.
- Migration is heuristic. A circuit whose outputs were all rewired in the same
  edit falls back to the positional rule, which can still mis-assign. It cannot
  be worse than the previous behaviour, since that rule is the fallback.

**Compatibility.**

- Rack patches saved before this change carry a single `circuitState` blob. It
  is read as the state of whatever patch `patchPath` names and filed under that
  patch's fingerprint on the first load — nothing is lost, and one reopen
  converts the save to the new shape.
- `dataToJson` keeps writing `circuitState` alongside the store, holding the
  currently loaded patch's snapshot, so a *downgrade* to an older vcvoid build
  finds exactly what it expects.
- Saved snapshots have no per-circuit signature. An absent signature simply
  means "no migration identity", which falls back to the positional rule — the
  pre-#42 behaviour.
- `dontsave`, `[droid] clearall` and the UAT bridge's `reset-state` endpoint are
  unchanged in meaning. `reset-state` now also drops the current patch's stored
  snapshot and blocks migration for that one reload, so it still means
  "fresh-boot this patch" — and it leaves every other patch's snapshot alone,
  exactly as pulling one SD card leaves the others alone.
