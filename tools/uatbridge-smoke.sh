#!/bin/bash
# UAT bridge smoke: exercises every bridge endpoint against a known patch.
# Run it as `make smoke` from the repo root (the canonical invocation; every
# env override below still applies, e.g. `make smoke SKIP_HASH=1`).
# Requires: plugin make install'd (build carries VCVOID_GIT_HASH), jq.
#
# SCOPE: contract-level checks only -- status codes, response shapes and
# classifications that must hold within a second or two of the call that causes
# them. Anything timing- or trajectory-dependent belongs in tools/uat_run.py
# (the runbook's executor), not here: this script is the fast gate, and it is
# fail-fast on purpose.
#
# Two modes, picked automatically:
#   attach  - a bridge already answers on :2601 (Rack running): use it, and
#             LEAVE Rack running at the end. This is the normal interactive
#             mode and the most reliable: no launch/quit lifecycle at all.
#   launch  - nothing on :2601: launch Rack env-gated with the template
#             patch tests/smoketest_default.vcv (a master-only rack; the
#             p2b8 is self-assembled), quit it at the end (POST /rack/quit;
#             trap kill as failure fallback). The template makes launch mode
#             deterministic: no autosave dependency, no empty-rack 503, and
#             POST /rack/save gets a real patch path.
#
# Preconditions:
#   * At least one vcvoid module must exist in the rack. The bridge's UI
#     queue attaches from a vcvoid module widget's step() -- a truly empty
#     rack cannot service POST /modules (503) and the script fails fast.
#     A vcvoid master is required (MASTER_ID override or autosave discovery);
#     the p2b8 is self-assembled via POST /modules when missing.
#   * Master jacks I1/I2 must have NO VCV cables patched in: O3==0 relies on
#     I2 reading 0, and the O4 probe relies on I1 falling back to its N1
#     normalization (2 Hz envelope retrigger).
#
# End state: master left running uat-core.ini, B1.1 toggle OFF (on success).
#
# Env overrides:
#   RACK       Rack executable (launch mode; default: probe /Applications)
#   TEMPLATE   .vcv patch to launch Rack with (launch mode; default:
#              tests/smoketest_default.vcv; TEMPLATE= to use the autosave)
#   MASTER_ID  Rack module id of the master (skips autosave discovery)
#   P2B8_ID    Rack module id of the p2b8 (skips discovery/self-assembly)
#   SKIP_HASH  =1 to warn instead of fail on gitHash mismatch. Rarely needed:
#              a mismatch whose two commits have IDENTICAL plugin/ and engine/
#              trees (docs-only drift) is accepted automatically, because the
#              binary cannot differ.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BASE=http://127.0.0.1:2601
CORE_PATCH="$REPO_ROOT/patches/uat-core.ini"
ERR_PATCH="$REPO_ROOT/patches/uat-err-register.ini"
WARN_PATCH="$REPO_ROOT/patches/uat-warn-deprecated.ini"
AUTOSAVE="$HOME/Library/Application Support/Rack2/autosave/patch.json"

command -v jq >/dev/null || { echo "jq required"; exit 2; }
[ -f "$CORE_PATCH" ] || { echo "FAIL: missing $CORE_PATCH"; exit 2; }
[ -f "$ERR_PATCH" ] || { echo "FAIL: missing $ERR_PATCH"; exit 2; }
[ -f "$WARN_PATCH" ] || { echo "FAIL: missing $WARN_PATCH"; exit 2; }

fail() { echo "FAIL: $*" >&2; exit 1; }

# Scratch dir for the generated fixtures (state-store copies, the oversize
# patch): they are per-run, deliberately not committed, and must not be written
# next to the curated ones in patches/.
SCRATCH=$(mktemp -d -t uatbridge-smoke)
RACK_PID=""
cleanup() {
    if [ -n "$RACK_PID" ]; then
        kill "$RACK_PID" 2>/dev/null || true
        wait "$RACK_PID" 2>/dev/null || true
    fi
    rm -rf "$SCRATCH"
}
trap cleanup EXIT

