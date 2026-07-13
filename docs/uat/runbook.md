# Pre-release UAT runbook (bridge-driven, auto e2e)

Repeatable pre-release verification, driven **end to end through the
`UatBridge` HTTP API** — see the **`uat-bridge`** skill
(`.claude/skills/uat-bridge/SKILL.md`) for the full endpoint reference, unit
conventions, and assertion vocabulary this doc assumes throughout. This file is
the thing to re-run before every release.

## How this run works

The entire body below (Phases 1–10) is a **single automated end-to-end
sequence**: every step is a `GET`/`POST` bridge call with an inline expected
value, so an agent or a script can drive the whole thing without a human
clicking through Rack. The harness even handles the process-level actions the
bridge has no endpoint for — quitting Rack (`POST /rack/quit`) and relaunching
the binary for the persistence phases — by controlling the Rack process
directly and re-polling `/ping` on the new process.

**The only human action is a single pass/fail at the very end** (see
**Final sign-off** below). The automated run produces a per-step PASS/FAIL
record; the reviewer reads that summary, does the short consolidated
sensory/manual spot-check collected there (the handful of qualities no bridge
endpoint can observe — panel art, MIDI-hardware feel, motor-fader resistance,
LED color/flash, OLED legibility, a DAW's receive side), and signs off
pass or fail on the release as a whole. There are **no human steps interleaved
in the phases** — if a step is in a phase body, it is machine-verified.

Each step keeps its original phase/number so results can cite `10.5`-style
locators.

## Preconditions

- Build + install: `cd plugin && make install` (bakes `VCVOID_GIT_HASH` into
  the binary; the bridge's `/ping` echoes it so a stale build can't silently
  pass).
- Launch from the **rack template** `tests/smoketest_default.vcv` — a
  single BLANK master, nothing else, and it stays pristine that way; every
  run builds its rack up from that one master via `POST /modules`. Rack
  takes the patch as a positional argument. Always launch a scratch COPY —
  `POST /rack/save` writes back to the loaded patch's own path and would
  mutate the repo template:
  1. `cp tests/smoketest_default.vcv /tmp/uat-session.vcv`
     `VCVOID_UAT_BRIDGE=1 "/Applications/VCV Rack 2 Free.app/Contents/MacOS/Rack" /tmp/uat-session.vcv &`
     (or just run `tools/uatbridge-smoke.sh` — launch mode does exactly this
     and doubles as the fast regression gate before the phases below).
  2. Poll `GET /ping` until it answers; compare `.gitHash` to
     `git rev-parse --short HEAD` — mismatch means rebuild/reinstall, don't
     proceed (`SKIP_HASH`-style overrides are for docs-only HEAD drift only).
  3. Poll `GET /modules` until 200 (503 = UI queue not attached yet; the
     template's master attaches it within the first frames), take the
     master's `{id}` from the response, then poll
     `GET /master/{id}/status` until 200 (master registered). Additional
     modules per phase are self-assembled via `POST /modules` — see
     `tools/uatbridge-smoke.sh` for the working recipe.
     **The template exists because a truly-empty rack cannot bootstrap:**
     the drain hook attaches from a vcvoid module widget's `step()` (the
     HTTP thread cannot attach it), so zero vcvoid modules ⇒ every
     UI-thread route 503s, `POST /modules` included.
- Audio device active in Rack (the engine must tick).
- Template patches (all in `patches/`, all pre-validated against
  `droidcheck` and `make patchsmoke`):
  `uat-core.ini`, `uat-overlays.ini`, `uat-gates.ini`,
  `test-m6-master18.ini`, `test-m5-extclock.ini`, `uat-mfps.ini`,
  `uat-err-register.ini`, `uat-err-cable.ini`, `uat-err-inputasoutput.ini`,
  `uat-midi-nox7.ini`, `test-m4-e4.ini`, `test-m4-m4.ini`,
  `test-m4-db8e.ini`, `test-m6-midifileplayer.ini` (+ `patches/midi1.mid`
  beside it), `test-m5-midiout.ini`, `test-m5-loopback.ini`.
- Final-sign-off sensory checks only (not needed for the automated run): a
  VCV Scope module available; for the MIDI-hardware items, IAC Driver
  "Bus 1" enabled and a MIDI monitor/DAW (Ableton).
- At the end of a session: `POST /rack/quit` for a graceful shutdown
  (flushes autosave/settings); only kill the process as a failure fallback.

### paramId lookup tables (no label-resolver endpoint exists)

`POST /params` takes a raw Rack `Module::paramId` int, not a `P1.2`/`B1.1`
label — SKILL.md documents this gap explicitly. Derived from each module's
`ParamId` enum (`plugin/src/<Module>.cpp`):

| Module | paramId | Control |
|---|---|---|
| `p2b8` | 0–1 | pots `P<c>.1`–`P<c>.2` |
| `p2b8` | 2–9 | buttons `B<c>.1`–`B<c>.8` |
| `b32` | 0–31 | buttons `B<c>.1`–`B<c>.32` |
| `m4` | 0–3 | faders `P<c>.1`–`P<c>.4` (set directly — moving the value) |
| `m4` | 4–7 | touch plates for faders 1–4 (set `1` to simulate a grab/touch, matching hardware's touch-plate-as-sensor; the widget also sets this while dragging, but the bridge can only drive the plate param, not a drag gesture) |
| `db8e` | 0–7 | 8 face buttons `B<c>.1`–`B<c>.8` |

Modules with **no scriptable params** (bridge cannot drive these controls —
confirmed against their `enum ParamId { PARAMS_LEN }`, i.e. empty):
- `e4` — all 4 endless encoders (turn is a monotonic internal detent counter
  the mouse/hardware widget increments, not a param; push is likewise
  internal state, not a param).
- `db8e`'s encoder (`E<c>.1`, push `B<c>.9`) — same reason; only the 8 face
  buttons above are scriptable.
- `g8`, `x7` — no controls at all (jacks/MIDI ports only; use `/cables` and
  `/probe` for I/O, `/master/{id}/midi-port` for MIDI routing).

Any control that turns/pushes an encoder therefore has no bridge path; those
are folded into the Final sign-off's manual checklist rather than blocking the
automated run.

---

## Phase 1 — Build, install, smoke

1. ☐ Follow Preconditions: build+install, launch, `/ping` gitHash gate,
   master-registration poll. Equivalent to running
   `tools/uatbridge-smoke.sh` up through its ping/discovery section.
2. ☐ First record the template master's id from `GET /modules` — that
   module is the run's lifeline and must NEVER be deleted: the bridge's UI
   drain detaches with zero vcvoid modules in the rack, after which every
   UI-thread route (including `POST /modules` itself) 503s until a
   relaunch. Then instantiate each vcvoid slug in turn via
   `POST /modules {"plugin":"vcvoid","slug":"<x>", "x":..,"y":..}`
   for each of `master, master18, p2b8, p4b2, p10, s10, p8s8, b32, e4, m4,
   g8, db8e, x7, bling` → expect 200 + `{id}` each time, then
   `DELETE /modules/{id}` for **exactly the ids returned by those POSTs**
   (never the template master's) → expect `{ok:true}`, no non-200 in the
   sequence. Afterwards `GET /modules` → the template master (original id)
   is still present, alone.

## Phase 2 — Chain assembly edge cases

Rack row: `master | p2b8 | b32` (adjacent, left to right).

1. ☐ Assemble via `POST /modules` (master at x=0, p2b8 at
   x=master_width_px, b32 at x=master_width_px+p2b8_width_px — see
   `tools/uatbridge-smoke.sh`'s adjacency comment for the px convention).
   `GET /master/{id}/status` → `.chain == ["p2b8","b32"]` (before any patch
   load, the chain line reads `master only` — expected with no patch loaded).
2. ☐ `POST /modules` to insert `bling` between p2b8 and b32 (move
   b32 right via `POST /modules/{id}/move` first if needed, then place
   bling, then move b32 back adjacent). `GET /master/{id}/status` →
   `.chain == ["p2b8","b32"]` still (bling transparent pass-through).
3. ☐ `POST /modules/{id}/move` the master to the right of the
   controllers. `GET /master/{id}/status` → `.chain == []`. Move back via
   the same endpoint (id is stable across a move, per SKILL.md).
4. ☐ Load `uat-overlays.ini` first (`POST /master/{id}/patch`).
   `DELETE /modules/{b32id}`, re-`POST /modules` a new b32 adjacent, then
   swap p2b8/b32 declared order by moving: `GET /master/{id}/status` while
   the chain is broken → `.chainError` truthy/non-empty and outputs hold
   (assert via two `GET /master/{id}/registers?ids=O1` a beat apart ==
   equal while broken); after re-add, poll `/status` until `.chainError`
   clears (~1 s debounce per SKILL.md's driving-knowledge notes). The
   declared-order swap → `CHAIN ERROR` (declaration/chain type mismatch,
   not a renumbering) — assert `.chainError` set until order is restored.

## Phase 3 — Core registers & math (`uat-core.ini`)

Rack row: `master | p2b8`. `POST /master/{id}/patch` with the absolute path
to `uat-core.ini` first.

1. ☐ Sweep P1.1: `POST /params {"moduleId":p2b8,"paramId":0,"value":0.0}`
   then `1.0`. `GET /master/{id}/registers?ids=O1` before/after → value
   moves consistent with a 1→10 Hz sine sweep (A×B+C input math); or probe
   O1 (`kind=out`, portId=0) at each end and compare `edges` over a fixed
   window.
2. ☐ `GET /probe?moduleId={id}&portId=3&kind=out&ms=1300` (O4) with
   I1 unpatched → `edges` in `4±1` for a 2 Hz retrigger over 1300 ms (N1
   normalization feeding I1). Exact recipe in `tools/uatbridge-smoke.sh`.
3. ☐ `POST /cables` wiring a Rack LFO (~8 Hz square) into master
   I1 (portId per `GET /modules`/`GET /cables` port indices). Probe O4 →
   edges jump to ~8 Hz×window. `DELETE /cables/{id}` → probe O4 again →
   back to ~2 Hz (N1 normalization). First live normalization test.
4. ☐ Euclid rate cross-check: `GET /probe?moduleId={id}&portId=1&kind=out&ms=2000`
   (O2) → edges consistent with 5-in-8 euclid at the R1-driven rate. (The
   master matrix LED's 3 Hz *flash*/color for input 1 has no bridge endpoint
   — `GET /master/{id}/leds` reports instantaneous RGB, not a flash rate;
   deferred to the Final sign-off sensory check.)
5. ☐ `POST /cables` a 5 V DC source into I2. `GET /probe?moduleId={id}&portId=2&kind=out&ms=100`
   (O3) → `min≈max≈avg≈10` (15 V clamped to 10). No tooltip hover needed —
   probe reads the same Rack-port voltage a tooltip would.
6. ☐ `POST /params {"moduleId":p2b8,"paramId":2,"value":1,"holdMs":300}`
   (tap B1.1). `GET /master/{id}/registers?ids=L1.1,O5` before/after →
   `L1.1` flips 0→1 (LED lit) and O5 gate register flips (toggle via
   `[button]`). Second tap flips back — exact recipe (incl. read-before-
   acting for persisted toggle state) in `tools/uatbridge-smoke.sh`.
7. ☐ `POST /cables` ±5 V into I3/I4 (the wiring the LED-color check needs).
   Assert the downstream register response numerically; the LED matrix
   *color* mirroring (red=positive/blue=negative, brightness by
   `ledbrightness`) has no bridge endpoint beyond the raw RGB triples in
   `GET /master/{id}/leds` — the color judgment is a Final sign-off item.

## Phase 4 — Overlays & presets (`uat-overlays.ini`)

Rack row: `master | p2b8 | b32`. Load via `POST /master/{id}/patch`.

1. ☐ `POST /params` selecting each of B1.1..B1.4 in turn
   (paramId 2–5, tap `holdMs:300`), sweeping P1.1 (paramId 0) after each.
   `GET /master/{id}/registers?ids=O1,L1.1,L1.2,L1.3,L1.4` → only the
   selected stage's LED lit (radio behavior) and O1 tracks only the
   selected envelope stage; re-select an earlier button → its pot value is
   unchanged (held).
2. ☐ Tap B2.1, B2.4, B2.7 (algoquencer steps; b32 paramIds
   0, 3, 6). `GET /master/{id}/registers?ids=O2,O3` and/or
   `GET /master/{id}/leds` `.buttons` for the corresponding `L2.n` →
   step LEDs latch; probe O2 for trigger edges on those steps; O3 register
   steps its pitch CV.
3. ☐ `POST /params {"moduleId":p2b8,"paramId":1,"value":0.9}`
   (P1.2 ~preset 7 — p2b8 has 2 pots, so P1.2 is paramId 1). Longpress
   save: `POST /params {"moduleId":p2b8,"paramId":<B1.6 id=7>,"value":1,"holdMs":1800}`
   (≥1.5 s per SKILL.md's longpress convention — **must** go through this
   bridge's `holdMs`, not a scripted click, per the driving-knowledge note
   that other UI-click paths don't reliably register holds). Change the
   pattern (tap a different step button), then longpress-load
   (`paramId=<B1.5 id=6>`, `holdMs:1800`). `GET /master/{id}/registers`
   for the pattern-bearing registers → pattern snaps back to the saved one.

## Phase 5 — Persistence & timing

Continue from Phase 4 state.

1. ☐ `POST /rack/save`, then the harness quits and relaunches Rack —
   `POST /rack/quit`, wait for the process to exit, relaunch the binary on
   the same scratch patch (no bridge endpoint restarts the Rack process, so
   the harness drives it), re-poll `/ping` + `/master/{id}/status` on the
   new process, then `GET /master/{id}/registers` for pattern/preset/
   pot-overlay/button-toggle state → all intact (patch path auto-reloads
   from Rack's own save).
2. ☐ Edit `uat-overlays.ini` on disk (e.g. `hz` change) while
   Rack runs; poll `GET /master/{id}/status` for `.statusLine` to flip
   within ~1 s (hot-reload) — or force it via `POST
   /master/{id}/reload`; then `GET /master/{id}/registers` → envelope rate
   follows, sequencer pattern/presets survive.
3. ☐ `POST /master/{id}/tick-rate {"hz":2000}` → 2000 → 4000 →
   8000 → 6000, asserting 200 + no registers reads glitching
   (`GET /master/{id}/registers?ids=O1` stays numeric throughout, no 500s).
   Each response's `.timingMode == "fixed"` and `.targetHz` matches the
   requested `hz`. Then `POST /master/{id}/tick-rate {"mode":"adaptive"}`
   (the default for a freshly placed master, exercised here explicitly
   after leaving Fixed) → `.timingMode == "adaptive"` and `.targetHz ==
   .adaptiveHz`. Sample-rate contract: `POST /rack/sample-rate`
   48k→96k→44.1k each returns 200 (setting accepted). Whether the *running*
   audio device actually re-opens live is unverified by the bridge (SKILL.md
   Caveats) — the no-crash/no-stuck-output judgment while switching in the
   Engine menu is a Final sign-off item.
4. ☐ CPU/profiling (`GET`/`POST /master/{id}/cpu[/profiling]`, SKILL.md
   for full field reference). Recipe: with `uat-core.ini` loaded, `POST
   /master/{id}/cpu/profiling {"enabled":true}` → 200, `.profiling.enabled
   == true`; wait ~1.5 s (one whole-tick stat epoch); `GET
   /master/{id}/cpu` → `.tick.valid == true`, `.tick.avgUs`/`.tick.maxUs`
   numeric and > 0, `.profiling.circuits` non-empty with each entry's
   `avgUs ≈ totalUs / ticks`; sort by `avgUs` descending to sanity-check the
   costliest circuit looks plausible (not asserted, just eyeballed for the
   sign-off notes). `POST /master/{id}/reload` → `GET /master/{id}/cpu` →
   `.profiling.enabled == false` again (profiling resets on every patch
   reload, fresh `Engine` default) and `.tick.valid == false` immediately
   after (stale epoch cleared). Re-enable and repeat once to confirm the
   toggle round-trips. Separately: `POST /master/{id}/cpu/profiling` before
   any patch has ever loaded on a scratch master → 409 `no patch loaded`.

## Phase 6 — Gates & MASTER18 (`uat-gates.ini`, `test-m6-master18.ini`)

Rack row: `master | g8 | x7`.

1. ☐ `GET /probe?moduleId={g8id}&portId=0&kind=out&ms=1200` →
   edges `≈1.2±1` (1 Hz gate), `min≈0,max≈5` (0/5 V, not the 0/10 V
   register scale). Jack 2 (portId=1) at 0.5 Hz similarly.
2. ☐ `POST /cables` a Rack square LFO into G8 jack 5 **input**
   half (`portId=4`, `kind=in` semantics — the input is a separate Rack
   input port per SKILL.md's split-hitbox note, not a kind flag on one
   port); `POST /cables` a 0.5 V DC into jack 6 input (portId=5).
   `GET /probe?moduleId={masterid}&portId=0&kind=out` (O1) → mirrors jack
   5 as 0/10 V. `GET /master/{id}/registers?ids=O2` (jack 6 downstream) at
   0.5 V → low (<0.75 V G8 threshold → reads 0); raise the DC source to
   1 V → O2 jumps (threshold crossed, using the G8's 0.75 V comparator per
   SKILL.md, not MASTER18's 0.1 V).
3. ☐ `GET /probe?moduleId={x7id}&portId=0&kind=out&ms=1000` → X7
   gate 1 (G9) pulses 2 Hz, edges `2±1`. (First G8 matrix LED's 3 Hz flash
   has no bridge endpoint — Final sign-off sensory check.)
4. ☐ Swap to `master18` + `POST /master/{id}/patch`
   `test-m6-master18.ini`. `POST /cables` a Rack CV source at
   0.05 V then 0.2 V into I1/I2; `GET /master/{id}/registers?ids=O6` →
   0.05 V reads O6=0, 0.2 V reads O6 nonzero/10 (MASTER18's 0.1 V
   threshold, per SKILL.md — different comparator from G8's 0.75 V, don't
   reuse the constant). `GET /master/{id}/midi-ports` → 6 ports present.
   (The VCO tuner's ±1 cent readout has no bridge endpoint — Final sign-off;
   remember DROID's octave numbering is 3 above MIDI/VCV, so a VCV C4 VCO
   reads 1.000 V on a `vcotuner` pitch input, not 4 V.)
5. ☐ `POST /master/{id}/patch` with `uat-core.ini`'s absolute
   path on the MASTER18. Expect 200 with `.statusLine` matching
   `^LOAD ERROR` (N1 rejected — no normalization on MASTER18); then
   `GET /master/{id}/registers?ids=O1` → 400 "no patch loaded" (the engine
   stops on a rejected load, matches hardware per SKILL.md's driving
   knowledge — not "previous patch keeps running").

## Phase 7 — Error paths & recovery

`master | p2b8` row with `uat-core.ini` running.

1. ☐ `POST /master/{id}/patch` → `uat-err-register.ini` →
   `.statusLine` matches `LOAD ERROR line 7:.*O9`; `GET
   /master/{id}/registers?ids=O1` → 400 (no patch loaded — the engine stops
   on a rejected load rather than keeping the previous patch running).
2. ☐ `POST /master/{id}/patch` → `uat-err-cable.ini` and
   `uat-err-inputasoutput.ini` in turn → both `.statusLine` matches
   `^LOAD ERROR`; the cable error is known to report "line 0", so assert
   message content, not a specific line number for that one.
3. ☐ Recovery: fix the error in a scratch copy of the errored
   `.ini` on disk (e.g. `O9`→`O1`) while it's the loaded path; poll
   `GET /master/{id}/status` for `.statusLine` to flip to `^ok` within
   ~1 s (hot-reload), or force via `POST /master/{id}/reload`.
4. ☐ `POST /master/{id}/patch` → `uat-midi-nox7.ini` →
   `GET /master/{id}/status` → `.midiWarning == true` (the patch uses MIDI
   but no MIDI hardware is reachable; this is the same diagnostic the panel
   context menu shows). `POST /modules` to chain an X7 → re-`GET
   /master/{id}/status` → `.midiWarning == false`. (The X7's physical USB
   MIDI-out activity LED / external monitor confirmation is a Final sign-off
   item — no bridge endpoint observes it.)

## Phase 8 — Controllers (M4 / E4 / DB8E)

Rack row: `master | e4 | m4 | db8e`. The scriptable surface here is the M4
faders/touch-plates and the DB8E face buttons; the encoder and OLED/feel
qualities have no bridge params (see the paramId table's "no scriptable
params" list) and are Final sign-off items.

1. ☐ `test-m4-m4.ini`: script a "grab and move":
   `POST /params {"moduleId":m4,"paramId":4,"value":1}` (touch plate 1) then
   `POST /params {"moduleId":m4,"paramId":0,"value":0.5}` (fader 1 value),
   then release the plate (`paramId:4,"value":0}`); `GET /master/{id}/faders`
   → `.motorTarget`/`.global` reflect the commanded value, no continuous
   glide/denormal creep in successive reads. (The *feel* of motor resistance
   under a physical grab is a Final sign-off item.)
2. ☐ `test-m4-db8e.ini`: `POST /params` tapping each of the 8
   face buttons (paramId 0–7) → `GET /master/{id}/leds` `.buttons` shows
   only that button's `L<c>.n` lit. (OLED header "SWEEP" + 0..1 value
   legibility at 6 HP, and the E3.1 encoder drag/ring, are Final sign-off
   items — no bridge endpoint renders/reads OLED pixels or drives encoders.)
3. ☐ `test-m6-midifileplayer.ini` (with `patches/midi1.mid`
   beside it): `POST /master/{id}/patch`, then `GET /probe` on O1/O2
   over the file's known duration → edges/voltage pattern consistent with
   the file's notes (exercises the SD-card file provider without needing
   to listen).

## Phase 9 — MIDI routing (M5)

Chain `master | x7`. The device-selection persistence is machine-verifiable;
the DAW receive/send side is not (no bridge endpoint reads a DAW's buffer or
event log) and is a Final sign-off item.

1. ☐ Device selections survive save/reopen: `POST /master/{id}/midi-port`
   to select a device, `POST /rack/save`, harness quit+relaunch (as in
   Phase 5 step 1), re-poll readiness, `GET /master/{id}/midi-ports` →
   `.currentDevice` matches what was set.
2. ☐ `test-m5-extclock.ini`: while an external 24 PPQN MIDI clock drives the
   chain (set up as a Final sign-off precondition — DAW send is not
   scriptable), `GET /probe?moduleId={id}&portId=0&kind=out&ms=2000`
   (O1 16ths) → `periodStddevMs < 2` (steady-clock check, needs
   `edges >= 3`). (Note delivery into Ableton for `test-m5-midiout.ini` /
   `test-m5-loopback.ini`, and the "no events lost" log check under load,
   are Final sign-off items.)

## Phase 10 — Capstone: Motor Fader Performance Sequencer (`uat-mfps.ini`)

Rack row: `master | p2b8 | b32 | m4 | m4`. `uat-mfps.ini` is the canonical
FULL Forge-generated MFPS, abbreviated with `tools/inicompress.py` (see
CLAUDE.md) — same bytes as would ship to hardware.

1. ☐ Assemble the five-module row via `POST /modules`; `POST
   /master/{id}/patch` the absolute path to `uat-mfps.ini` →
   `.statusLine` matches `^ok, [0-9]+ bytes RAM`; `.chainError` empty.
2. ☐ Press B2.12 (START — b32 paramId 11): `POST /params
   {"moduleId":b32,"paramId":11,"value":1,"holdMs":300}`. `GET
   /probe?moduleId={id}&portId=1&kind=out&ms=1000` (gate on O2) → edges
   present; `GET /master/{id}/registers?ids=O1` (pitch) changes across
   steps; `GET /master/{id}/faders` → M4 fader `.led`/`.global` step.
3. ☐ Script a melody: `POST /params` touch+move on m4 faders 1–8
   (two m4 modules, paramId 0–3 each, with touch-plate paramId 4–7 set
   first per the Phase 8 recipe) to a rising scale; tap B1.1 ROOT / B1.3
   5TH (p2b8 paramIds 2,4) to enable scale notes. `GET
   /master/{id}/registers` for the pitch outputs → quantized to scale
   degrees; `GET /master/{id}/faders` across a page change → faders
   re-snap to per-step values.
4. ☐ Track menu (B2.15 — b32 paramId 14): tap, select track 2.
   `GET /master/{id}/faders` → re-snap to track 2's values (motor recall
   under patch control).
5. ☐ Preset save/load round-trip. Baseline: `GET /master/{id}/faders`
   (record melody state). Save to preset A: `POST /params` longpress B2.1
   (`paramId:0`, `holdMs:1800`, per the ≥1.5 s longpress convention).
   Mangle the melody (change several fader values via touch+move as in
   step 3). Re-read `GET /master/{id}/faders` to confirm the mangle landed
   (rules out a false read). Load preset A (longpress the MFPS's load
   control — confirm its actual load paramId from the patch's generated
   button map before scripting). `GET /master/{id}/faders` → matches the
   saved baseline. If it does not, `POST /master/{id}/reset-state`
   (fresh-boot re-seed from startvalues) and repeat save→mangle→load once
   from a known-clean state to isolate save-vs-load — the assertion is that
   a clean-state round-trip restores the saved faders exactly.
6. ☐ LUCK (B2.10) randomize: `POST /params` tap; RATC (B2.17)
   ratchets: `POST /params` tap. `GET /probe` on the gate outputs (O2 /
   O4 / O6 / O8, portIds 1/3/5/7) before/after each → edges count/pattern
   changes, confirming an audible/visible effect without needing to
   listen.
7. ☐ `POST /rack/save`, harness quit+relaunch (as in Phase 5 step 1),
   re-poll readiness then `GET /master/{id}/faders`/`registers` →
   sequence, presets, fader values, selected track all intact; press START
   (paramId 11) again → sequencer resumes, probe a gate output to confirm.

---

## Final sign-off

The automated run above records a PASS/FAIL per step. This is the single
human touchpoint for the release.

1. Review the automated PASS/FAIL summary. Any FAIL blocks release.
2. Run the consolidated sensory/manual spot-check — the qualities no bridge
   endpoint can verify, grouped by the step they relate to:
   - **1.x** Rack `log.txt`: vcvoid loads 14 models, no errors/warnings.
     Panel appearance (already regression-checked by `tools/panelshots.sh` /
     `tools/check_panelshots.py`).
   - **3.4 / 3.7** Master matrix LED flash (~3 Hz) and color mirroring
     (red=positive / blue=negative, brightness by `ledbrightness`).
   - **5.3** No crash / stuck outputs while switching sample rate live in the
     Engine menu.
   - **5.4** Rack ctx-menu Duplicate of the master: clone loads the same
     patch/state, no crash; delete it (no bridge endpoint duplicates a
     module).
   - **6.3** First G8 matrix LED flashes ~3 Hz.
   - **6.4** VCO tuner reads a patched Rack VCO within ±1 cent.
   - **7.4** X7 USB MIDI-out activity LED green / ~2 notes per second on an
     external monitor.
   - **8.1** M4 motor-fader physical resistance: grabbing wins over the
     motor; release → motor retakes the commanded position.
   - **8.2** DB8E OLED header "SWEEP" + smooth 0..1 value legible at 6 HP;
     E3.1 encoder drag/ring. E4 encoders: turn moves output + value-ring
     dot, cap tick-mark rotates, push registers without full-white ring,
     per-encoder LEDs independent (`test-m4-e4.ini`).
   - **9.x** `test-m5-midiout.ini` notes arrive in Ableton;
     `test-m5-loopback.ini` round-trip intact, no dropped events at 2 notes/s
     under load; `test-m5-extclock.ini` log stays clean of "events lost"
     through a live tempo change and the 64 Hz stress case.
3. **Pass/fail the release.** If every automated step passed and the sensory
   spot-check shows no regressions, the build is good to ship. Otherwise,
   record what failed and hold the release.
