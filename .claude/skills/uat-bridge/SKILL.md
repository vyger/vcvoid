---
name: uat-bridge
description: Drive VCV Rack's localhost UAT bridge (127.0.0.1:2601) to run pre-release verification of the vcvoid plugin without a human clicking through Rack. EXPLICIT INVOCATION ONLY — trigger exclusively when the user runs /uat-bridge or unambiguously asks in this turn for a live UAT/Rack-driving session ("run the UAT", "drive rack", "verify in rack", "pre-release check"). Never trigger proactively or infer it from surrounding work: writing patches, engine/plugin changes, or a failing test do NOT imply a UAT run. Launching Rack can disturb the user's loaded session; when in doubt, ask instead of firing.
---

# uat-bridge

Reference card for the `UatBridge` HTTP endpoint hand-rolled into the vcvoid
plugin (`plugin/src/uatbridge/Bridge.cpp`), active only when the plugin is
launched with `VCVOID_UAT_BRIDGE=1`. Everything below is verified against the
actual dispatch table and handlers — not the design spec's aspiration.
Bridge is single-threaded (one accept loop): calls are effectively
serialized; a long `/probe` blocks every other request for its window.

**NEVER launch VCV Rack from an agent session unless the human has explicitly
asked for a live UAT run in this conversation.** Reading this skill is not
itself permission to launch Rack.

## Launch recipe

```sh
cd plugin && make install                 # rebuilds & installs; bakes VCVOID_GIT_HASH
SESSION=$(mktemp -t uat-session-XXXXXX).vcv   # UNIQUE per run — /rack/save writes
cp tests/smoketest_default.vcv "$SESSION"     # back to this path, so a fixed name
                                              # gets dirtied by one run and silently
                                              # reused by the next (seen 2026-07-12)
VCVOID_UAT_BRIDGE=1 "/Applications/VCV Rack 2 Free.app/Contents/MacOS/Rack" \
  "$SESSION" &
```

Within one run, relaunches (persistence checks) reuse the same `$SESSION`
path deliberately; a NEW run always mints a new name.