# do_http METHOD PATH [JSON-BODY] -> sets HTTP_CODE, HTTP_BODY. Never aborts
# under set -e; connection failures surface as HTTP_CODE=000.
do_http() {
    local method="$1" path="$2" body="${3:-}" resp
    if [ -n "$body" ]; then
        resp=$(curl -s -m 15 -w '\n%{http_code}' -X "$method" \
               -H 'Content-Type: application/json' -d "$body" "$BASE$path") || resp=$'\n000'
    else
        resp=$(curl -s -m 15 -w '\n%{http_code}' -X "$method" "$BASE$path") || resp=$'\n000'
    fi
    HTTP_CODE=$(printf '%s' "$resp" | tail -n1)
    HTTP_BODY=$(printf '%s' "$resp" | sed '$d')
}

assert_code() {
    [ "$HTTP_CODE" = "$1" ] || fail "$2: expected HTTP $1, got $HTTP_CODE (body: $HTTP_BODY)"
    echo "ok: $2 -> HTTP $HTTP_CODE"
}

assert_jq() {
    echo "$HTTP_BODY" | jq -e "$1" >/dev/null 2>&1 || fail "$2: jq '$1' failed on: $HTTP_BODY"
    echo "ok: $2 -> $1"
}

# poll_jq PATH JQ-PREDICATE TIMEOUT-SECS LABEL -- GET PATH until the predicate
# holds, or fail at the deadline. Every "wait for the engine/UI to catch up"
# here goes through this instead of a fixed sleep: the observable is what the
# assertion is about, so poll IT (same rule as tools/uat_run.py's wait_for) and
# the fast path stays fast. Leaves HTTP_CODE/HTTP_BODY on the last response.
poll_jq() {
    local path="$1" pred="$2" timeout="$3" label="$4"
    local deadline=$(( $(date +%s) + timeout ))
    while :; do
        do_http GET "$path"
        if [ "$HTTP_CODE" = "200" ] && echo "$HTTP_BODY" | jq -e "$pred" >/dev/null 2>&1; then
            echo "ok: $label -> $pred"
            return 0
        fi
        [ "$(date +%s)" -ge "$deadline" ] && break
        sleep 0.1
    done
    fail "$label: '$pred' never held within ${timeout}s (last HTTP $HTTP_CODE, body: $HTTP_BODY)"
}

# --- attach or launch ------------------------------------------------------
LAUNCHED=0
if curl -sf -m 2 "$BASE/ping" >/dev/null 2>&1; then
    echo "mode: attach (bridge already up; Rack will be left running)"
else
    if [ -z "${RACK:-}" ]; then
        for cand in "/Applications/VCV Rack 2 Pro.app/Contents/MacOS/Rack" \
                    "/Applications/VCV Rack 2 Free.app/Contents/MacOS/Rack" \
                    "/Applications/Rack.app/Contents/MacOS/Rack"; do
            [ -x "$cand" ] && { RACK="$cand"; break; }
        done
    fi
    [ -n "${RACK:-}" ] && [ -x "$RACK" ] || fail "no bridge on $BASE and Rack executable not found (set RACK=)"
    TEMPLATE="${TEMPLATE-$REPO_ROOT/tests/smoketest_default.vcv}"
    LAUNCH_PATCH=""
    if [ -n "$TEMPLATE" ]; then
        [ -f "$TEMPLATE" ] || fail "template patch not found: $TEMPLATE (set TEMPLATE= to use the autosave)"
        # Launch a scratch copy: POST /rack/save saves to the loaded patch's
        # own path, which would otherwise mutate the repo template.
        LAUNCH_PATCH=$(mktemp -t uatbridge-smoke).vcv
        cp "$TEMPLATE" "$LAUNCH_PATCH"
        echo "mode: launch ($RACK, template $TEMPLATE -> $LAUNCH_PATCH)"
    else
        echo "mode: launch ($RACK, autosave)"
    fi
    VCVOID_UAT_BRIDGE=1 "$RACK" ${LAUNCH_PATCH:+"$LAUNCH_PATCH"} &
    RACK_PID=$!          # cleanup() (EXIT trap) kills it as the failure fallback
    LAUNCHED=1
    for _ in $(seq 1 60); do
        curl -sf -m 2 "$BASE/ping" >/dev/null 2>&1 && break
        sleep 1
    done
    curl -sf -m 2 "$BASE/ping" >/dev/null 2>&1 || fail "bridge never came up (Rack launch or VCVOID_UAT_BRIDGE gate)"
fi

