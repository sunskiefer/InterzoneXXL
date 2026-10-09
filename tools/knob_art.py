#!/usr/bin/env python3
"""Replace the skin's drawn knob filmstrips with the original modules' knobs (PlateauXXL's tool; InterzoneXXL adds the
orange small Rogan at r=35 and accepts a skin that uses only some of the radii).

    python3 tools/knob_art.py "<skin dir>/Plugin Skins"

Keep every radius at 58 or less: the filmstrip is 128 frames of 2R+10 px, and r=59 makes it exactly 16384 px tall,
which MPC draws misaligned (the knob jumped up and down as it turned, seen on a Force, 2026-10-08).

gen_vst.py writes one filmstrip per knob radius (sh_knob_r<R>.png: 128 square frames of 2R+10 px, stacked down,
minimum first). layout.conf gives each kind of knob its own radius, and this redraws each strip from art/:
a static background layer, the knob rotated over its VCV range, and a static highlight on top, on the colour of
the page the knob sits on. Needs cairosvg and Pillow.
"""
import io
import math
import os
import sys

import cairosvg
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
ART = os.path.join(ROOT, "art")
FRAMES = 128
SUPER = 4                     # render this many times larger, then scale down (smooth edges)
ROGAN = 0.83 * math.pi        # componentlibrary Rogan and RoundKnob (Bogaudio's BGKnob): -0.83 pi .. +0.83 pi

VALLEY = (40, 40, 40)         # Plateau's dark panel, #282828
BOGAUDIO = (221, 221, 221)    # Bogaudio's light panel, #dddddd
MUTABLE = (230, 230, 230)     # the Mutable panels' grey, #e6e6e6

# radius in layout.conf -> (background layer or None, knob, highlight or None, page colour, sweep, size share)
KNOBS = {
    # Plateau (Valley): Med knobs on the main page, Small ones for the CV attenuverters and the sequencers
    58: ("valley/Rogan1PSMed-bg", "valley/Rogan1PSBlueMed", "valley/Rogan1PSBlueMed-fg", VALLEY, ROGAN, 1.0),
    47: ("valley/Rogan1PSMed-bg", "valley/Rogan1PSGreenMed", "valley/Rogan1PSGreenMed-fg", VALLEY, ROGAN, 1.0),
    46: ("valley/Rogan1PSMed-bg", "valley/Rogan1PSRedMed", "valley/Rogan1PSRedMed-fg", VALLEY, ROGAN, 1.0),
    44: ("valley/Rogan1PSMedSmall-bg", "valley/Rogan1PSWhiteMedSmall", "valley/Rogan1PSWhiteMedSmall-fg", VALLEY, ROGAN, 1.0),
    36: ("valley/Rogan1PSSmall-bg", "valley/Rogan1PSWhiteSmall", "valley/Rogan1PSWhiteSmall-fg", VALLEY, ROGAN, 1.0),
    35: ("valley/v2/Small/Rogan1PSSmall-bg", "valley/v2/Small/Rogan1PSOrangeSmall", "valley/v2/Small/Rogan1PSOrangeSmall-fg",
         VALLEY, ROGAN, 1.0),   # InterzoneXXL: the CV IN amounts, Interzone's orange
    37: ("valley/Rogan1PSSmall-bg", "valley/Rogan1PSGreenSmall", "valley/Rogan1PSGreenSmall-fg", VALLEY, ROGAN, 1.0),
    38: ("valley/Rogan1PSSmall-bg", "valley/Rogan1PSBlueSmall", "valley/Rogan1PSBlueSmall-fg", VALLEY, ROGAN, 1.0),
    39: ("valley/Rogan1PSSmall-bg", "valley/Rogan1PSRedSmall", "valley/Rogan1PSRedSmall-fg", VALLEY, ROGAN, 1.0),
    # Bogaudio LFO: Knob68 (frequency), Knob26 (sample, pulse width), Knob16 (smooth, offset, scale)
    57: (None, "bogaudio/knob_68px", None, BOGAUDIO, ROGAN, 1.0),
    42: (None, "bogaudio/knob_26px", None, BOGAUDIO, ROGAN, 1.0),
    34: (None, "bogaudio/knob_16px", None, BOGAUDIO, ROGAN, 1.0),
    # Tidal Modulator 2 / Random Sampler: VCV's Rogan 3PS, 2PS and 1PS white knobs
    56: ("vcv/Rogan3PS_bg", "vcv/Rogan3PSWhite", "vcv/Rogan3PSWhite_fg", MUTABLE, ROGAN, 1.0),
    48: ("vcv/Rogan2PS_bg", "vcv/Rogan2PSWhite", "vcv/Rogan2PSWhite_fg", MUTABLE, ROGAN, 1.0),
    40: ("vcv/Rogan1PS_bg", "vcv/Rogan1PSWhite", "vcv/Rogan1PSWhite_fg", MUTABLE, ROGAN, 1.0),
}


def render(name, px):
    png = cairosvg.svg2png(url=os.path.join(ART, name + ".svg"), output_width=px, output_height=px)
    return Image.open(io.BytesIO(png)).convert("RGBA")


def build(path, radius):
    old = Image.open(path)
    frame = old.size[0]
    if old.size[1] != frame * FRAMES:
        raise SystemExit("knob_art: %s: expected %d square frames" % (path, FRAMES))
    bg_name, knob_name, fg_name, panel, sweep, share = KNOBS[radius]
    size = int(round((frame - 2) * share)) * SUPER   # the knob fills the frame, 1 px margin
    bg = render(bg_name, size) if bg_name else None
    knob = render(knob_name, size)
    fg = render(fg_name, size) if fg_name else None
    out = Image.new("RGB", (frame, frame * FRAMES), panel)
    px = size // SUPER
    for i in range(FRAMES):
        angle = -sweep + 2.0 * sweep * i / (FRAMES - 1)
        cell = Image.new("RGBA", (size, size), panel + (255,))
        if bg:
            cell.alpha_composite(bg)
        # PIL rotates counter-clockwise for positive angles; a knob turns clockwise as the value rises
        cell.alpha_composite(knob.rotate(-math.degrees(angle), resample=Image.BICUBIC))
        if fg:
            cell.alpha_composite(fg)
        small = cell.resize((px, px), Image.LANCZOS)
        tile = Image.new("RGB", (frame, frame), panel)
        tile.paste(small.convert("RGB"), ((frame - px) // 2, (frame - px) // 2))
        out.paste(tile, (0, i * frame))
    out.save(path, optimize=True)
    print("knob_art: %s -> %s, %d px frames" % (os.path.basename(path), knob_name, frame))


def main():
    skin = sys.argv[1]
    done = set()
    for name in sorted(os.listdir(skin)):
        if name.startswith("sh_knob_r") and name.endswith(".png") and name[9:-4].isdigit():
            r = int(name[9:-4])
            if r not in KNOBS:
                raise SystemExit("knob_art: no knob art for radius %d (layout.conf radii: %s)" % (r, sorted(KNOBS)))
            build(os.path.join(skin, name), r)
            done.add(r)
    if not done:
        raise SystemExit("knob_art: no knob filmstrips in %s" % skin)


if __name__ == "__main__":
    main()
