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

The first experimental circuit, [`trigseq`](../../manual/circuits/experimental/trigseq.md),
also forced a related finding worth recording: the Forge charges 6 bytes per
*text atom occurrence* (a pointer plus a length) and excludes texts from
constant counting, while our accounting had stubbed texts at zero. That is fixed
in the same change, so an experimental circuit's documented footprint is a
number the engine actually computes.