# --- ping: identity + stale-build gate --------------------------------------
do_http GET /ping
assert_code 200 "GET /ping"
assert_jq '.bridgeVersion >= 2' "ping bridgeVersion (>= 2: /diagnostics + hold/release)"
HASH=$(echo "$HTTP_BODY" | jq -r .gitHash)
WANT=$(git -C "$REPO_ROOT" rev-parse --short HEAD)
if [ "$HASH" = "$WANT" ]; then
    echo "ok: ping gitHash matches HEAD ($HASH)"
# The gate exists to catch a stale BINARY, and the binary is built from
# plugin/ + engine/ only. So a hash mismatch whose two commits have identical
# plugin/ and engine/ trees (the common case: docs, patches or runbook commits
# moved HEAD after the install) cannot mean a stale build -- accept it and say
# so, instead of making every doc commit need SKIP_HASH=1.
elif git -C "$REPO_ROOT" rev-parse --verify -q "$HASH^{commit}" >/dev/null &&
     git -C "$REPO_ROOT" diff --quiet "$HASH" HEAD -- plugin engine; then
    echo "ok: gitHash $HASH != HEAD $WANT, but plugin/ and engine/ are byte-identical between them (non-code drift)"
elif [ "${SKIP_HASH:-}" = "1" ]; then
    echo "WARN: gitHash $HASH != HEAD $WANT and the code trees differ (SKIP_HASH=1, continuing)"
else
    fail "stale build: bridge gitHash=$HASH, HEAD=$WANT, and plugin/ or engine/ differs between them. Rebuild with 'cd plugin && make install' (SKIP_HASH=1 overrides, e.g. when the installed build came from another worktree)"
fi

# --- find the master ---------------------------------------------------------
# GET /modules is the source of truth when a bridge is up (autosave lags 15 s);
# fall back to the autosave only if the UI queue is not attached yet.
MASTER_ID="${MASTER_ID:-}"
if [ -z "$MASTER_ID" ]; then
    # /modules 503s until a vcvoid widget attaches the UI queue -- with the
    # launch template's master that is within the first frames, so poll.
    for _ in $(seq 1 30); do
        do_http GET /modules
        [ "$HTTP_CODE" = "200" ] && break
        sleep 1
    done
    if [ "$HTTP_CODE" = "200" ]; then
        MASTER_ID=$(echo "$HTTP_BODY" | jq -r '[.[] | select(.plugin=="vcvoid" and .slug=="master")][0].id // empty')
    elif [ -f "$AUTOSAVE" ]; then
        MASTER_ID=$(jq -r '[.modules[]? | select(.plugin=="vcvoid" and .model=="master")][0].id // empty' "$AUTOSAVE")
    fi
fi
[ -n "$MASTER_ID" ] || fail "no vcvoid master found. Precondition: the rack must already contain a master (the bridge cannot bootstrap a truly empty rack -- add one by hand), or set MASTER_ID"
echo "ok: MASTER_ID=$MASTER_ID"

echo "waiting for master $MASTER_ID to register with the bridge ..."
for _ in $(seq 1 60); do
    do_http GET "/master/$MASTER_ID/status"
    [ "$HTTP_CODE" = "200" ] && break
    sleep 1
done
[ "$HTTP_CODE" = "200" ] || fail "master $MASTER_ID never registered (last HTTP $HTTP_CODE)"
echo "ok: master registered"

# --- diagnostics: no-patch, before this run loads anything --------------------
# (issue #46: the structured record the panel's error display derives from.)
do_http GET "/master/$MASTER_ID/diagnostics"
assert_code 200 "GET diagnostics (before any load)"
if [ -z "$(echo "$HTTP_BODY" | jq -r '.patchPath // ""')" ]; then
    assert_jq '.state == "no-patch"' "diagnostics state no-patch"
    assert_jq '.severity == "info"' "diagnostics no-patch severity"
    assert_jq '.line == 0 and (.warnings | length) == 0' "diagnostics no-patch has no line/warnings"
    assert_jq '.code == ""' "diagnostics no-patch reports no hardware error code"
else
    echo "note: attach mode with a patch already loaded ($(echo "$HTTP_BODY" | jq -r .patchPath)) -- no-patch class not assertable, skipped"
fi

# --- find or self-assemble the p2b8 -----------------------------------------
P2B8_ID="${P2B8_ID:-}"
if [ -z "$P2B8_ID" ]; then
    do_http GET /modules
    assert_code 200 "GET /modules"
    P2B8_ID=$(echo "$HTTP_BODY" | jq -r '[.[] | select(.plugin=="vcvoid" and .slug=="p2b8")][0].id // empty')
