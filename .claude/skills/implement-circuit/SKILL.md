---
name: implement-circuit
description: Implement one DROID circuit in the vcvoid engine — goldens-first from the manual, full-suite verification, ledger update, atomic commit. EXPLICIT INVOCATION ONLY — this is an internal building block of the implementation system (docs/implementation-system.md), not a user-facing skill. Trigger only when the user runs /implement-circuit themselves or an implementation-system workflow (e.g. a /loop over it) invokes it. Never trigger it from conversational phrasing about circuits, bugs, or engine work. Argument: a circuit name, or "next" to pick from circuits-status.yaml.
---

# implement-circuit

Execute the pipeline from docs/implementation-system.md for ONE circuit.
Announce which circuit was selected before starting. Never commit red.

**Explicit invocation only** (see the frontmatter rule): this skill mutates
the ledger and makes commits, so never infer it from conversation.

## 1. Select

- Before touching the ledger, run `git status --porcelain` and record its
  output. Clean means no uncommitted changes to *tracked* files —
  pre-existing untracked files (e.g. `droid-manual-blue-7.pdf`, `build/`,
  `vendor/`) are expected and must be ignored, never STOPped on or swept up
  by cleanup. If tracked files are dirty, STOP and report. The failure
  path's cleanup only ever touches files this iteration created (see
  Failure handling) — never `git clean` paths that were already untracked
  at the recorded baseline.
- Explicit name: use it. Verify it exists in circuits-status.yaml and is not
  `done` / `not-feasible`. If `blocked`, say why and stop unless the blocker
  is resolved.
- `next`: pick the lowest-`rank` entry with `status: todo` whose `depends_on`
  are all `done`. If none exists, report "no unblocked todo circuits" and STOP
  (this is the /loop termination signal).
- Set the entry to `status: in-progress` (do not commit this — it's working
  state; the final commit sets the end state).

## 2. Read the spec

- Read `manual/circuits/<name>.md` IN FULL: prose, every worked `droid`
  example, the Inputs/Outputs tables, difficulty/verification notes.
- Read any referenced figures (the per-page renders under `manual/images/`,
  git-ignored) when timing or waveform shape matters — the image IS the spec
  for shapes.
- Follow `see_also` links when semantics depend on another circuit or on
  basics.md concepts (taptempo, presets, overlays).
- Check `engine/gen/` jack tables exist for the circuit (they do for all 76;
  if a Forge update changed them, run `make gen`). Existing ≠ correct: if any
  documented jack name has a digit as the last character of its full or short
  form (e.g. `log2`), or the firmware JSON lists array jacks comma-separated
  rather than space-separated, verify `findJack()` actually resolves it (quick
  unit test) before trusting the table — codegen has silently mis-parsed both
  patterns before, and the failure mode looks like a spec ambiguity, not a
  jack-table bug.

## 3. Author goldens FIRST

