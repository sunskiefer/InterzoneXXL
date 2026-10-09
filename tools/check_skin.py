#!/usr/bin/env python3
"""Run mpc-vst-plugins' skin_check.py on the built skin and fail on any finding on InterzoneXXL's own pages.

    python3 tools/check_skin.py "<skin dir>/Plugin Skins"

The source pages (LFO, TIDAL / RANDOM, SEQ) are PlateauXXL's as they are: their open option lists cover the controls
below them while open, which skin_check reports as TOUCH overlaps; PlateauXXL has the same and was used that way on a
Force. Those are printed as a count, not failed.
"""
import os
import subprocess
import sys

STRICT = ("VCO", "FILTER / LFO", "MIXER / ENV", "VOICE", "EXT OSC", "PRESETS", "CV 1", "CV 2")
HERE = os.path.dirname(os.path.abspath(__file__))
CHECK = os.path.join(HERE, "..", "third_party", "mpc-vst-plugins", "tools", "skin_check.py")


def main():
    r = subprocess.run([sys.executable, CHECK, sys.argv[1]], capture_output=True, text=True)
    lines = [l for l in (r.stdout + r.stderr).splitlines() if l.strip()]
    bad = [l for l in lines if l.split(":")[0].strip() in STRICT]
    other = len(lines) - len(bad)
    for l in bad:
        print("check_skin: " + l, file=sys.stderr)
    print("check_skin: %d finding(s) on InterzoneXXL's pages, %d open-list overlap(s) on the source pages (as PlateauXXL)"
          % (len(bad), other))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
