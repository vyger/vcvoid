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
# droidcheck's exit code is its count of problem patches, so it is nonzero in
# normal operation and cannot gate anything here — keep stderr instead.
"$DROIDCHECK" --labels --experimental patches/*.ini 2>"$TMP/forge.err" \
    | grep '^LABEL ' | sort > "$TMP/forge.txt" || true
"$OURS" patches/*.ini | sort > "$TMP/ours.txt"

# A droidcheck that fails for ANY reason still exits quietly with an empty label
# list, and the diff below would then blame engine/src/labels.cpp for every label
# we correctly found. The usual cause is a stale binary: build/ is git-ignored, so
# pulling a Forge-side change (e.g. --labels itself, issue #26) leaves the old one
# in place, which parses the unknown flag as a patch path. Catch that here rather
# than emitting a misleading wall of "+LABEL" additions.
if [ ! -s "$TMP/forge.txt" ] && [ -s "$TMP/ours.txt" ]; then
    echo "labelcheck: droidcheck produced no labels at all, but we found $(wc -l < "$TMP/ours.txt" | tr -d ' ')."
    echo "  This is a droidcheck problem, not a labels.cpp problem — most likely a"
    echo "  stale binary predating a Forge-side change. Rebuild it:"
    echo "      tools/droidcheck/build.sh"
    if [ -s "$TMP/forge.err" ]; then
        echo "  droidcheck stderr:"
        sed 's/^/    /' "$TMP/forge.err"
    fi
    exit 1
fi

if diff -u "$TMP/forge.txt" "$TMP/ours.txt" > "$TMP/diff.txt"; then
    echo "labelcheck: $(wc -l < "$TMP/ours.txt" | tr -d ' ') label(s) match the Forge across $(ls patches/*.ini | wc -l | tr -d ' ') patches"
    exit 0
fi
echo "labelcheck: our labels differ from the Forge's (-forge +ours):"
sed 's/^/  /' "$TMP/diff.txt"
exit 1
