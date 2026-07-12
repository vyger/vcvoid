# Contributing to vcvoid

Thanks for your interest! vcvoid is a VCV Rack 2 plugin that emulates the
DROID hardware and runs real `droid.ini` patches. This guide covers development
setup, the test suites, and the circuit-implementation workflow.

Please read [`architecture.md`](architecture.md) (what is being built and why)
and [`docs/implementation-system.md`](docs/implementation-system.md) (how
circuits get built) before making substantial changes.

## AI-generated code

Fair warning: the vast majority of this code has been generated through `claude-code`.
Some of it may make sense, some of it may be horrible. This project has placed most of
its effort towards automated and spec-driven verification via AI tools and very little
effort towards human-readable code. That said, PRs that improve the overall state of the
code are very welcome as long as the golden tests and the e2e UAT pass.

## Bug reports with repro steps are gold

You don't have to write code to make a valuable contribution. One of the most
useful things anyone can send is a **GitHub issue with detailed reproduction
steps**, ideally including:

- the **DROID patch** (`.ini`) that triggers the problem — trimmed down as far
  as you can while still reproducing it;
- the **VCV Rack patch** (`.vcv`) if the module chain / cabling matters
  (which modules, in what chain order, what's plugged into what);
- what you expected (a manual section or page reference for the circuit's
  documented behavior is perfect — see `manual/circuits/<name>.md`), what
  actually happened, and any exact values (register read vs. expected
  voltage);
- your setup: macOS version, VCV Rack version, and the vcvoid commit/build.

A faithful-emulation project lives and dies on exactly this kind of report:
a minimal patch that behaves differently from the hardware (or from the
manual) usually converts directly into a golden test and a fix.

## Platform support

**vcvoid has only been tested on macOS so far.** Other platforms (Linux,
Windows) are **not supported yet** — the engine is portable C++17 and will
probably build, but nothing has been verified there, and the UAT tooling
(`tools/uatbridge-smoke.sh`, the runbook launch recipes) assumes a macOS Rack
install. PRs that port, test, or fix things on other platforms are always
welcome.

## Development setup

The **engine and its tests need no VCV Rack install** — that is by design, so
most work can be done and verified headlessly.

| Need it for | Requirement | Install |
|-------------|-------------|---------|
| Everything | C++17 toolchain (clang/gcc), `make`, `git`, Python 3 | system package manager |
| Building the Rack plugin | [VCV Rack SDK](https://vcvrack.com/manual/Building) | download the SDK; set `RACK_DIR` |
| `make crosscheck` / `make layoutcheck` | Qt 6 (for `tools/droidcheck`) | `brew install qt` (or distro Qt6) |
| `make artcheck` | `opencv-python` | `pip3 install opencv-python` |

`tools/droidcheck` clones and builds the DROID Forge on first run
(`tools/droidcheck/build.sh`, ~30 s); its `build/` and `vendor/` are git-ignored.
Checks that need it **skip gracefully** if the vendor checkout or the Python/Qt
deps are missing, so a minimal setup still runs the core tests.

## Test suites

Run from the repo root:

```sh
make test         # unit tests + all golden circuit tests (the primary gate)
                  #   also runs layoutcheck + artcheck when their deps are present
make crosscheck   # validate golden patches against the Forge (parity oracle)
make layoutcheck  # parity-check plugin/src/Layout.hpp against the Forge's layout data
make artcheck     # validate control positions against the faceplate art (opencv)
make gen          # regenerate engine/gen/ + scales from droidfirmware.json
```

Panel-render regression (needs a Rack install):

```sh
tools/panelshots.sh              # render per-module screenshots through Rack
tools/check_panelshots.py        # diff them against tests/panel-baseline/
```

**`make test` is the gate.** Its exit code is the number of failures, so CI and
agents can verify without launching Rack. Never commit with `make test` red.

### End-to-end UAT (bridge-driven)

Beyond the headless suites, the plugin has an end-to-end UAT process that
drives a real Rack instance through the plugin's localhost HTTP bridge
(`UatBridge`, `127.0.0.1:2601`, enabled by launching Rack with
`VCVOID_UAT_BRIDGE=1`). **Any change must leave all test suites green,
including the e2e UAT** — at minimum the automated smoke, and the full runbook
for changes that touch the plugin/adapter layer, panels, or the bridge itself.

- **Fast regression gate:** `tools/uatbridge-smoke.sh` — exercises every
  bridge endpoint against a known patch. It attaches to an already-running
  Rack if one answers on `/ping`, or launches Rack itself from the
  `tests/smoketest_default.vcv` template and quits it when done.
  Prerequisites (a fresh `plugin` install, `jq`) are documented in the
  script's header comment.
- **Full autonomous run:** [`docs/uat/runbook.md`](docs/uat/runbook.md) — the
  repeatable pre-release verification. Steps tagged `[AUTO]` are scriptable
  end to end through the bridge (including by an agent); `[HUMAN]` steps are
  the small feel/visual remainder (target ≤ 15 min of human time).
- **Bridge API reference:** the `uat-bridge` skill
  ([`.claude/skills/uat-bridge/SKILL.md`](.claude/skills/uat-bridge/SKILL.md))
  documents every endpoint, the launch recipe, unit conventions, and the
  assertion vocabulary the runbook assumes. In a `claude-code` session you can
  simply ask to "run the UAT" and the skill drives the whole thing.

## Implementing a circuit (goldens-first)

Circuit work follows a strict, repeatable pipeline. The
[`implement-circuit`](.claude/skills/implement-circuit/SKILL.md) skill automates
it; the full spec is in
[`docs/implementation-system.md`](docs/implementation-system.md). In short:

1. **Select** — pick the circuit (or the lowest-`rank` unblocked `todo` from
   [`circuits-status.yaml`](circuits-status.yaml)) and mark it `in-progress`.
2. **Read the spec** — `manual/circuits/<name>.md` in full (prose, worked
   `droid` examples, Inputs/Outputs tables), plus referenced figures. Confirm
   the generated jack table `engine/gen/<name>` exists (`make gen` if not).
3. **Author golden tests first** — `tests/golden/<name>/*.gold` (format:
   `tests/runner/goldfile.hpp`), derived from the manual: every worked example
   becomes a test, plus I/O edge cases (defaults, clamping, short aliases),
   trigger/gate semantics, and patch-order sensitivity where relevant. Pin the
   tick rate and RNG seed per test. For `spec_gap: true` circuits, test what is
   specified and add a `# SPEC-GAP:` comment stating exactly what hardware
   reference data would close the gap.
4. **Implement** — `engine/circuits/<name>.{hpp,cpp}`, subclassing `Circuit`;
   semantics from the manual, interface from the generated table. Register it
   in the circuit factory.
5. **Verify** — the new goldens pass, the **full** suite passes (no
   regressions), and `droidcheck` accepts every patch used.
6. **Record & commit** — set the ledger entry to `done` with coverage notes;
   one atomic commit (implementation + goldens + ledger).

If you hit an ambiguous spec, missing infrastructure, or a test you can't make
pass, leave the ledger entry `blocked` with the reason in `notes` and leave the
tree clean — **never commit red, never partially commit.**

## Code style

- **Dense rationale comments are the house style.** Explain *why* — cite the
  manual section, the worked example, or the hardware behavior a line
  reproduces. The ledger `notes` and `# SPEC-GAP:` annotations carry the same
  spirit: record the reasoning and any corner-case decisions, not just the code.
- The engine stays **pure**: no Rack or Qt dependencies in `engine/`. All Rack,
  panel, and MIDI concerns live in the `plugin/` adapter layer.
- **Never hand-edit** `engine/gen/` — it is code-generated by `make gen`.
- Match the surrounding formatting; the Makefile builds with `-Wall -Wextra`,
  so keep it warning-clean.

## Submitting changes

1. Branch off `main`.
2. Make your change with tests. For a circuit, that means goldens-first.
3. Ensure **all test suites pass**: `make test` (and `make crosscheck` if you
   touched patches or the parser), plus the e2e UAT — `tools/uatbridge-smoke.sh`
   at minimum, and the [`docs/uat/runbook.md`](docs/uat/runbook.md) `[AUTO]`
   phases for plugin/panel/bridge changes. Include the relevant ledger update
   if you implemented a circuit.
4. Open a pull request describing the change and the verification you ran.
   Keep commits atomic; for circuits use the message form `circuit: implement
   <name>`.

By contributing you agree that your contributions are licensed under
**GPL-3.0-or-later**, matching the project license.
