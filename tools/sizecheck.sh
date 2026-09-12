#!/bin/sh
# Parity check for the patch-size measurement (issue #41): the size the engine
# enforces the 64 000-byte limit on — stripped, with parameter names abbreviated
# the way the Forge deploys them — must equal what tools/inicompress.py reports
# for the same patch. The two implementations share only the firmware file, so a
# disagreement means one of them abbreviates a jack the other does not.
# Skips itself when the Forge checkout is missing, like labelcheck does.
set -eu
cd "$(dirname "$0")/.."
OURS=build/patchsize
FW=tools/droidcheck/vendor/droidforge/droidforge/droidfirmware.json
if [ ! -f "$FW" ]; then
    echo "sizecheck: Forge checkout missing, SKIPPED (run tools/droidcheck/build.sh)"
    exit 0
fi
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

fail=0
n=0
for ini in patches/*.ini; do
    # inicompress reports "stripped size: <verbose> -> <abbreviated> bytes ..."
    theirs=$(python3 tools/inicompress.py "$ini" 2>&1 >/dev/null \
             | sed -n 's/^stripped size: [0-9]* -> \([0-9]*\) bytes.*/\1/p')
    ours=$("$OURS" "$ini" | awk '{print $4}')
    if [ "$theirs" != "$ours" ]; then
        echo "sizecheck: $ini — inicompress says $theirs bytes, the engine says $ours"
        fail=$((fail + 1))
    fi
    n=$((n + 1))
done
if [ "$fail" -eq 0 ]; then
    echo "sizecheck: deployed size matches tools/inicompress.py across $n patches"
fi
exit "$fail"
