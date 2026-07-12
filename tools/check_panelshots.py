#!/usr/bin/env python3
"""Layer-3 regression: current Rack screenshots vs the approved baseline.
usage: check_panelshots.py tests/panel-baseline build/panelshots"""
import sys, os
import numpy as np
from PIL import Image

def main(basedir, curdir):
    if not os.path.isdir(basedir):
        print("panelshots: no baseline yet — run tools/panelshots.sh and "
              "approve shots into tests/panel-baseline/")
        return 0
    failures = 0
    for f in sorted(os.listdir(basedir)):
        if not f.endswith(".png"):
            continue
        cur = os.path.join(curdir, f)
        if not os.path.exists(cur):
            print(f"FAIL {f}: no current screenshot"); failures += 1; continue
        a = np.asarray(Image.open(os.path.join(basedir, f)).convert("RGB"), float)
        b = np.asarray(Image.open(cur).convert("RGB"), float)
        if a.shape != b.shape:
            print(f"FAIL {f}: size {b.shape} != baseline {a.shape}"); failures += 1; continue
        rms = float(np.sqrt(((a - b) ** 2).mean())) / 255
        if rms > 0.02:
            print(f"FAIL {f}: rms diff {rms:.3%} > 2%"); failures += 1
    print(f"panelshots: {'%d failure(s)' % failures if failures else 'all match baseline'}")
    return 1 if failures else 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))