fi
if [ -z "$P2B8_ID" ]; then
    # Place it flush on the master's right edge: expander adjacency is what
    # links the chain (no cable). setModulePosForce shoves neighbors aside.
    read -r MX MY MW < <(echo "$HTTP_BODY" | jq -r --argjson id "$MASTER_ID" \
        '.[] | select(.id==$id) | "\(.x) \(.y) \(.width)"')
    PX=$(jq -n "$MX + $MW")
    do_http POST /modules "{\"plugin\":\"vcvoid\",\"slug\":\"p2b8\",\"x\":$PX,\"y\":$MY}"
    assert_code 200 "POST /modules p2b8 (self-assemble)"
    P2B8_ID=$(echo "$HTTP_BODY" | jq -r .id)
    [ -n "$P2B8_ID" ] && [ "$P2B8_ID" != "null" ] && [ "$P2B8_ID" != "-1" ] \
        || fail "self-assemble p2b8: bad id in response (body: $HTTP_BODY)"
fi
echo "ok: P2B8_ID=$P2B8_ID"

# --- patch load + chain ------------------------------------------------------
do_http POST "/master/$MASTER_ID/patch" "{\"path\":\"$CORE_PATCH\"}"
assert_code 200 "POST /master/{id}/patch uat-core.ini"
assert_jq '.patchPath == "'"$CORE_PATCH"'"' "patch load patchPath"
assert_jq '.statusLine | test("ok, [0-9]+ bytes RAM")' "patch load statusLine ok+RAM"

# chainPhysical populates over the expander scan -- allow a few frames.
CHAIN_OK=0
for _ in $(seq 1 10); do
    do_http GET "/master/$MASTER_ID/status"
    if echo "$HTTP_BODY" | jq -e '.chain | index("p2b8") != null' >/dev/null 2>&1; then
        CHAIN_OK=1; break
    fi
    sleep 0.5
done
[ "$CHAIN_OK" = 1 ] || fail "chain: p2b8 not in chain after 5s (body: $HTTP_BODY) -- is the p2b8 physically right of the master?"
echo "ok: chain includes p2b8"

# --- diagnostics: running ------------------------------------------------------
poll_jq "/master/$MASTER_ID/diagnostics" '.state == "running"' 5 "diagnostics state running"
assert_jq '.severity == "ok"' "diagnostics running severity"
assert_jq '.line == 0 and .code == "" and (.warnings | length) == 0' \
    "diagnostics running carries no error/warnings"
assert_jq '.patchPath == "'"$CORE_PATCH"'"' "diagnostics running patchPath"
assert_jq '.stateLine | startswith("state: ")' "diagnostics running stateLine present"

# --- registers ----------------------------------------------------------------
do_http GET "/master/$MASTER_ID/registers?ids=O1,O2,O3,O4,O5,R1"
assert_code 200 "GET registers O1..O5,R1"
for reg in O1 O2 O3 O4 O5 R1; do
    echo "$HTTP_BODY" | jq -e --arg r "$reg" '.[$r] | type == "number"' >/dev/null \
        || fail "registers: $reg not numeric (body: $HTTP_BODY)"
done
echo "ok: registers all numeric"
assert_jq '.O3 == 0' "O3 == 0 (I2 unpatched -- see header precondition if this fails)"

do_http GET "/master/$MASTER_ID/registers"
assert_code 400 "GET registers (missing ids)"
do_http GET "/master/$MASTER_ID/registers?ids=NOTAREG"
assert_code 400 "GET registers NOTAREG"
assert_jq '.name == "NOTAREG"' "unknown-register body echoes name"

# --- read-back shapes ----------------------------------------------------------
do_http GET "/master/$MASTER_ID/midi-ports"
assert_code 200 "GET midi-ports"
do_http GET "/master/$MASTER_ID/faders"
assert_code 200 "GET faders"
do_http GET "/master/$MASTER_ID/leds"
assert_code 200 "GET leds"
assert_jq '.matrix | length == 16' "leds matrix 16 entries"
assert_jq '.buttons | has("L1.1")' "leds buttons has L1.1"

