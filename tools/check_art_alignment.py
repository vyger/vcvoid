#!/usr/bin/env python3
"""Layer-2 check: Layout.hpp positions must land on the controls actually
painted in the faceplate renders. Detects circular features (jacks, knobs,
round buttons) with a Hough transform and asserts every expected circular
control center is within TOL_PX of a detected circle center.

Only circle-like types are checked: P (pots/knobs), I/N/O/G (jacks), and B
(round buttons). Switches, sliders, encoders, LED rings, displays are skipped
(not reliably circular). N shares the I jack (normalization = same hole).
usage: check_art_alignment.py layout.json faceplates_dir
"""
import json, sys, math
from collections import defaultdict
import cv2
import numpy as np

HP_MM = 5.08
TOL_PX = 8          # kept <= 10px per the tuning note; do not raise to compensate
CIRCLE_TYPES = set("PIOGB")   # N intentionally excluded: same hole as I

# Empirically-calibrated render scale: the faceplate PNGs are NOT exactly
# height/128.5mm (the naive "3U panel height" assumption) -- fit against
# several modules' jack/knob grids (which are parity-proven correct in HP
# units by `make layoutcheck`, Layer 1) shows the true export scale is
# ~22.75 px/mm, about 0.2-0.6% off the naive per-image guess. That sounds
# tiny but compounds linearly across an 8HP-wide panel into an 8-12px drift
# at the far edge -- exactly the near-miss failures this constant fixes.
# Calibrated once against plugin/res/faceplates/master.png (color-segmented
# jack-surround discs vs. Layout.hpp's own I/O grid spacing); reused as a
# single global constant since all faceplates come from the same render
# pipeline/DPI.
PX_PER_MM = 22.75

# Modules/control-types that defeat Hough circle detection on the rendered
# faceplate art because they are not actually circular controls, even though
# the Forge's abstract register type is reused for them. Verified manually
# per the tuning note in task-11-brief.md (no silent exclusions):
#   - p8s8 "P" registers are physically SLIDERS (the module is 8 faders +
#     8 rotary switches); their register type is "P" (pot) in the Forge's
#     data model but the artwork renders a rectangular fader cap, not a
#     disc. Verified visually: plugin/res/faceplates/p8s8.png shows vertical
#     slider slots at every P position; the round controls in that image are
#     the S (switch) row, which is already outside CIRCLE_TYPES.
#   - m4 "P" registers are the motor-fader slots (the caps are drawn by the
#     widget, not baked into the art; the slot is a rectangle, not a disc) --
#     plugin/res/faceplates/m4.png.
#   - m4 "B" registers are the touch plates (manual/hardware.md "The touch
#     plates"). The faceplate is blank there -- the widget draws a standard
#     VCV light-button (the vendor art's icon graphic was DMMDM branding,
#     removed in the vcvoid rebrand) -- so there is no circle in the art to
#     detect.
EXCLUDE = {
    "p8s8": {"P"},
    "m4": {"P", "B"},
}


def main(layout_path, facedir):
    layout = json.load(open(layout_path))
    failures = 0
    for name, mod in layout.items():
        img = cv2.imread(f"{facedir}/{name}.png", cv2.IMREAD_GRAYSCALE)
        if img is None:
            print(f"FAIL {name}: faceplate missing"); failures += 1; continue
        excl = EXCLUDE.get(name, set())
        expected = [c for c in mod["controls"] if c["type"] in CIRCLE_TYPES and c["type"] not in excl]
        if not expected:
            continue
        img_blur = cv2.medianBlur(img, 5)
        # Group by control size: lumping very different physical sizes (e.g.
        # a 2.7 HP button with a 4.1 HP knob) into one Hough pass forces a
        # minDist/maxRadius compromise that misses the smaller control or
        # over-merges nearby detections. Each distinct size gets its own
        # pass with radius bounds scaled to it.
        groups = defaultdict(list)
        for c in expected:
            groups[round(c["size"], 2)].append(c)
        for size, ctrls in groups.items():
            rad = size / 2 * HP_MM * PX_PER_MM
            circles = cv2.HoughCircles(
                img_blur, cv2.HOUGH_GRADIENT, dp=1,
                minDist=max(int(rad * 0.8), 1),
                param1=80, param2=20,
                minRadius=max(int(rad * 0.5), 1), maxRadius=int(rad * 1.3))
            centers = circles[0][:, :2] if circles is not None else np.empty((0, 2))
            for c in ctrls:
                ex = c["x"] * HP_MM * PX_PER_MM
                ey = c["y"] * HP_MM * PX_PER_MM
                d = min((math.hypot(ex - cx, ey - cy) for cx, cy in centers),
                        default=1e9)
                if d > TOL_PX:
                    print(f"FAIL {name} {c['type']}{c['n']}: expected ({ex:.0f},{ey:.0f}) "
                          f"nearest detected circle {d:.1f}px away")
                    failures += 1
    if failures:
        print(f"artcheck: {failures} failure(s)"); return 1
    print("artcheck: all circular controls align with the artwork"); return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))
