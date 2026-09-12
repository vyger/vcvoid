#!/usr/bin/env python3
"""Compress a droid.ini by abbreviating parameter names (blue-6 shorts),
the same transformation the Forge's "Use abbreviated parameter names"
preference applies before upload. Comments and layout are preserved — the
64,000-byte hardware limit ignores spaces and comments, so readability
costs nothing.

Usage: tools/inicompress.py in.ini > out.ini
Shorts come from the Forge's droidfirmware.json (vendored via droidcheck),
plus vcvoid's experimental-circuit overlay (engine/experimental.json).

The engine measures the same thing in C++ (droid::deployedPatchSize, issue
#41); `make sizecheck` diffs the two over every patch in patches/.
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
FW = ROOT / "tools/droidcheck/vendor/droidforge/droidforge/droidfirmware.json"
sys.path.insert(0, str(ROOT / "tools/jackgen"))
import overlay          # noqa: E402  — tools/jackgen/overlay.py, shared with make gen


def short_names(circuit):
    """{parameter name: short form} for one circuit.

    Mirrors the Forge's DroidFirmware::jackShortname: inputs are searched
    before outputs (first match wins), a scalar matches its own name, and an
    array jack matches prefix + element number only — a bare prefix is not a
    jack name to the Forge, so `pitch` (meaning `pitch1`) stays verbose.
    Jacks without a "short" stay verbose too.
    """
    names = {}
    for io in ("inputs", "outputs"):
        for j in circuit.get(io, []):
            short = j.get("short")
            if not short:
                continue
            if j.get("count"):
                prefix = j.get("prefix")
                if not prefix:
                    prefix = re.sub(r"\d+$", "", j["name"].split()[0].rstrip(","))
                # The firmware file carries the real element numbering; the
                # Forge hardcodes 1..count and so mis-handles the single
                # zero-based array (calibrator's tune0 … tune8). Follow the
                # firmware, not that bug.
                start = j.get("start_at", 1)
                for k in range(start, start + j["count"]):
                    names.setdefault(f"{prefix}{k}", f"{short}{k}")
            else:
                names.setdefault(j["name"].split()[0].rstrip(","), short)
    return names


def stripped_size(text):
    """Byte count the hardware enforces: comments and whitespace removed.

    Quote-aware, like the engine's stripPatch(): whitespace and '#' inside a
    quoted text value are content and are counted.
    """
    n = 0
    for line in text.splitlines():
        kept, in_quote = [], False
        for c in line:
            if c == '"':
                in_quote = not in_quote
                kept.append(c)
            elif in_quote:
                kept.append(c)
            elif c == "#":
                break
            elif not c.isspace():
                kept.append(c)
        if kept:
            n += len(kept) + 1
    return n


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    fw = json.loads(FW.read_text())
    defs = dict(fw["circuits"])
    defs.update(overlay.load(fw["circuits"]))
    circuits = {name: short_names(c) for name, c in defs.items()}
    src = pathlib.Path(sys.argv[1]).read_text()

    out = []
    shorts = None
    for line in src.splitlines():
        m = re.match(r"\s*\[(\w+)\]", line)
        if m:
            shorts = circuits.get(m.group(1))   # None for controllers
        else:
            p = re.match(r"(\s*)([A-Za-z][A-Za-z0-9]*)(\s*=.*)", line)
            if p and shorts:
                short = shorts.get(p.group(2).lower(), p.group(2))
                if len(short) < len(p.group(2)):
                    line = p.group(1) + short + p.group(3)
        out.append(line)
    result = "\n".join(out) + "\n"
    sys.stdout.write(result)
    print(f"stripped size: {stripped_size(src)} -> {stripped_size(result)} "
          f"bytes (limit 64000)", file=sys.stderr)


if __name__ == "__main__":
    main()