# --- reload + error paths -------------------------------------------------------
do_http POST "/master/$MASTER_ID/reload"
assert_code 200 "POST reload"
assert_jq '.statusLine | test("ok, [0-9]+ bytes RAM")' "reload statusLine ok"
# issue #42: reloading the SAME patch is an exact fingerprint hit, so the patch
# gets its own saved state back rather than a foreign one.
assert_jq '.stateLine | test("^state: restored")' "reload stateLine restored"
do_http POST "/master/$MASTER_ID/patch" '{"path":"relative.ini"}'
assert_code 400 "POST patch (non-absolute path)"
do_http GET "/master/999999999999/status"
assert_code 404 "GET status (unknown master)"

# --- probe: O4 envelope retriggers from N1 at 2 Hz -----------------------------
do_http GET "/probe?moduleId=$MASTER_ID&portId=3&kind=out&ms=1300"
assert_code 200 "GET probe O4"
EDGES=$(echo "$HTTP_BODY" | jq -r .edges)
[[ "$EDGES" =~ ^[0-9]+$ ]] || fail "probe O4: non-numeric edges (body: $HTTP_BODY)"
[ "$EDGES" -ge 2 ] && [ "$EDGES" -le 5 ] \
    || fail "probe O4: expected 2..5 edges at 2Hz over 1300ms, got $EDGES -- cable in I1? (see header)"
assert_jq '.max > .min' "probe O4 signal varies"
echo "ok: probe O4 $EDGES edges"

# --- params: B1.1 toggle -> O5 5 Hz square -------------------------------------
# B1.1 = p2b8 paramId 2 (2 pots then 8 buttons); toggle circuit, state read
# from L1.1 first (persists across runs). One retry per tap: rare misses of a
# 300 ms hold were observed live.
read_l11() {
    do_http GET "/master/$MASTER_ID/registers?ids=L1.1"
    [ "$HTTP_CODE" = "200" ] || fail "read L1.1: HTTP $HTTP_CODE"
    L11=$(echo "$HTTP_BODY" | jq -r '."L1.1"')
}
tap_b11_until() {  # $1 = wanted L1.1 state as jq predicate, $2 = label
    local try deadline
    for try in 1 2; do
        # Read before acting: the toggle's state persists across runs and
        # across the hold/release block below, so an unconditional tap would
        # flip a toggle that is already where we want it.
        read_l11
        echo "$L11" | jq -e "$1" >/dev/null 2>&1 && return 0
        do_http POST /params "{\"moduleId\":$P2B8_ID,\"paramId\":2,\"value\":1,\"holdMs\":300}"
        assert_code 200 "POST /params tap B1.1 ($2, try $try)"
        # Poll the observable to a deadline rather than sleeping out the hold:
        # holdMs is ENGINE sample time (the bridge's frame-deadline holds), and
        # with no Audio module the fallback engine thread can pace ~26% slower
        # than wall clock -- the same reason tools/uat_run.py multiplies its
        # settle. A deadline poll is correct at either pace and returns as soon
        # as the toggle actually lands.
        deadline=$(( $(date +%s) + 4 ))
        while :; do
            read_l11
            if echo "$L11" | jq -e "$1" >/dev/null 2>&1; then
                # The toggle flips on the PRESS edge, so it can land while the
                # 300 ms hold is still running. Wait out the release too --
                # posting the next press onto a still-pressed param produces no
                # edge at all (the same trap gesture_fired() guards in
                # tools/uat_run.py) -- but wait on the observable, not a clock.
                poll_jq "/master/$MASTER_ID/registers?ids=B1.1" '."B1.1" < 0.5' 6 \
                    "tap B1.1 ($2) released"
                return 0
            fi
            [ "$(date +%s)" -ge "$deadline" ] && break
            sleep 0.1
        done
        echo "note: tap did not register within 4s (L1.1=$L11), retrying"
    done
    fail "B1.1 did not reach state '$1' after 2 taps (L1.1=$L11)"
}
tap_b11_until '. >= 0.5' "toggle on"
echo "ok: B1.1 on (L1.1=$L11)"
do_http GET "/probe?moduleId=$MASTER_ID&portId=4&kind=out&ms=1200"
assert_code 200 "GET probe O5"
EDGES=$(echo "$HTTP_BODY" | jq -r .edges)
[[ "$EDGES" =~ ^[0-9]+$ ]] || fail "probe O5: non-numeric edges"
[ "$EDGES" -ge 4 ] && [ "$EDGES" -le 8 ] \
    || fail "probe O5: expected 4..8 edges (~5Hz over 1200ms), got $EDGES"
