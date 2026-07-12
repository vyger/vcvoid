#!/bin/sh
# Render every module's screenshot via Rack's built-in screenshot mode and
# stage them for review/diffing. Requires VCV Rack 2 installed and the plugin
# built+installed (make -C plugin install).
set -eu
RACK_APP="${RACK_APP:-/Applications/VCV Rack 2 Pro.app/Contents/MacOS/Rack}"
[ -x "$RACK_APP" ] || RACK_APP="/Applications/VCV Rack 2 Free.app/Contents/MacOS/Rack"
RACK_USER="${RACK_USER:-$HOME/Library/Application Support/Rack2}"
"$RACK_APP" -t 3   # zoom 3 => ~14.2 px/mm; writes $RACK_USER/screenshots/<plugin>/<module>.png
OUT=build/panelshots
mkdir -p "$OUT"
cp "$RACK_USER/screenshots/vcvoid/"*.png "$OUT/"
echo "panelshots in $OUT:"
ls "$OUT"
