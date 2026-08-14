#!/usr/bin/env python3
"""Render a module panel with its register-label chips, exactly as the plugin
draws them (issue #26) — without launching Rack.

Chip rectangles come from build/layout.json, which the Rack-free dumper emits
straight out of Layout.hpp, and the labels come from build/labeldump, which
`make labelcheck` holds to the Forge's own parser. So this preview cannot drift
from what the plugin renders: both sides read the same two sources.

    make build/layoutdump build/labeldump
    build/layoutdump > build/layout.json
    tools/labelpreview.py master patches/uat-core.ini > /tmp/preview.svg

The output is an SVG at Rack's 100% zoom (1 HP = 15 px) with the faceplate PNG
embedded, so what you see is the on-screen size.
"""
import base64
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HP_MM = 5.08
PX_PER_MM = 75.0 / 25.4          # Rack: mm2px at zoom 1
ART_PX_PER_MM = 22.75            # the faceplate render scale (dw::ArtMap)
LABEL_FONT_HP = 50.0 / 87.0      # Forge QFont::setPixelSize(50) at 87 px/HP
LABEL_RADIUS_HP = 10.0 / 87.0
MAX_CHARS = 18
# Modules that place overlays with plain hpVec() rather than through the art.
PLAIN = {"m4"}
# Faceplate pixel sizes, as passed to dw::setupPanel / dw::ArtMap per module.
ART = {
    "master": (928, 2917), "master18": (692, 2917), "x7": (462, 2915),
    "g8": (461, 2915), "p2b8": (577, 2915), "p4b2": (577, 2915),
    "p10": (576, 2915), "s10": (577, 2915), "p8s8": (923, 2915),
    "b32": (1153, 2915), "e4": (692, 2915), "m4": (0, 0), "db8e": (692, 2918),
}


def hp_px(hp):
    return hp * HP_MM * PX_PER_MM


def art_x(slug, hp_val, box_w):
    if slug in PLAIN:
        return hp_px(hp_val)
    return hp_val * HP_MM * ART_PX_PER_MM * box_w / ART[slug][0]


def art_y(slug, hp_val, box_h):
    if slug in PLAIN:
        return hp_px(hp_val)
    return hp_val * HP_MM * ART_PX_PER_MM * box_h / ART[slug][1]


# Share Tech Mono advances 0.5 em per glyph. The plugin measures exactly with
# nvgTextBounds; this approximation is within a character of it, and the chip
# is clipped besides, so the preview can never claim text fits when it does not.
MONO_ADVANCE_EM = 0.5


def chip_text(label, width_px, font_px):
    short, text = label
    s = short if short else text[:MAX_CHARS]
    fits = max(1, int((width_px - 2) / (MONO_ADVANCE_EM * font_px)))
    if len(s) > fits:
        s = s[:max(1, fits - 1)] + "\u2026"
    return s


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    slug, patch = sys.argv[1], sys.argv[2]

    layout = json.loads((ROOT / "build/layout.json").read_text())
    if slug not in layout:
        sys.exit(f"unknown module {slug}; have {', '.join(sorted(layout))}")
    mod = layout[slug]

    # Labels, keyed the way ModuleLabels::find() keys them. This preview only
    # covers a master, so controller/G8 offsets are not applied.
    out = subprocess.run([str(ROOT / "build/labeldump"), patch],
                         capture_output=True, text=True, check=True).stdout
    labels = {}
    for line in out.splitlines():
        _, _, t, ctrl, g8, num, rest = line.split(" ", 6)
        short, text = rest.split("|", 2)[1], rest.split("|", 2)[2]
        labels[(t, int(num))] = (short, text)

    box_w, box_h = mod["hp"] * 15.0, 128.5 * PX_PER_MM
    png = ROOT / "plugin/res/faceplates" / f"{slug}.png"
    b64 = base64.b64encode(png.read_bytes()).decode() if png.exists() else ""

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{box_w:.1f}" '
        f'height="{box_h:.1f}" viewBox="0 0 {box_w:.1f} {box_h:.1f}">',
        f'<image href="data:image/png;base64,{b64}" x="0" y="0" '
        f'width="{box_w:.1f}" height="{box_h:.1f}"/>' if b64 else "",
    ]
    font_px = hp_px(LABEL_FONT_HP)
    radius = hp_px(LABEL_RADIUS_HP)
    drawn = "IOGBPESRX"
    for c in mod["controls"]:
        if c["type"] not in drawn:
            continue
        lab = labels.get((c["type"], c["n"]))
        if lab is None and c["type"] == "I":
            lab = labels.get(("N", c["n"]))     # a jack shows its N label
        if lab is None:
            continue
        r = c["label"]
        x, y = art_x(slug, r["x"], box_w), art_y(slug, r["y"], box_h)
        w, h = hp_px(r["w"]), hp_px(r["h"])
        parts.append(
            f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" '
            f'rx="{radius:.2f}" fill="#fff" stroke="#000" stroke-width="1"/>')
        parts.append(
            f'<text x="{x + w / 2:.2f}" y="{y + h / 2:.2f}" fill="#000" '
            f'font-family="Share Tech Mono, monospace" font-size="{font_px:.2f}" '
            f'text-anchor="middle" dominant-baseline="central">'
            f'{chip_text(lab, w, font_px)}</text>')
    parts.append("</svg>")
    print("\n".join(p for p in parts if p))


if __name__ == "__main__":
    main()
