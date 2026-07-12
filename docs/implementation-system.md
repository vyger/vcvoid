# The implementation system

How circuits get built: an autonomous, repeatable pipeline so that "implement
the `lfo` circuit" is a single command, and continuous circuit-by-circuit
implementation can run unattended via `/loop`.

Companion to [architecture.md](../architecture.md) (which defines *what* is
being built).

## Overview

Three parts:

1. **The ledger** — `circuits-status.yaml` at the repo root: one entry per
   circuit, tracking status and ordering. The single source of truth for
   "what's done, what's next".
2. **The skill** — `.claude/skills/implement-circuit/`: the per-circuit
   pipeline an agent executes (spec → golden tests → implement → verify →
   record → commit).
3. **Loop mode** — `/loop` invokes the skill with `next`, which picks the
   top-ranked `todo` circuit from the ledger. One circuit per iteration.

## The ledger (`circuits-status.yaml`)

Generated initially from the manual frontmatter
(`manual/circuits/*.md`: `impl_difficulty`, `controller_binding`,
`verification`, `spec_gap`), then maintained by the pipeline.

```yaml
# One entry per circuit. Status transitions: todo → in-progress → done
#                                            todo → blocked (with reason)
#                                            (not-feasible is terminal)
circuits:
  copy:
    status: done
    difficulty: easy          # from manual frontmatter
    binding: master-only      # master-only | x7-required | controller-required | controller-enhanced
    verification: headless    # headless | rack-automated | requires-human
    spec_gap: false
    rank: 10                  # implementation order (lower = sooner)
    depends_on: []            # circuits/infrastructure that must exist first
    notes: ""                 # coverage caveats, SPEC-GAP details, blockers
  lfo:
    status: todo
    difficulty: easy
    binding: master-only
    verification: headless
    spec_gap: false
    rank: 20
    depends_on: []
    notes: "randomize output needs seeded-RNG harness support"
```

**Ordering policy** (encoded in `rank` when the ledger is generated):

1. `master-only` + `easy` + `headless` first — they exercise the engine core
   with minimal surface.
2. Then `moderate`, then `hard`.
3. Dependency-aware: e.g. `select` before overlay-dependent circuits; MIDI
   circuits after the X7 module exists; controller-UI circuits after the
   expander framework (M4 milestone).
4. `controller_binding` gates: `x7-required` and `controller-required`
   circuits stay `blocked` until their milestone's infrastructure lands.
5. `not-feasible` circuits (per frontmatter) are recorded and never picked.

## The pipeline (`implement-circuit <name|next>`)

Executed by the skill. Each step must pass before the next; failure leaves
the ledger entry `blocked` with a reason — **never commit red**.

1. **Select** — explicit circuit name, or `next`: the lowest-rank `todo`
   whose `depends_on` are all `done`. Mark `in-progress`.
2. **Read the spec** — `manual/circuits/<name>.md` in full (prose, worked
   `droid` examples, Inputs/Outputs tables, `verification_note`), plus
   linked docs and referenced figures (the per-page renders under
   `manual/images/`, git-ignored) when waveforms/timing diagrams matter. Check `engine/gen/<name>` jack tables
   exist (regenerate with `tools/jackgen` if not).
3. **Author golden tests first** — `tests/golden/<name>/*.gold` (format: see
   tests/runner/goldfile.hpp), derived
   from the manual: every worked example in the circuit doc becomes a test;
   plus I/O-table edge cases (defaults, clamping, short aliases), trigger/
   gate edge semantics, and — where relevant — patch-order sensitivity.
   Pin tick rate and RNG seed per test. For `spec_gap: true` circuits, test
   what is specified and add a `# SPEC-GAP:` comment stating exactly what
   hardware reference data would close the gap.
4. **Implement** — `engine/circuits/<name>.{hpp,cpp}`, subclassing
   `Circuit`, semantics from the manual, interface from the generated
   tables. Register in the circuit factory.
5. **Verify** — build the headless runner; the new goldens pass; the **full**
   golden suite passes (no regressions); `droidcheck` accepts all test
   patches used. This is deterministic and requires no Rack and no human.
6. **Record & commit** — ledger entry → `done` (with coverage notes), one
   atomic commit to `main`: implementation + goldens + ledger update,
   message `circuit: implement <name>`.

### Failure handling

- Ambiguous spec discovered mid-implementation → implement the best-effort
  reading, note the ambiguity in the ledger `notes` and a `# SPEC-GAP:`
  test comment; don't silently guess without recording.
- Missing infrastructure (needs an engine feature that doesn't exist) →
  ledger `blocked` with the dependency named in `depends_on`; no partial
  commit. The gap itself becomes work for the maintainer or a dedicated
  infrastructure task.
- Test failures that resist fixing → `blocked` with findings in `notes`;
  leave the tree clean (stash/revert).

## Loop mode

To enable incremental autonomy, run exactly this in a Claude Code session at
the repo root (single line):

```
/loop use the implement-circuit skill to implement the next circuit from circuits-status.yaml; stop looping when no todo circuit is unblocked
```

Notes on the command:

- `/loop` with no interval argument (as above) is **self-paced**: each
  iteration starts as soon as the previous one finishes — right for this,
  since circuit implementation time varies wildly. (An interval form like
  `/loop 10m …` exists but adds nothing here.)
- The prompt names the skill and the ledger explicitly, so a fresh iteration
  (or a fresh session after an interruption) needs no other context.
- The stop condition is part of the prompt — the loop ends itself when the
  ledger has no unblocked `todo` entries, leaving `blocked` items as the
  human-attention list.

Properties:

- Each iteration = one circuit through the full pipeline (self-paced —
  an easy circuit is minutes, a hard one much longer).
- The ledger makes iterations stateless: any session can pick up `next`.
- Terminates cleanly when nothing is `todo`-and-unblocked; remaining
  `blocked` entries are the human-attention list.
- Because every iteration ends with the full regression suite green and one
  atomic commit, an interrupted loop never leaves the repo broken.

## Verification philosophy

- **Headless-first**: the golden runner is the gate; Rack is never in the
  inner loop. Circuits marked `verification: rack-automated` or
  `requires-human` in the manual frontmatter still get maximal headless
  coverage, with the residual noted in the ledger for later Rack-driven or
  human passes.
- **Forge parity**: `droidcheck` runs in CI over `tests/golden/**` patches
  and `patches/*.ini` — our loader and the Forge must agree on what loads.
- **Determinism**: fixed tick rate per test, seeded RNG, no wall-clock
  dependence. A golden suite run is bit-reproducible.

## Bootstrap order (before the loop can run)

1. **M0/M1 infrastructure** (see architecture.md milestones): scaffold,
   `jackgen`, parser, registers, input math, kernel loop, RAM accounting,
   golden runner — proven end-to-end with `copy`. — ✅ done
2. Generate `circuits-status.yaml` from the manual frontmatter. — ✅ done
3. Write the `implement-circuit` skill. — ✅ done
4. Run the pipeline manually on 2–3 easy circuits (`lfo`, `math`,
   `flipflop`) to shake out the skill. — ✅ done (lessons from those runs were
   folded back into the `implement-circuit` skill)
5. Turn on `/loop`.
