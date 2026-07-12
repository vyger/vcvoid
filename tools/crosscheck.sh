#!/bin/sh
# Cross-validate golden patches against the Forge's own validator (droidcheck).
# Goldens that expect a successful load must be problem-free in the Forge too.
# Goldens with expect_load_error are skipped (message wording differs).
set -eu
cd "$(dirname "$0")/.."
DROIDCHECK=tools/droidcheck/build/droidcheck
if [ ! -x "$DROIDCHECK" ]; then
    echo "droidcheck not built — run tools/droidcheck/build.sh first" >&2
    exit 2
fi
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
fail=0
for gold in $(find tests/golden -name '*.gold' | sort); do
    if grep -q '^expect_load_error' "$gold"; then
        echo "SKIP $gold (expects load error)"
        continue
    fi
    ini="$TMP/$(basename "$gold" .gold).ini"
    awk '/^patch <<</{flag=1;next} /^>>>$/{flag=0} flag' "$gold" > "$ini"
    if "$DROIDCHECK" "$ini" > "$TMP/out.txt" 2>&1; then
        echo "PASS $gold"
    else
        # Recorded divergence: the Forge flags obsolete circuits (togglebutton,
        # fourstatebutton, ...) as "deprecated" problems, but we still emulate
        # them (the hardware runs them). A golden that exercises a deprecated
        # circuit is expected to trip this — accept it as long as EVERY reported
        # problem is a deprecation notice (any other problem is a real
        # Forge-parity bug). Problem detail lines are indented "[section ...]".
        probs=$(grep -c '^[[:space:]]*\[section' "$TMP/out.txt" || true)
        deps=$(grep -c 'This circuit is deprecated' "$TMP/out.txt" || true)
        if [ "$probs" -gt 0 ] && [ "$probs" -eq "$deps" ]; then
            echo "PASS $gold (deprecated-circuit divergence, accepted)"
        else
            echo "FAIL $gold — the Forge reports problems we do not:"
            sed 's/^/  /' "$TMP/out.txt"
            fail=$((fail + 1))
        fi
    fi
done
exit "$fail"
