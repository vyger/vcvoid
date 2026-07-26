"""vcvoid's experimental-circuit overlay (engine/experimental.json), shared by
its two consumers so the merge policy exists exactly once:

  * tools/jackgen/jackgen.py    — engine jack tables (engine/gen/jacktables.gen.*)
  * tools/droidcheck/build.sh   — the validator's embedded firmware + name list

Both must agree on which circuits are experimental; ADR 0001 rests on that. See
docs/adr/0001-experimental-circuits.md.
"""
import json
import pathlib
import sys

OVERLAY = pathlib.Path(__file__).resolve().parents[2] / "engine/experimental.json"


def load(firmware_circuits, path=None):
    """Return [(name, circuit_def), ...] from the overlay, sorted by name.

    Sorted so codegen output is deterministic. A name that already exists in
    the firmware is a hard error rather than a silent override: the point of
    the overlay is to ADD circuits vcvoid alone has, never to redefine a
    hardware one (which would make a patch look hardware-valid when it is not).
    """
    p = pathlib.Path(path) if path else OVERLAY
    if not p.exists():
        return []
    circuits = json.loads(p.read_text()).get("circuits", {})
    for name in circuits:
        if name in firmware_circuits:
            sys.exit(f"experimental circuit '{name}' collides with a firmware circuit")
    return sorted(circuits.items())