Rack takes the patch as a positional argument; the repo template
`tests/smoketest_default.vcv` is the canonical deterministic starting rack
for automated runs: a single BLANK master (no patch loaded, no other
modules) — it stays pristine that way. It guarantees the UI queue attaches
(no empty-rack 503) and gives `POST /rack/save` a real path; every run
builds the rest of its rack up from that one master via `POST /modules`.
Prefer attaching to an already-running bridge when one answers on `/ping`
(skip the launch entirely, and skip `/rack/quit` at the end — leave the
user's session up).

(`tools/uatbridge-smoke.sh` probes `/Applications/VCV Rack 2 {Pro,Free}.app`
then `/Applications/Rack.app` in that order if `$RACK` isn't set; `-u`
pins the autosave dir the script later parses.)

**Scripted full UAT**: `tools/uat_run.py` is the canonical executor for the
whole runbook (`docs/uat/runbook.md`) — it owns this entire lifecycle
(mktemp session copy, launch, `/ping` hash gate, readiness polls, graceful
quits and persistence relaunches, crash detection with evidence collection)
and drives every machine-checkable step from the driving maps in
`docs/uat/driving/*.json`, writing per-step results to `docs/uat/results/`.
Example: `python3 tools/uat_run.py` (all phases), `--phases 10` for a
subset, `--list` to print steps, `--allow-hash-mismatch` after docs-only
HEAD drift. Exit code = FAIL count. Prefer it over hand-driving the phases;
the endpoint reference below is for triage and ad-hoc checks.

1. **Poll `/ping` until it answers** (bridge HTTP thread comes up quickly,
   but don't assume instant — poll, don't sleep-once):
   `curl -sf http://127.0.0.1:2601/ping`
2. **Stale-build gate**: compare `gitHash` from the response against
   `git rev-parse --short HEAD`. Mismatch means the running Rack is an old
   build — stop and `make install` again, don't proceed on stale code.
3. **Master registration poll**: `/ping` answering does NOT mean a master
   module is ready to drive — Rack may still be restoring the autosave.
   Poll `GET /master/{id}/status` until it returns 200 (404 = not
   registered yet). Discover the master's Rack module id from the autosave
   JSON (`~/Library/Application Support/Rack2/autosave/patch.json`,
   `.modules[] | select(.plugin=="vcvoid" and .model=="master")`) or
   self-assemble more modules with `POST /modules` (see below). **Precondition:
   at least one vcvoid module must already be in the rack** — the bridge's
   UI-queue drain widget is attached only from a vcvoid module widget's
   `step()` (the HTTP thread cannot attach it), so a truly-empty rack (zero
   vcvoid modules) 503s every UI-thread route, including `POST /modules`
   itself. Launch from the runbook template (which places a master) or add one
   by hand before driving.
4. When done: `POST /rack/quit` for a graceful shutdown (saves
   settings/autosave); only kill the process as a failure-path fallback.
5. **Crash = stop, permanently.** If the Rack process dies without your
   `/rack/quit` (check `pgrep -f "MacOS/Rack"` when `/ping` starts refusing
   connections mid-session), the session is over and any UAT run in
   progress is an immediate FAIL. Do **not** relaunch and continue: the
   next launch after a crash shows a modal "Rack crashed" safe-mode dialog
   that the bridge cannot see or dismiss — automation would hang on
   `/ping` or, worse, drive a safe-moded Rack. Preserve evidence first
   (your Rack stderr/stdout capture, `~/Library/Application Support/Rack2/log.txt`,
   new `~/Library/Logs/DiagnosticReports/Rack-*.ips`), then hand recovery
   to the human.

## Endpoint reference (as implemented)

All bodies/responses are JSON. `{id}` is a Rack module id (int64). Routes not
listed here 404 with `{"error":"no such route"}`.

### Meta
| Route | Request | Response 200 | Other codes |
|---|---|---|---|
| `GET /ping` | — | `{bridgeVersion:1, gitHash:"<short-sha>"}` | — |

### Master — patch & status
| Route | Request | Response 200 | Other codes |
|---|---|---|---|
| `GET /master/{id}/status` | — | `{patchPath, statusLine, chain:[...names, "x7" prefixed if present], x7Present:bool, chainError, midiWarning:bool, timingMode:"adaptive"|"fixed", targetHz, effectiveRate, adaptiveHz}` (`midiWarning` true when the loaded patch uses MIDI but no MIDI hardware is reachable — same diagnostic as the panel context menu) | 404 unknown master |
| `POST /master/{id}/patch` | `{path}` (must be absolute) | same shape as `/status` | 400 invalid JSON / missing path / non-absolute path; 404 unknown master |
| `POST /master/{id}/reload` | — | same shape as `/status` | 400 `{"error":"no patch loaded"}`; 404 unknown master |
| `POST /master/{id}/reset-state` | — | same shape as `/status` (fresh-boot: all stateful circuits re-seed from startvalues) | 400 no patch loaded; 404 unknown master |
| `POST /master/{id}/tick-rate` | `{hz}` (one of `2000\|4000\|6000\|8000`, implies Fixed) **or** `{mode:"adaptive"}` (implies Adaptive at the master's current `adaptiveHz`) | same shape as `/status` | 400 invalid JSON, or neither a valid `hz` nor `mode:"adaptive"` (if both `mode` and `hz` are present, `mode` wins — `hz` is not even inspected); 503 ui-not-attached; 404 unknown master |
| `GET /master/{id}/cpu` | — | `{timingMode, targetHz, effectiveRate, adaptiveHz, tick:{valid, avgUs, maxUs, estCpuShare, windowSeconds}, rack:{meterEnabled, cpuShare?}, profiling:{enabled, circuits:[{index, circuit, totalUs, ticks, avgUs}, ...]}}` — see the CPU/profiling notes below | 404 unknown master |
| `POST /master/{id}/cpu/profiling` | `{enabled}` (bool) | same shape as `/cpu` | 400 missing/invalid `enabled`; 409 `{"error":"no patch loaded"}`; 404 unknown master |

Note: `statusLine` carries both the success message (`"ok, N bytes RAM"`) and
load errors (`"LOAD ERROR line N: ..."`) — there is no separate `ok`/
`errorLine`/`errorText`/`ramBytes` field despite the design doc's sketch;
parse `statusLine` with a regex/`test()`.

### Timing mode, adaptive rate, and CPU/profiling (issue #3)

- `timingMode` is `"adaptive"` (the default for a newly placed master) or
  `"fixed"`. In Adaptive mode `targetHz` tracks `adaptiveHz`, which the
  master recomputes from the loaded patch's RAM footprint every load
  (`cycleUs = 180 + 0.0029 × ramUsed; hz = clamp(1e6/cycleUs, 2000, 5555)`,
  `plugin/src/AdaptiveRate.hpp`). `effectiveRate` is the actual rate the
  engine runs at after the sample-rate/`targetHz` divider rounds (never
  exactly `targetHz` unless it divides the sample rate evenly).
- `POST /master/{id}/tick-rate {"hz":N}` always implies Fixed at that rate;
  `{"mode":"adaptive"}` always implies Adaptive — there is no way to set a
  fixed rate and stay in Adaptive mode, or vice versa, in one call.
- `GET /master/{id}/cpu`'s `tick.valid` is false for the first ~1 s epoch
  after a patch (re)load — the previous patch's numbers are cleared, not
  carried over, so an immediate read after loading reads `avgUs`/`maxUs` as
  stale/zero until a full epoch completes at the current `effectiveRate`.
  `tick.estCpuShare` is `avgUs × effectiveRate / 1e6`, an estimate derived
  from the same atomics, not Rack's own meter.
- `rack.cpuShare` is present only in clang builds (it uses a private Rack
  API that GCC poisons at compile time) and only once Rack's own CPU meter
  is turned on (Engine menu) and has collected at least one sample;
  otherwise only `rack.meterEnabled` is reported.
- `profiling.enabled`/`profiling.circuits` reflect per-circuit wall-time
  profiling (`Engine::setProfiling`), off by default and **reset to off on
  every patch (re)load** — including a hot-reload and Adaptive's own
  rate-driven engine rebuild, since each load constructs a fresh `Engine`.
  Re-enable via `POST /master/{id}/cpu/profiling {"enabled":true}` after any
  reload. `POST .../cpu/profiling` 409s with `{"error":"no patch loaded"}`
  if no engine exists yet.

### Master — state read-back
| Route | Request | Response 200 | Other codes |
|---|---|---|---|
| `GET /master/{id}/registers?ids=O1,O2,L1.1,_CABLE,F1` | — | `{"<name>": <float>, ...}` for every requested id | 400 missing `ids`; 400 `{"error":"no patch loaded"}`; 400 `{"error":"unknown register","name":"<offender>"}` for the first unresolvable plain register name (stops there — does not report the rest of the batch); 404 unknown master |
| `GET /master/{id}/midi-ports` | — | `[{port, direction, name, currentDevice, devices:[{driverId,deviceId,name}]}]`; `[]` on a MASTER16 with no X7 chained | 503 ui-not-attached; 404 unknown master |
| `POST /master/{id}/midi-port` | `{port, direction:"in"|"out", deviceName}` (substring match, first hit by driverId then deviceId order) | `{ok:true, driverId, deviceId, name}` | 400 invalid body / bad direction; 404 `{"error":"no such midi port on this master","port":...}` or `{"error":"no device name matched","candidates":[...]}`; 503 ui-not-attached; 404 unknown master |
| `GET /master/{id}/faders` | — | `[{global, motorTarget, notches, led, ledColor}, ...]` in chain order; `[]` with no patch or no M4 | 404 unknown master |
| `GET /master/{id}/leds` | — | `{matrix:[16 x {r,g,b}]}` present **only** on masters with an LED matrix (MASTER16; omitted entirely on MASTER18 — check for key presence, not an empty array), plus `buttons:{"L<ctrl>.<n>": 0..1}` always present | 404 unknown master |

Register-id kinds: plain registers (`O1`, `L1.1`, `R3`, ...) go through the
same `parseRegisterName` the engine uses and 400 if unrecognized. Cable
(`_NAME`) and fader (`F<n>`) handles have **no existence check** — an
unresolvable one silently reads back `0.0` rather than 400ing (see Caveats).

**Units — two different scales, don't mix them up.** `/master/{id}/registers`
returns values in DROID's normalized **−1…+1** engine domain (the project's
±10 V ↔ ±1 convention — `MasterBase.hpp` writes `outputs[i].setVoltage(
getRegister(...) * 10.f)` to go from register to Rack port). `GET /probe`,
by contrast, always samples the **actual Rack port voltage** (`Port::
getVoltage()`, so roughly ±10 V, whichever backend — vcvoid ring buffer or
foreign per-ms sampling) — the same number a cable-hover tooltip shows. When
cross-checking a register readback against a probe of the same signal,
multiply/divide by 10 to compare like for like: `register × 10 ≈ probe.avg`.
Gate/trigger-type registers (e.g. an `O` register a `[button]`/`[lfo]`
square feeds) read back as **0/1**, not 0/10 — only the Rack-port voltage
(probe or a physical jack) is on the 0–10 V hardware scale.

### Generic rack ops
| Route | Request | Response 200 | Other codes |
|---|---|---|---|
| `GET /modules` | — | `[{id, plugin, slug, x, y, width}]` (px) | 503 ui-not-attached |
| `POST /modules` | `{plugin, slug, x, y}` | `{id}` | 400 invalid body; 404 unknown plugin/model; 500 create failure; 503 ui-not-attached |
| `DELETE /modules/{id}` | — | `{ok:true}` | 404 no such module; 503 ui-not-attached |
| `POST /modules/{id}/move` | `{x, y}` | `{x, y}` (actual post-move px pos) | 400 invalid body; 404 no such module; 503 ui-not-attached |
| `GET /cables` | — | `[{id, outputModuleId, outputId, inputModuleId, inputId}]` | 503 ui-not-attached |
| `POST /cables` | `{outputModuleId, outputId, inputModuleId, inputId}` | `{id}` | 400 invalid body / portId out of range; 404 no such module; 503 ui-not-attached |
| `DELETE /cables/{id}` | — | `{ok:true}` | 404 no such cable; 503 ui-not-attached |
| `POST /params` | `{moduleId, paramId, value, holdMs}` — `holdMs` omitted/0 = plain set; `>0` = set now, auto-reset to `0` after `holdMs` ms | `{ok:true, holdMs?}` (holdMs echoed only if >0) | 400 missing/non-numeric moduleId/paramId, negative paramId, or paramId out of range for the module; 404 no such module; 503 ui-not-attached |
| `GET /probe?moduleId=&portId=&kind=out\|in&ms=500` | query only | `{min, max, avg, edges, periodStddevMs, sampleRateHz}` | 400 missing/invalid moduleId, portId, kind, or ms; 400 portId out of range; 404 no such module (or module removed mid-probe) |

`503 {"error":"ui bridge not attached; add any vcvoid module or launch from
the runbook template"}` is returned by every route that marshals onto the UI
thread (all "Generic rack ops" plus midi-ports/midi-port and the mutating
half of tick-rate) if no vcvoid widget has attached the drain hook yet — add
any vcvoid module (e.g. via a template patch) if you see this on a clean
launch.

### Rack lifecycle
| Route | Request | Response 200 | Other codes |
|---|---|---|---|
| `POST /rack/save` | — | `{ok:true, path}` | 400 no patch path (unsaved); 500 save exception; 503 ui-not-attached |
| `POST /rack/quit` | — | `{ok:true}` (async: closes the window on the next frame, process exit follows) | 503 ui-not-attached |
| `POST /rack/sample-rate` | `{hz}` | `{ok:true, hz}` | 400 missing/non-numeric hz, or hz not in {44100,48000,88200,96000,176400,192000}; 503 ui-not-attached |

`portId` for `/probe` and `/cables` is the Rack port INDEX (0-based), not a
DROID register: `O1` = outputs[0], `O4` = outputs[3], etc.

## Assertion vocabulary

- **Gate/clock check**: `probe` with `kind=out`, `ms` covering several
  periods; assert `edges` within **±1** of `expected_hz × (ms/1000)`. Edge =
  rising crossing of 1.0 V with 0.5 V re-arm hysteresis (Schmitt-style), so
  noisy/ramping signals near threshold won't multi-count.
- **Steady-clock check**: same probe, assert `periodStddevMs` is a small
  fraction of the expected period (needs `edges >= 3` to be meaningful —
  0/1 edges always report `periodStddevMs: 0`, which is "no data," not
  "steady"). FIXED (2026-07-12, issue #5): probe timestamps are no longer
  reconstructed from a wall-clock window. vcvoid-master probes are
  frame-accurate — each sample's timestamp is derived from the true
  audio-thread sample rate (`args.sampleRate`, captured live alongside the
  sample write, not looked up after the fact), so `sampleRateHz` reports the
  real engine rate and `periodStddevMs` is honest. Absolute asserts like
  `periodStddevMs < 2 ms` on a clean square are now legitimate for master
  probes. Foreign-module probes (any non-vcvoid module) get a real
  `steady_clock` timestamp per sample instead of an assumed-uniform 1 ms
  grid, so their `periodStddevMs` reflects actual HTTP-thread scheduling
  jitter at ~1 kHz resolution — expect looser tolerances there than on a
  master probe.
  `sampleRateHz` tells you the probe's time resolution for that call (vcvoid
  masters: the true audio engine rate via a ring buffer; any other module:
  actual samples-over-actual-span at ~1 kHz on the HTTP thread).
- **DC / level check**: probe over a short window (~50–200 ms is plenty for
  a non-moving signal); assert `min ≈ max ≈ avg` (within float noise) and
  compare `avg` to the expected voltage.
- **Toggle / button-state check**: read the register or `L<ctrl>.<n>` LED via
  `GET /master/{id}/registers` (or `/leds`) *before* acting, `POST /params`
  to tap, then read again and assert it flipped. Don't assume a starting
  state — some toggles persist across reloads.

## Driving knowledge

- `POST /params` with `holdMs≈300` simulates a tap; `holdMs>=1500` simulates
  a longpress (save/load-preset gestures etc.). UI clicks issued through
  other tools (e.g. an MCP server's `set_params`) do **not** reliably
  register as holds — always drive longpresses through this bridge's
  `holdMs`, never a scripted click-and-wait.
- DROID octave numbering is **3 octaves above MIDI/VCV**: DROID's A1 = 440 Hz.
  A VCV C4 VCO reads **1.000 V** on a `vcotuner` pitch input, not 4 V — don't
  apply the naive MIDI-octave formula when checking pitch CV.
- MASTER18's native `I1`/`I2` gate inputs threshold at **0.1 V**; G8 gate
  jacks threshold at **0.75 V** — these are different hardware comparators,
  don't reuse one constant for both when scripting a gate-in check.
- A patch load error **stops the engine** — the previously-running patch does
  NOT keep running (matches hardware, where the new `droid.ini` has already
  replaced the old one on disk by the time the Forge/master parses it).
  After a load error, `GET /master/{id}/registers` 400s with "no patch
  loaded" until a good patch is loaded.
- Fundamental 8vert rows normalize to the row above's input when chaining —
  watch for this when a patch's expected default value seems to silently
  inherit from an unrelated-looking neighbor row.
- `motorfader` `startvalue` is interpreted as an **output value** (the dent
  index for a notched fader), not a 0..1 physical position — don't clamp it
  to 0..1 when comparing against an expected startvalue.
- **M4 touch plate ≠ fader grab** (split 2026-07-11, UAT F8): the plate
  button (`TOUCH_PARAMS`, paramIds 4–7) is the step/B-button surface —
  pressing it fires motoquencer's step button (gate/skip/pattern toggle per
  the active buttonmode) and raises `B<ctrl>.<f>`. A mouse drag of the fader
  counts only as *holding* (edit acceptance, motor stop, pitch-bend), never
  as a plate press. To script a fader edit via the bridge you must hold the
  plate param while setting the fader param (there is no drag primitive), so
  **each edit toggles the step once** — pair every plate press with a second
  tap when the sequence's gates must stay unchanged, or budget an even
  number of presses per step. This is hardware parity, not a bridge quirk.
- G8 has **split top/bottom hit-boxes** per jack pair: the top half of a jack
  is the input, the bottom half is the output. Undiscoverable by eye; only
  matters for `portId`/`kind` selection when probing/cabling a G8 jack (the
  physical jack panel), not for the bridge API itself.
- Module ids survive a Rack save/reopen but do **not** survive delete +
  re-add (a fresh id is assigned) — use `POST /modules/{id}/move` instead of
  delete/re-add whenever a module's persisted state (patch path, presets,
  circuit state) needs to be preserved.
