#!/usr/bin/env python3
"""Compress a droid.ini by abbreviating parameter names (blue-6 shorts),
the same transformation the Forge's "Use abbreviated parameter names"
preference applies before upload. Comments and layout are preserved — the
64,000-byte hardware limit ignores spaces and comments, so readability
costs nothing.

Usage: tools/inicompress.py in.ini > out.ini
Shorts come from the Forge's droidfirmware.json (vendored via droidcheck).
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
FW = ROOT / "tools/droidcheck/vendor/droidforge/droidforge/droidfirmware.json"


def jack_maps(circuit):
    """(scalar name->short, array prefix->short) for one firmware circuit."""
    scalars, arrays = {}, {}
    for io in ("inputs", "outputs"):
        for j in circuit.get(io, []):
            short = j.get("short")
            if not short:
                continue
            if j.get("count"):
                prefix = j.get("prefix")
                if not prefix:
                    prefix = re.sub(r"\d+$", "", j["name"].split()[0].rstrip(","))
                arrays[prefix] = short
            else:
                scalars[j["name"].split()[0].rstrip(",")] = short
    return scalars, arrays


def shorten(name, scalars, arrays):
    """Mirror the engine's findJack order: exact scalar first, then the
    longest array prefix with the element index appended. Skip when the
    abbreviation would collide with a scalar (e.g. bare `led` -> `l` when
    a scalar `length` also shortens to `l` — the engine resolves scalars
    first, so the abbreviation would change meaning)."""
    if name in scalars:
        return scalars[name]
    for prefix in sorted(arrays, key=len, reverse=True):
        digits = name[len(prefix):]
        if name.startswith(prefix) and (digits == "" or digits.isdigit()):
            cand = arrays[prefix] + digits
            if cand in scalars or cand in scalars.values():
                return name
            return cand
    return name


def stripped_size(text):
    """Byte count the hardware enforces: comments and whitespace removed."""
    n = 0
    for line in text.splitlines():
        line = "".join(line.split("#", 1)[0].split())
        if line:
            n += len(line) + 1
    return n


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    fw = json.loads(FW.read_text())
    circuits = {name: jack_maps(c) for name, c in fw["circuits"].items()}
    src = pathlib.Path(sys.argv[1]).read_text()

    out = []
    maps = None
    for line in src.splitlines():
        m = re.match(r"\s*\[(\w+)\]", line)
        if m:
            maps = circuits.get(m.group(1))   # None for controllers
        else:
            p = re.match(r"(\s*)([A-Za-z][A-Za-z0-9]*)(\s*=.*)", line)
            if p and maps:
                short = shorten(p.group(2), *maps)
                if len(short) < len(p.group(2)):
                    line = p.group(1) + short + p.group(3)
        out.append(line)
    result = "\n".join(out) + "\n"
    sys.stdout.write(result)
    print(f"stripped size: {stripped_size(src)} -> {stripped_size(result)} "
          f"bytes (limit 64000)", file=sys.stderr)


if __name__ == "__main__":
    main()
