"""voidbot's experimental-circuit overlay (engine/experimental.json), shared by
its two consumers so the merge policy exists exactly once:

  * tools/jackgen/jackgen.py    — engine jack tables (engine/gen/jacktables.gen.*)
  * tools/droidcheck/build.sh   — the validator's embedded firmware + name list

Both must agree on which circuits are experimental; ADR 0001 rests on that. See
docs/adr/0001-experimental-circuits.md.
"""
import copy
import json
import pathlib
import sys

OVERLAY = pathlib.Path(__file__).resolve().parents[2] / "engine/experimental.json"

# Keys of an overlay entry that drive the derivation itself rather than being
# part of the emitted circuit definition.
_DERIVE_KEYS = ("extends", "extra_inputs", "extra_outputs")


def _derive(name, spec, firmware_circuits):
    """Resolve an `extends` entry into a full circuit definition.

    An experimental circuit that is "circuit X plus one jack" (motoquencer2 =
    motoquencer + probabilitymode) declares

        "extends": "motoquencer", "extra_inputs": [ ... ]

    instead of copying X's whole jack list. The copy would be the larger
    fidelity risk of the two: a 100-jack transcription silently drifts the day
    the Forge changes one of those jacks, and drift between voidbot and the
    firmware is exactly what ADR 0001 exists to prevent. Deriving keeps the
    inherited jacks a pure function of the vendored firmware file; only the
    delta lives in the overlay.

    Any other key present in the entry overrides the base's (title,
    description, ramsize, category, ...). `extra_inputs` / `extra_outputs` are
    APPENDED to the base's lists, so the inherited jacks keep their order.
    """
    base_name = spec["extends"]
    base = firmware_circuits.get(base_name)
    if base is None:
        sys.exit(f"experimental circuit '{name}' extends unknown circuit '{base_name}'")
    c = copy.deepcopy(base)
    for key, value in spec.items():
        if key not in _DERIVE_KEYS:
            c[key] = copy.deepcopy(value)
    for key, extra in (("inputs", "extra_inputs"), ("outputs", "extra_outputs")):
        added = copy.deepcopy(spec.get(extra, []))
        # A duplicate jack name or short form would be silently accepted by the
        # generators and then break both patch abbreviation (two jacks claiming
        # one short) and the Forge's jack lookup.
        taken_names = {j["name"] for j in c.get(key, [])}
        taken_shorts = {j.get("short") for j in c.get(key, [])} - {None, ""}
        for j in added:
            if j["name"] in taken_names:
                sys.exit(f"experimental circuit '{name}': {extra} jack "
                         f"'{j['name']}' already exists on '{base_name}'")
            if j.get("short") and j["short"] in taken_shorts:
                sys.exit(f"experimental circuit '{name}': {extra} jack "
                         f"'{j['name']}' reuses short form '{j['short']}' "
                         f"of '{base_name}'")
        c[key] = c.get(key, []) + added
    return c


def load(firmware_circuits, path=None):
    """Return [(name, circuit_def), ...] from the overlay, sorted by name.

    Sorted so codegen output is deterministic. A name that already exists in
    the firmware is a hard error rather than a silent override: the point of
    the overlay is to ADD circuits voidbot alone has, never to redefine a
    hardware one (which would make a patch look hardware-valid when it is not).

    An entry carrying `extends` is derived from that firmware circuit's
    definition (see _derive); the result is a plain circuit definition, so both
    consumers see exactly the same thing they see for a hand-written entry.
    """
    p = pathlib.Path(path) if path else OVERLAY
    if not p.exists():
        return []
    circuits = json.loads(p.read_text()).get("circuits", {})
    for name in circuits:
        if name in firmware_circuits:
            sys.exit(f"experimental circuit '{name}' collides with a firmware circuit")
    return sorted((name, _derive(name, spec, firmware_circuits)
                   if "extends" in spec else spec)
                  for name, spec in circuits.items())
