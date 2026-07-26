# Experimental circuits (vcvoid only)

> **⚠️ Nothing on this page is DROID.**
> These circuits exist only in [vcvoid](../../../README.md). They are not part
> of any DROID firmware, the Droid Forge does not know them, and a patch using
> one **will not run on DROID hardware**. Everything else under `manual/` is a
> transcription of the real DROID manual; this directory is the one place that
> is not.

## Using them

1. In vcvoid, open the master module's context menu and enable
   **"Allow experimental circuits"** (under *Experimental*, next to *Ignore
   memory limits*). Until you do, loading a patch that uses one fails with an
   error naming the circuit.
2. The setting is saved with your Rack patch, per module.
3. To validate such a patch with `tools/droidcheck`, pass `--experimental`.
   Without the flag droidcheck reports experimental circuits as problems on
   purpose — a clean default run means "the real Forge would accept this".

## The circuits

| Circuit | Function |
|---------|----------|
| [`trigseq`](trigseq.md) | Declarative trigger sequencer — write the rhythm as text (`"x...x.x."`) |

## Adding one

See [`docs/adr/0001-experimental-circuits.md`](../../../docs/adr/0001-experimental-circuits.md)
for why this exists and how it is built. In short: declare the circuit in
`engine/experimental.json` (the Forge's own schema — one file feeds both the
engine's jack tables and droidcheck), implement it in `engine/circuits/`, write
its page here with `experimental: true` in the frontmatter, and mark its goldens
with the `experimental` directive.