echo "ok: probe O5 $EDGES edges (5Hz square while B1.1 on)"
tap_b11_until '. < 0.5' "toggle off"
echo "ok: B1.1 back off"

# --- params: un-timed hold / release (bridgeVersion 2) --------------------------
# The two halves of holdMs, for gestures a script must ORDER itself. Contract
# here; the ordered multi-finger gestures they exist for (chords, motoquencer
# start/end) live in tools/uat_run.py.
do_http POST /params/hold "{\"moduleId\":$P2B8_ID,\"paramId\":2}"
assert_code 200 "POST /params/hold B1.1"
assert_jq '.held == true' "hold response"
poll_jq "/master/$MASTER_ID/registers?ids=B1.1" '."B1.1" >= 0.5' 4 "hold: B1.1 reads pressed"
# "Survives across ticks" is a per-TICK claim, so measure it per tick: arm a
# watch, re-hold the already-held param (which must be a no-op, not a
# re-press), and collect. min >= 0.5 over ticks > 0 samples = it never dropped.
do_http POST "/master/$MASTER_ID/watch" '{"ids":["B1.1"]}'
assert_code 200 "watch arm B1.1 (hold window)"
do_http POST /params/hold "{\"moduleId\":$P2B8_ID,\"paramId\":2}"
assert_code 200 "POST /params/hold B1.1 again (idempotent)"
do_http GET "/master/$MASTER_ID/watch"
assert_code 200 "watch collect B1.1 (hold window)"
assert_jq '.signals["B1.1"].ticks > 0' "hold window saw engine ticks"
assert_jq '.signals["B1.1"].min >= 0.5' "hold: B1.1 stayed pressed for every tick of the window"
do_http POST /params/release "{\"moduleId\":$P2B8_ID,\"paramId\":2}"
assert_code 200 "POST /params/release B1.1"
poll_jq "/master/$MASTER_ID/registers?ids=B1.1" '."B1.1" < 0.5' 4 "release: B1.1 back to rest"
do_http POST /params/release "{\"moduleId\":$P2B8_ID,\"paramId\":2}"
assert_code 200 "POST /params/release B1.1 again (idempotent no-op)"
poll_jq "/master/$MASTER_ID/registers?ids=B1.1" '."B1.1" < 0.5' 2 "release: still at rest"
do_http POST /params/hold '{}'
assert_code 400 "params/hold (empty body)"
do_http POST /params/hold "{\"moduleId\":$P2B8_ID,\"paramId\":99}"
assert_code 400 "params/hold (paramId out of range)"
do_http POST /params/release '{"moduleId":999999999999,"paramId":0}'
assert_code 404 "params/release (unknown module)"
# The hold above pressed B1.1 once, which toggled the [button] circuit on.
tap_b11_until '. < 0.5' "toggle off after hold/release"
echo "ok: B1.1 toggle back off after the hold/release block"

# --- validation error paths -----------------------------------------------------
do_http GET "/probe"
assert_code 400 "probe (missing params)"
do_http GET "/probe?moduleId=$MASTER_ID&portId=0&kind=sideways"
assert_code 400 "probe (bad kind)"
do_http GET "/probe?moduleId=$MASTER_ID&portId=9999&kind=out&ms=10"
assert_code 400 "probe (portId out of range)"
do_http GET "/probe?moduleId=999999999999&portId=0&kind=out&ms=10"
assert_code 404 "probe (unknown module)"
do_http POST /params '{}'
assert_code 400 "params (empty body)"
do_http POST /params '{"moduleId":999999999999,"paramId":0,"value":0}'
assert_code 404 "params (unknown module)"
do_http POST /params "{\"moduleId\":$P2B8_ID,\"paramId\":99,\"value\":1}"
assert_code 400 "params (paramId out of range)"
do_http POST /params "{\"moduleId\":$P2B8_ID,\"paramId\":-1,\"value\":1}"
assert_code 400 "params (negative paramId)"