- Breaking the controller chain (removing/reordering an adjacent controller)
  pauses the engine sample-and-hold style: outputs hold their last value
  until the chain is restored (~1 s debounce), `status.chainError` reports
  `CHAIN ERROR` meanwhile.
- Controller numbering (`P1.x`, `B2.x`, ...) follows the **patch's declared
  controller order**, not physical chain position — reordering controllers
  physically without matching the patch's declaration order produces a
  `CHAIN ERROR` (declaration/chain type mismatch), not a renumbering.

- `POST /params`'s `paramId` is a Rack `Module::paramId` int (e.g. p2b8:
  2 pots then 8 buttons, so button `B1.1` is `paramId 2`), not a DROID
  register name — there is no bridge endpoint that resolves `P1.2`/`B1.1`
  style labels to `paramId`. Look up the target module's `ParamId` enum in
  its `plugin/src/<Module>.cpp`/`.hpp`, or infer it from a known layout
  (e.g. `tools/uatbridge-smoke.sh`'s own comments), before scripting a tap.

## Caveats

- `POST /rack/sample-rate` only writes the global `rack::settings::sampleRate`
  (the same lever Rack's Engine-menu picker turns). CONFIRMED INERT on a
  running engine (measured 2026-07-11: probe `sampleRateHz` stayed pinned
  through 48k/96k requests) — the 200 response means "setting accepted,"
  and nothing re-applies it live. Change the rate via the Engine menu (or
  restart Rack) when a test actually needs a different rate.
- `GET /probe` blocks the bridge's single-threaded accept loop for its whole
  window (`ms`, clamped to 10–5000) — no other bridge request can be served
  until it returns. Keep probe windows as short as the assertion allows when
  scripting a long sequence of calls.
- `GET /master/{id}/registers` returns `0.0` (HTTP 200), not a 400, for an
  unresolvable `_cable` or fader (`F<n>`) name — Engine exposes no existence
  check for those handles. Only plain register names (`O1`, `L1.1`, ...) 400
  on typos. Double-check cable/fader spelling by other means (e.g. cross-
  reading the source `.ini`) rather than trusting a non-zero-vs-zero
  distinction to catch a typo.
- `DELETE /modules/{id}` targeting a master mid-`/probe` is unreachable in
  practice (the accept loop is serial — the delete can't arrive until the
  probe request returns) — don't bother scripting that race, it can't
  happen through this bridge.
