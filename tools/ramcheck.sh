#!/bin/sh
# RAM-accounting parity check (issue #88): engine/src/ram.cpp must report the
# same number of bytes as the Forge's own Patch::usedRAM, for every patch in
# patches/, in BOTH deploy modes — with the preference "Detect and share
# duplicate values for inputs" off ("plain") and on ("shared").
#
# Both sides print the same "RAM <file> <mode> <bytes>" lines; we sort and diff
# them, the way labelcheck does. Skips itself when droidcheck is unbuilt.
set -eu
cd "$(dirname "$0")/.."
DROIDCHECK=tools/droidcheck/build/droidcheck
OURS=build/ramdump
if [ ! -x "$DROIDCHECK" ]; then
    echo "ramcheck: droidcheck not built, SKIPPED (run tools/droidcheck/build.sh)"
    exit 0
fi
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# --experimental: the voidbot-only circuits are in droidcheck's firmware but are
# reported as problems without the flag, and a problem patch still measures
# fine — we are comparing RAM, not validity. droidcheck's exit code is its count
# of problem patches, so it cannot gate anything here; keep stderr instead.
"$DROIDCHECK" --ram --experimental patches/*.ini 2>"$TMP/forge.err" \
    | grep '^RAM ' | sort > "$TMP/forge-all.txt" || true
"$OURS" patches/*.ini 2>"$TMP/ours.err" | sort > "$TMP/ours.txt"

# ramdump leaves out any patch our engine refuses outright — see its comment;
# the deliberately broken UAT fixtures in patches/ are not comparable. Compare
# only the files it did measure, and name the rest so the set cannot shrink
# unnoticed.
cut -d' ' -f2 "$TMP/ours.txt" | sort -u > "$TMP/files.txt"
awk 'NR==FNR { keep[$1]; next } ($2 in keep)' "$TMP/files.txt" "$TMP/forge-all.txt" \
    > "$TMP/forge.txt"
if [ -s "$TMP/ours.err" ]; then
    sed 's/^ramdump: /  not comparable: /' "$TMP/ours.err"
fi

# A droidcheck that fails for ANY reason still exits quietly with no RAM lines,
# and the diff below would then blame ram.cpp for every patch. The usual cause
# is a stale binary: build/ is git-ignored, so pulling a Forge-side change (or
# --ram itself) leaves the old one in place, which treats the unknown flag as a
# patch path. Say so plainly rather than printing a wall of bogus differences.
if [ ! -s "$TMP/forge.txt" ] && [ -s "$TMP/ours.txt" ]; then
    echo "ramcheck: droidcheck reported no RAM figures at all."
    echo "  This is a droidcheck problem, not a ram.cpp problem — most likely a"
    echo "  stale binary predating --ram (issue #88). Rebuild it:"
    echo "      tools/droidcheck/build.sh"
    if [ -s "$TMP/forge.err" ]; then
        echo "  droidcheck stderr:"
        sed 's/^/    /' "$TMP/forge.err"
    fi
    exit 1
fi

if diff -u "$TMP/forge.txt" "$TMP/ours.txt" > "$TMP/diff.txt"; then
    echo "ramcheck: RAM matches the Forge in both modes across \
$(wc -l < "$TMP/files.txt" | tr -d ' ') of \
$(ls patches/*.ini | wc -l | tr -d ' ') patches \
($(wc -l < "$TMP/ours.txt" | tr -d ' ') measurements)"
    exit 0
fi
echo "ramcheck: our RAM figures differ from the Forge's (-forge +ours):"
sed 's/^/  /' "$TMP/diff.txt"
exit 1