- `tests/golden/<name>/*.gold` (format: tests/runner/goldfile.hpp).
- Minimum coverage:
  - every worked example from the manual doc with externally observable
    behavior → one golden each (name them after the example's purpose);
  - defaults (unconnected inputs), from the Inputs table;
  - each input's documented range/clamping behavior;
  - trigger/gate edge semantics where the circuit has trigger inputs;
  - for time-based circuits: exact tick math at a stated tickrate — compute
    expected values by hand IN A COMMENT in the golden;
  - seeded-RNG determinism for random behavior (fixed seed → fixed sequence,
    plus a distribution sanity check if feasible).
- `spec_gap: true` circuits: golden what IS specified; add `# SPEC-GAP:`
  comments stating exactly what hardware reference data would close each gap.
- Multiple `patch <<< >>>` blocks in one `.gold` file merge into a single
  multi-circuit patch (`goldfile.cpp` appends) — use this deliberately to
  test variants side-by-side (e.g. several instances routed to different
  output registers), not by accident.
- Outputs clamp to ±10 V (engine units ±1.0): pick golden values that stay
  in range, or the clamp — not the behavior under test — is what you'll see
  mismatch. If a case genuinely needs an unbounded/unclamped observation
  value, route it through an internal cable or an `O` register — never a
  `N<n>` register: `N<n>` feeds the *default* for an unpatched `I<n>`, so
  writing an observation value there silently corrupts whatever else in the
  same patch reads `I<n>` unpatched (this produces a bafflingly-wrong number
  from a *different* golden block, not a load error — hard to diagnose).
- Internal cables (`_NAME`) MUST have at least one reader — a bare `_CABLE`
  used only as an `expect` probe with no other input consuming it fails to
  load ("cable is never used as an input"). Give every probe cable a real
  downstream reader, or observe via an `O` register instead.
- The goldfile runner enforces "sets before expects" per tick **globally
  across the whole file, keyed only by tick number — not per patch block**.
  When a `.gold` file merges multiple `patch <<< >>>` blocks, order ALL
  events for a given tick together across every block, sets first, even if
  the blocks are otherwise independent; two internally-correct blocks can
  still fail together if a later block's `@N set` follows an earlier block's
  `@N expect` in file order.
- Per-circuit `expect_ram` goldens are optional, not required minimum
  coverage; RAM accounting is exercised generically elsewhere.
- A circuit's ledger `spec_gap: false` describes the circuit overall, not
  every sub-behavior — some formulas can still have under-specified corner
  cases (e.g. a zero-divisor sign, a boundary case) even when the ledger
  says spec-gap-free. Don't skip the manual's worked examples on the
  assumption "false" means "everything is pinned"; if a sub-case truly can't
  be derived from the text, treat it like any other ambiguity (literal
  reading + `# SPEC-GAP:` comment) even though the ledger entry stays
  `spec_gap: false`.
- Run `make test` — the new goldens MUST FAIL with "not yet implemented"
  (proves they're actually exercising the circuit). This gate applies to the
  circuit's own goldens; shared-infrastructure unit tests added in step 4
  may be written alongside the fix instead of failing first.

## 4. Implement

- `engine/circuits/<name>.cpp` — subclass `droid::Circuit`, register with
  `DROID_REGISTER_CIRCUIT(<name>, <Class>)`. Follow `copy.cpp` as the
  template. Read 2–3 existing circuit implementations first and match style.
- State lives in member variables (constructed fresh per load). Randomness
  ONLY via the engine RNG (`EngineState::rngState`) — never `rand()`/chrono.
- Missing OR broken shared infrastructure (trigger edge detection, taptempo
  helper, text support, persistence hooks, jackgen/codegen helpers like
  `base_name()`/`findJack()`)? STOP and evaluate — this gate covers a shared
  piece that's *absent* just as much as one that's *present but subtly
  wrong* (e.g. a codegen bug):
  - small + reusable → implement/fix in `engine/src/` with unit tests, same
    commit;
  - large → set ledger `status: blocked`, `notes:` naming the missing piece,
    revert working tree, report. Do NOT hack circuit-local approximations or
    special-case around a shared bug in the circuit file.
- Before finishing, `grep -rn <name> tests/` — an infra test may use this
  circuit's name as an "unimplemented" placeholder; repoint it at a
  still-`todo` circuit instead of leaving it to break silently. Verify the
  replacement patch text uses *that* circuit's own real jack names (check
  its jack table) — copy-pasting the old placeholder's example patch verbatim
  can reference jacks the new circuit doesn't have.

## 5. Verify

- `make test` — new goldens pass AND the full suite stays green.
- `make crosscheck` if droidcheck is built (skip with a note if not).
- A crosscheck FAILURE (droidcheck reports problems on a golden patch we load
  cleanly) is a blocker: check whether it's one of the two recorded
  divergences in architecture.md (deprecated circuits; unary-negated
  registers) — if so, note it and proceed; otherwise it's a real Forge-parity
  bug → follow the "cannot make goldens pass" failure path (ledger blocked,
  clean tree, no commit).
- Re-read the manual doc top to bottom once more against your implementation —
  list each documented behavior and where it's covered (golden or N/A with
  reason). Add missed goldens now.

## 6. Record & commit

- Ledger: `status: done`, `notes:` = coverage caveats ("randomize
  distribution unverified vs hardware", etc.) or "". If any output
  convention (phase, direction, curve, etc.) isn't pinned by the manual,
  note it as "conventions unverified vs hardware".
- ONE commit: implementation + goldens + ledger:
  `circuit: implement <name>`
  Body: coverage summary + any SPEC-GAP notes.

## Failure handling

- Ambiguous spec → implement the most literal reading of the manual, record
  the ambiguity as `# SPEC-GAP:` + ledger note. Never silently guess.
- Cannot make goldens pass → ledger `status: blocked` with findings in
  `notes`, working tree cleaned back to the step-1 baseline (`git checkout
  -- <tracked files this iteration touched>` plus `rm`/`git clean` only for
  paths this iteration created — never sweep pre-existing untracked files),
  no commit.
- NEVER leave `in-progress` in the ledger on exit; never leave the tree
  dirty relative to the step-1 baseline.
