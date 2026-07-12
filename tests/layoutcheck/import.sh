#!/bin/sh
# Stage the Forge's layout classes for the parity check. Quoted includes search
# the including file's directory first, so we copy the sources into a build dir
# that contains OUR stub module.h/tuning.h instead of the Forge's Qt-heavy ones.
set -eu
cd "$(dirname "$0")/../.."
V=tools/droidcheck/vendor/droidforge/droidforge
OUT=build/layoutcheck-src
[ -d "$V/modules" ] || { echo "vendor missing; run tools/droidcheck/build.sh" >&2; exit 1; }
mkdir -p "$OUT"
for m in master master18 x7 g8 p2b8 p4b2 p10 s10 p8s8 b32 e4 m4 db8e; do
    cp "$V/modules/module$m.cpp" "$V/modules/module$m.h" "$OUT/"
done
cp "$V/main/tuning.h" "$OUT/tuning.h"
cp tests/layoutcheck/stubs/module.h "$OUT/module.h"