# --- error patch: LOAD ERROR stops the engine -----------------------------------
do_http POST "/master/$MASTER_ID/patch" "{\"path\":\"$ERR_PATCH\"}"
assert_code 200 "POST patch uat-err-register.ini"
assert_jq '.statusLine | test("^LOAD ERROR")' "error patch statusLine LOAD ERROR"
do_http GET "/master/$MASTER_ID/registers?ids=O1"
assert_code 400 "registers after error patch (engine stopped)"
# --- diagnostics: load-failed, with the offending LINE --------------------------
# uat-err-register.ini's `square = O9` is line 7; the line is what the MASTER's
# matrix encodes in its LEDs, so it is the field the panel (#46) needs right.
do_http GET "/master/$MASTER_ID/diagnostics"
assert_code 200 "GET diagnostics (load failed)"
assert_jq '.state == "load-failed"' "diagnostics state load-failed"
assert_jq '.severity == "error"' "diagnostics load-failed severity"
assert_jq '.line == 7' "diagnostics load-failed line (uat-err-register.ini square = O9)"
assert_jq '.code == "unknown_register" and .codeColor == "yellow"' \
    "diagnostics load-failed hardware error code"
assert_jq '.message != ""' "diagnostics load-failed message"
do_http POST "/master/$MASTER_ID/patch" "{\"path\":\"$CORE_PATCH\"}"
assert_code 200 "cleanup reload uat-core.ini"

# --- diagnostics: warnings ------------------------------------------------------
# A deprecated circuit is the only warning class a default master can reach
# (the memory-limit downgrades need the "ignore hardware memory limits"
# opt-in, which has no bridge route). See the patch's own header note about
# droidcheck flagging it.
do_http POST "/master/$MASTER_ID/patch" "{\"path\":\"$WARN_PATCH\"}"
assert_code 200 "POST patch uat-warn-deprecated.ini"
assert_jq '.statusLine | test("ok, [0-9]+ bytes RAM")' "warn patch still loads"
do_http GET "/master/$MASTER_ID/diagnostics"
assert_code 200 "GET diagnostics (warnings)"
assert_jq '.state == "warnings"' "diagnostics state warnings"
assert_jq '.severity == "warning"' "diagnostics warnings severity"
assert_jq '(.warnings | length) >= 1' "diagnostics warnings list populated"
assert_jq '.warnings[0] | test("deprecated")' "diagnostics warning names the deprecation"
assert_jq '.line == 0' "diagnostics warnings carries no error line"

# --- diagnostics: chain-error ---------------------------------------------------
# Provoked WITHOUT touching the rack: a patch that declares a controller the
# chain does not have. (The physical row is master|p2b8.)
CHAIN_PATCH="$SCRATCH/uat-chain-mismatch.ini"
cat > "$CHAIN_PATCH" <<'EOF'
# Smoke fixture: declares an m4 the smoke rack does not have -> CHAIN ERROR
# while the patch itself loads fine.
[m4]

[lfo]
    hz = 1
    square = O1
EOF
do_http POST "/master/$MASTER_ID/patch" "{\"path\":\"$CHAIN_PATCH\"}"
assert_code 200 "POST patch (declares an absent m4)"
assert_jq '.statusLine | test("ok, [0-9]+ bytes RAM")' "chain-mismatch patch loads"
poll_jq "/master/$MASTER_ID/diagnostics" '.state == "chain-error"' 6 "diagnostics state chain-error"
assert_jq '.severity == "error"' "diagnostics chain-error severity"
assert_jq '.message | test("m4")' "diagnostics chain-error message names the mismatch"

# --- diagnostics + #41: a patch over the 64 000-byte deployed size --------------
# The limit is measured on the ABBREVIATED (deployed) form, so generate well
# past it and assert the message quotes the measured size, not the file size.
BIG_PATCH="$SCRATCH/uat-oversize.ini"
{
    echo "# Smoke fixture: deliberately over the 64 000-byte deployed limit (#41)."
    i=0
    while [ "$i" -lt 4000 ]; do
        printf '[copy]\n    input = I1\n    output = _C%s\n\n' "$i"
        i=$((i + 1))
    done
} > "$BIG_PATCH"
do_http POST "/master/$MASTER_ID/patch" "{\"path\":\"$BIG_PATCH\"}"
assert_code 200 "POST patch (oversize)"
do_http GET "/master/$MASTER_ID/diagnostics"
assert_code 200 "GET diagnostics (oversize)"
assert_jq '.state == "load-failed"' "oversize -> load-failed"
assert_jq '.code == "patch_too_big" and .codeColor == "blue"' "oversize hardware code"
assert_jq '.line == 0' "oversize is a global error (no line)"
assert_jq '.message | test("64000")' "oversize message names the limit"
assert_jq '.message | test("[0-9]+ bytes with abbreviated parameter names")' \
    "oversize message names the MEASURED deployed size (#41)"

