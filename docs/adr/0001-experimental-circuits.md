# 1. vcvoid-only experimental circuits, gated at load

Date: 2026-07-25
Status: Accepted
Issue: [#12](https://github.com/vyger/vcvoid/issues/12)

## Context

vcvoid emulates DROID hardware. The project's central promise is fidelity: every
circuit it runs exists on a real master, and a patch that loads in vcvoid loads
in the Droid Forge. The `manual/` tree is a transcription of the DROID manual and
is treated as the spec; `tools/droidcheck` exists purely to answer "would the
real Forge accept this patch?"; the engine's jack tables are *generated* from the
Forge's own `droidfirmware.json` so they cannot drift.

That fidelity also means vcvoid cannot do anything the hardware does not — even
where the emulator has freedoms the firmware lacks (no flash budget, no
8-bit-era parser, no firmware release to review). We wanted a declarative
trigger sequencer that no DROID circuit provides. Adding one naively would have
broken three things quietly:

1. A patch using it would load here and fail on hardware, with nothing marking
   which circuit was the culprit.
2. droidcheck would report it as an unknown circuit, so such patches could not be
   validated at all — losing duplicate-output, register and memory checking along
   with the parity signal.
3. A non-hardware circuit documented beside the real ones would corrupt the one
   reference the project treats as spec.

## Decision

Introduce a distinct class of circuit — the **experimental circuit** — that
exists only in vcvoid, and gate it at patch load.

- **Declaration.** Experimental circuits live in `engine/experimental.json`,
  written in the Forge firmware file's own schema. `tools/jackgen` merges it with
  the vendored firmware to emit one circuit table, tagging overlay entries
  `experimental = true`; `tools/droidcheck/build.sh` merges the same file into
  the validator's embedded firmware and generates the name list it gates on. One
  file, two consumers, no possible drift. The generator's assertion that the
  firmware half contains exactly 76 circuits is retained.
- **Gate.** `LoadOptions.allowExperimental`, off by default. Patch compilation
  refuses an experimental circuit with a line-localised error naming both the
  circuit and the menu item that enables it. The check sits in `compilePatch`,
  not in the engine's implementation-coverage pass, so headless tooling behaves
  identically to the plugin.
- **Opt-in.** A persisted per-module boolean, surfaced as "Allow experimental
  circuits" in the *Experimental* section of a master's context menu, next to
  "Ignore memory limits". Toggling reloads the patch immediately.
- **Validation.** droidcheck reports experimental circuits as problems by
  default — a clean run must keep meaning Forge parity — and validates them
  normally under `--experimental`.
- **Documentation.** `manual/circuits/experimental/`, `experimental: true` in
  frontmatter, a standing banner on each page, and a fenced-off section in the
  circuits index. The ledger tracks them but does not *rank* them, so the
  firmware implementation backlog is unaffected.

## Consequences

**Good.**

- The default experience is unchanged: vcvoid remains exactly as faithful as
  before, and a patch built without touching the toggle still runs on hardware.
- Failure is loud and actionable — a load error naming the circuit and the
  switch, rather than silence now and a mystery on the master later.
- Experimental patches still get most of droidcheck's structural validation.
- Adding the next experimental circuit is an overlay entry plus an
  implementation file. No generator, engine or validator changes.

**Costs.**

- The generated jack table is no longer a pure function of the Forge's firmware
  file. This is the surprising part, and the reason this ADR exists.
- A "vcvoid patch" and a "DROID patch" are now potentially different things.
  Anyone sharing a patch that uses an experimental circuit must say so.
- Two places describe circuits (firmware JSON, overlay), so the overlay must
  keep tracking the Forge's schema if the Forge changes it.

**Deliberately not done.**

- No attempt to make hardware or the Forge run these circuits.
- No second mechanism for "vcvoid extensions" to existing firmware circuits —
  extra jacks on a real circuit would be a *different* and much more dangerous
  decision, because such a patch would look hardware-valid.

## Notes

**Numbered jacks in the overlay must start at 1.** The Forge's own model ignores
`start_at`: `DroidFirmware::findJack` validates an array jack by generating
`prefix1 … prefix<count>`, so a block declared `start_at: 8, count: 24` is
accepted by the engine as `hirescc8 … hirescc31` and by droidcheck as
`hirescc1 … hirescc24`. That is exactly the drift this ADR exists to prevent,
and it is silent — `crosscheck.sh` only sees the jack names the goldens happen
to use. The overlay may therefore use `{prefix, count}` freely but must leave
`start_at` at 1, and any circuit numbering its jacks by an external quantity has
to make that quantity start at 1 too. (This is a Forge limitation, not one of
ours: the firmware's own `start_at: 0` jack, `calibrator`'s `tune0`, is
mis-validated the same way.) Discovered while adding
[`midihirescc`](../../manual/circuits/experimental/midihirescc.md), whose jack
number *is* a MIDI controller number.

The first experimental circuit, [`trigseq`](../../manual/circuits/experimental/trigseq.md),
also forced a related finding worth recording: the Forge charges 6 bytes per
*text atom occurrence* (a pointer plus a length) and excludes texts from
constant counting, while our accounting had stubbed texts at zero. That is fixed
in the same change, so an experimental circuit's documented footprint is a
number the engine actually computes.

**An overlay entry may `extends` a firmware circuit** (added for
[`motoquencer2`](../../manual/circuits/experimental/motoquencer2.md), issue #85).
Some experimental circuits are "firmware circuit X plus one jack", and X can be
large: `motoquencer` has 112 jacks, each with its own description. Transcribing
them into the overlay would put a hundred-jack copy of the Forge's own data in
this repo, silently free to drift the day the Forge edits one of those jacks —
precisely the failure mode this ADR exists to prevent, and one this ADR's
"Costs" section already names. So `tools/jackgen/overlay.py` resolves
`"extends": "motoquencer"` plus `extra_inputs` / `extra_outputs` into a full
circuit definition, deep-copied from the vendored firmware file with the extra
jacks appended and any other key (title, description, `ramsize`) overridden.
Both consumers call the same `overlay.load()`, so they still see one identical
definition, and a jack name or short form that collides with the base's is a
hard error rather than a silent shadow.

This is **not** the "vcvoid extensions to existing firmware circuits" mechanism
that the Decision above deliberately rules out. `[motoquencer]` is untouched and
still generated from the firmware alone; `extends` only says where a *separate,
experimental* circuit's jack list comes from, and the result is gated, refused
and documented exactly like a hand-written entry. The engine side mirrors the
same idea: `engine/src/motoquencer.hpp` holds the M4 editing surface so
`motoquencer2` inherits the implementation verbatim instead of forking it, and
`SeqCore` asks its subclass for the new setting (returning the hardware
behaviour by default) rather than reading a jack the hardware circuits lack.
