#!/bin/sh
# Parity check for register-label extraction (issue #26): our engine/src/labels.cpp
# must find exactly the labels the Forge's own PatchParser finds, in every patch.
# Both sides print the same "LABEL <file> <type> <ctrl> <g8> <num> |<short>|<text>"
# lines; we sort and diff them.
set -eu
cd "$(dirname "$0")/.."
DROIDCHECK=tools/droidcheck/build/droidcheck
OURS=build/labeldump
if [ ! -x "$DROIDCHECK" ]; then
    echo "labelcheck: droidcheck not built, SKIPPED (run tools/droidcheck/build.sh)"
    exit 0
fi
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# The Forge's parser throws on the vcvoid-only experimental circuits unless they
# are allowed, and a parse failure yields no labels at all — so always pass the
# flag here. We are comparing label extraction, not circuit validity.
"$DROIDCHECK" --labels --experimental patches/*.ini 2>/dev/null \
    | grep '^LABEL ' | sort > "$TMP/forge.txt" || true
"$OURS" patches/*.ini | sort > "$TMP/ours.txt"

if diff -u "$TMP/forge.txt" "$TMP/ours.txt" > "$TMP/diff.txt"; then
    echo "labelcheck: $(wc -l < "$TMP/ours.txt" | tr -d ' ') label(s) match the Forge across $(ls patches/*.ini | wc -l | tr -d ' ') patches"
    exit 0
fi
echo "labelcheck: our labels differ from the Forge's (-forge +ours):"
sed 's/^/  /' "$TMP/diff.txt"
exit 1