# --- state store lines (#42) ----------------------------------------------------
# stateLine is "state: restored (saved <timestamp>)" / "state: migrated from
# ..." / "state: fresh" -- assert the PREFIX only, never the timestamp.
# A clean store reads "fresh" on a patch's first ever load, so the restored
# check loads core, goes somewhere else, and comes back.
OTHER_PATCH="$SCRATCH/uat-other.ini"
cat > "$OTHER_PATCH" <<'EOF'
# Smoke fixture: a patch that is simply not uat-core.ini, to move the master
# off it between the two core loads.
[lfo]
    hz = 1
    square = O1
EOF
do_http POST "/master/$MASTER_ID/patch" "{\"path\":\"$CORE_PATCH\"}"
assert_code 200 "state: load uat-core.ini (seeds its store entry)"
do_http POST "/master/$MASTER_ID/patch" "{\"path\":\"$OTHER_PATCH\"}"
assert_code 200 "state: load a different patch"
do_http POST "/master/$MASTER_ID/patch" "{\"path\":\"$CORE_PATCH\"}"
assert_code 200 "state: back to uat-core.ini"
assert_jq '.stateLine | startswith("state: restored")' \
    "stateLine 'state: restored' on returning to a patch this master has run (#42)"
# Migration: same FILE PATH, one more circuit -> a different fingerprint with
# the same lineage. Done on a scratch copy so nothing in patches/ is mutated;
# O6 is unused by uat-core.ini, so the appended circuit cannot collide.
MIG_PATCH="$SCRATCH/uat-core-copy.ini"
cp "$CORE_PATCH" "$MIG_PATCH"
do_http POST "/master/$MASTER_ID/patch" "{\"path\":\"$MIG_PATCH\"}"
assert_code 200 "state: load the scratch copy (same structure, new path)"
printf '\n[copy]\n    input = 0.1\n    output = O6\n' >> "$MIG_PATCH"
do_http POST "/master/$MASTER_ID/patch" "{\"path\":\"$MIG_PATCH\"}"
assert_code 200 "state: load the copy again with one circuit appended"
assert_jq '.statusLine | test("ok, [0-9]+ bytes RAM")' "migrated patch loads"
assert_jq '.stateLine | startswith("state: migrated")' \
    "stateLine 'state: migrated' for a new revision of the same file (#42)"
do_http POST "/master/$MASTER_ID/patch" "{\"path\":\"$CORE_PATCH\"}"
assert_code 200 "cleanup reload uat-core.ini (after state checks)"

# --- lifecycle -------------------------------------------------------------------
do_http POST "/master/$MASTER_ID/tick-rate" '{"hz":2000}'
assert_code 200 "tick-rate 2000"
do_http POST "/master/$MASTER_ID/tick-rate" '{"hz":6000}'
assert_code 200 "tick-rate 6000 (restore)"
do_http POST "/master/$MASTER_ID/tick-rate" '{"hz":5000}'
assert_code 400 "tick-rate 5000 (invalid)"
# sample-rate: setting is accepted but confirmed NOT live-applied (SKILL.md
# caveat) -- assert the validation codes only.
do_http POST /rack/sample-rate '{"hz":48000}'
assert_code 200 "sample-rate 48000 (accepted)"
do_http POST /rack/sample-rate '{"hz":44100}'
assert_code 200 "sample-rate 44100 (restore)"
do_http POST /rack/sample-rate '{"hz":12345}'
assert_code 400 "sample-rate 12345 (invalid)"
# save: 200 on a saved rack, 400 "no patch path" on an unsaved one -- both fine.
do_http POST /rack/save
if [ "$HTTP_CODE" = "200" ]; then
    echo "ok: POST /rack/save -> 200"
elif [ "$HTTP_CODE" = "400" ] && echo "$HTTP_BODY" | grep -q "no patch path"; then
    echo "ok: POST /rack/save -> 400 (unsaved rack, expected)"
else
    fail "POST /rack/save: HTTP $HTTP_CODE (body: $HTTP_BODY)"
fi

echo "SMOKE PASS"

# --- quit only if we launched -----------------------------------------------------
if [ "$LAUNCHED" = 1 ]; then
    do_http POST /rack/quit
    assert_code 200 "POST /rack/quit"
    wait "$RACK_PID" 2>/dev/null || true
    echo "ok: Rack quit gracefully"
else
    echo "ok: attach mode -- Rack left running"
fi
