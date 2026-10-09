#!/usr/bin/env python3
"""Interzone's panel, as InterzoneXXL draws it: the single source of where every control sits.

Used by tools/layout.py (writes the Interzone pages of layout.conf) and tools/panel_art.py (draws the skin).

Positions are the module's own (InterzoneWidget in third_party/valley/src/Interzone/Interzone.hpp: each widget's
top-left corner in panel pixels, the panel being 480 x 380), and each control is the module's own widget
(Interzone.cpp's addParam calls): Valley's sliders (ValleyComponents.hpp: OrangeSlider, BlueSlider, GreenSlider,
RedSlider, YellowStepSlider), Valley's Rogan knobs and VCV's CKSS / CKSSThree switches.

A page shows one piece of the panel (two side by side on MIXER / ENV, since the Force screen is wide), scaled by
`k` screen pixels per panel pixel. Screen coordinates are Force Shadow pixels: the plugin area is x 0..1280,
y 86..714 (skin y = screen y - 86).
"""
import math
import os

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
VALLEY = os.path.join(ROOT, "art", "valley")
VCV = os.path.join(ROOT, "art", "vcv")
Y_OFF = 86
W, H = 1280, 628

PANEL_SVG = os.path.join(VALLEY, "InterzonePanelDark.svg")
PANEL_W, PANEL_H = 480.0, 380.0
PANEL_RGB = (40, 40, 40)          # the dark panel, #282828

# ---- widgets (sizes in panel pixels) --------------------------------------------------------------------------------
# ValleySlider: the background (valleySliderBackground.svg, 22.8 x 87.5) at margin (-1, -0.55) from the widget's
# corner; the handle (12 x 29.9) moves from minHandlePos (bottom) to maxHandlePos (top), both + margin.
SLIDER = {"bg": "valleySliderBackground.svg", "bg_size": (22.8, 87.5), "margin": (-1.0, -0.55),
          "handle_size": (12.0, 29.9)}
SLIDER_HANDLE = {"orange": "sliderOrange.svg", "blue": "sliderBlue.svg", "green": "sliderGreen.svg",
                 "red": "sliderRed.svg", "yellow": "sliderYellow.svg"}
# handle travel (y of the handle's top, before the margin): ValleySlider 1.5 (top) .. 61.5 (bottom);
# YellowStepSlider 0.5 .. 61.5. x = handle width x 0.45.
SLIDER_TRAVEL = {"slider": (1.5, 61.5), "step": (0.5, 61.5)}

# Rogan knobs: background, the turning knob, a fixed highlight; VCV's Rogan sweep is -0.83 pi .. 0.83 pi.
KNOB = {
    "med_orange": ("v2/Med/Rogan1PSMed-bg.svg", "v2/Med/Rogan1PSOrangeMed.svg", "v2/Med/Rogan1PSOrangeMed-fg.svg",
                   31.746872),
    "small_orange": ("v2/Small/Rogan1PSSmall-bg.svg", "v2/Small/Rogan1PSOrangeSmall.svg",
                     "v2/Small/Rogan1PSOrangeSmall-fg.svg", 19.799255),
    "small_blue": ("v2/Small/Rogan1PSSmall-bg.svg", "v2/Small/Rogan1PSBlueSmall.svg",
                   "v2/Small/Rogan1PSBlueSmall-fg.svg", 19.799255),
}
ROGAN_SWEEP = 0.83 * math.pi
CKSS = {"frames": ["CKSS_0.svg", "CKSS_1.svg"], "size": (14.0, 20.64111)}
CKSS3 = {"frames": ["CKSSThree_0.svg", "CKSSThree_1.svg", "CKSSThree_2.svg"], "size": (13.457, 28.34766)}
LED_BUTTON = {"svg": "LightLEDButton80.svg", "size": (14.252, 14.252), "light_at": (2.5, 2.5),
              "light_d": 3.0 * 75.0 / 25.4, "light_rgb": (0xED, 0x2C, 0x24)}   # MediumLight<RedLight>, SCHEME_RED


# ---- pages -----------------------------------------------------------------------------------------------------------
# A piece: the panel rectangle (x0, y0, x1, y1) it shows, where its top-left lands on the page (sx, sy), and masks:
# panel-coordinate polygons painted over in the panel colour (the neighbouring sections that fall inside the rectangle).
def _pages():
    pages = []
    # VCO: the VCO section fills the page at 5 screen px per panel px
    k = 5.0
    pages.append({"tab": "VCO", "k": k, "pieces": [
        {"rect": (6.0, 25.6, 6.0 + W / k, 25.6 + H / k), "at": (0, 0), "masks": [
            [(0, 151.2), (480, 151.2), (480, 380), (0, 380)]]}]})
    # FILTER / LFO: the two sections side by side as on the panel; the Envelope frame's corner and the VCO frame
    # above are masked
    k = W / 288.0
    y0 = 151.5 - (H / k - 129.0) / 2.0
    pages.append({"tab": "FILTER / LFO", "k": k, "pieces": [
        {"rect": (6.5, y0, 6.5 + W / k, y0 + H / k), "at": (0, 0), "masks": [
            [(0, 0), (480, 0), (480, 151.4), (0, 151.4)],
            [(0, 281.0), (480, 281.0), (480, 380), (0, 380)],
            [(295.5, 140), (480, 140), (480, 300), (259.75, 300), (295.5, 216.25)]]}]})
    # MIXER / ENV: the Mixer section, then the Envelope and VCA sections, side by side
    gap = 6.0
    mix = (263.5, 25.6, 474.75, 151.5)
    env = (269.75, 151.0, 474.75, 281.0)
    k = W / ((mix[2] - mix[0]) + gap + (env[2] - env[0]))
    top = float(int((H - max(mix[3] - mix[1], env[3] - env[1]) * k) / 2.0))
    pages.append({"tab": "MIXER / ENV", "k": k, "pieces": [
        {"rect": mix, "at": (0, top), "masks": [[(0, 151.2), (480, 151.2), (480, 380), (0, 380)]]},
        {"rect": env, "at": (float(int((mix[2] - mix[0] + gap) * k)), top), "masks": [
            [(0, 0), (480, 0), (480, 151.3), (0, 151.3)],
            [(0, 281.0), (480, 281.0), (480, 380), (0, 380)],
            [(0, 140), (297.25, 140), (297.25, 216.25), (269.75 - 0.435 * 20.5, 300), (0, 300)]]}]})
    return pages


PAGES = _pages()

# ---- controls --------------------------------------------------------------------------------------------------------
# (key, widget, art, panel position): widget is slider / step (a stepped slider) / knob / ckss / ckss3 / led.
# For knobs `art` is (look, sweep in radians or None for Rogan's); for option knobs the detents spread over the sweep.
CONTROLS = {
    "VCO": [
        ("glide", "slider", "orange", (13.15, 43.6)),
        ("vco_mod", "slider", "orange", (34.0, 43.6)),
        ("vco_mod_pol", "ckss", None, (64.498, 51.172)),
        ("vco_mod_src", "ckss", None, (64.498, 99.173)),
        ("coarse_mode", "ckss", None, (88.489, 51.172)),
        ("octave", "knob", ("med_orange", 0.222222 * math.pi), (123.212, 64.674)),
        ("coarse", "knob", ("small_orange", None), (114.584, 113.266)),
        ("fine", "knob", ("small_orange", None), (144.584, 113.266)),
        ("width", "slider", "orange", (171.15, 43.6)),
        ("pwm", "slider", "orange", (191.95, 43.6)),
        ("pwm_pol", "ckss", None, (222.888, 51.172)),
        ("pwm_src", "ckss3", None, (222.742, 89.345)),
    ],
    "FILTER / LFO": [
        ("cutoff", "slider", "blue", (13.0, 172.5)),
        ("res", "slider", "blue", (34.0, 172.5)),
        ("hpf", "slider", "blue", (55.0, 172.5)),
        ("flt_env_pol", "ckss", None, (84.443, 184.085)),
        ("poles", "ckss", None, (84.443, 240.504)),
        ("flt_env", "slider", "orange", (108.15, 172.5)),
        ("flt_lfo", "slider", "orange", (129.25, 172.5)),
        ("flt_voct", "slider", "orange", (150.25, 172.5)),
        ("lfo_rate", "slider", "green", (186.25, 172.5)),
        ("lfo_wave", "knob", ("med_orange", 0.333333 * math.pi), (234.612, 194.074)),
        ("lfo_fine", "knob", ("small_orange", None), (214.862, 242.066)),
        ("lfo_slew", "knob", ("small_orange", None), (240.584, 242.066)),
    ],
    "MIXER / ENV": [
        ("saw", "slider", "green", (270.15, 43.6)),
        ("pulse", "slider", "green", (291.15, 43.6)),
        ("sub", "slider", "green", (312.15, 43.6)),
        ("sub_oct", "step", "yellow", (336.05, 43.6)),
        ("sub_wave", "ckss3", None, (373.3, 102.85)),
        ("noise_type", "ckss", None, (399.481, 56.632)),
        ("noise", "slider", "green", (426.15, 43.6)),
        ("ext", "slider", "green", (447.15, 43.6)),
        ("env_length", "ckss", None, (319.497, 180.663)),
        ("env_cycle", "ckss", None, (319.497, 240.665)),
        ("env_manual", "led", None, (292.844, 245.275)),
        ("attack", "slider", "red", (342.15, 172.5)),
        ("decay", "slider", "red", (363.15, 172.5)),
        ("sustain", "slider", "red", (384.15, 172.5)),
        ("release", "slider", "red", (405.15, 172.5)),
        ("vca_src", "ckss", None, (447.483, 181.663)),
    ],
}

# Q-Link order: the panel read left to right, a section at a time (bank 1 = Q-Links 1-8, bank 2 = 9-16).
QLINKS = {
    "VCO": ["glide", "vco_mod", "vco_mod_pol", "vco_mod_src", "coarse_mode", "octave", "coarse", "fine",
            "width", "pwm", "pwm_pol", "pwm_src"],
    "FILTER / LFO": ["cutoff", "res", "hpf", "flt_env_pol", "poles", "flt_env", "flt_lfo", "flt_voct",
                     "lfo_rate", "lfo_wave", "lfo_fine", "lfo_slew"],
    "MIXER / ENV": ["saw", "pulse", "sub", "sub_oct", "sub_wave", "noise_type", "noise", "ext",
                    "env_length", "env_cycle", "env_manual", "attack", "decay", "sustain", "release", "vca_src"],
}

VALUE_FONT = 19      # live value text above sliders and small knobs
VALUE_H = 22


def piece_for(page, pos):
    """The piece of `page` that shows panel point pos."""
    for pc in page["pieces"]:
        x0, y0, x1, y1 = pc["rect"]
        if x0 <= pos[0] <= x1 and y0 <= pos[1] <= y1:
            return pc
    raise SystemExit("panel: %r is on no piece of page %s" % (pos, page["tab"]))


def to_screen(page, pc, x, y):
    """Panel point -> page point (skin coordinates, y from the top of the plugin area)."""
    k = page["k"]
    return (pc["at"][0] + (x - pc["rect"][0]) * k, pc["at"][1] + (y - pc["rect"][1]) * k)


def slider_slot():
    """The part of a slider that moves, relative to the widget's corner: the panel's slot (about 3.6 .. 16.85 across,
    0.15 .. 86.4 down) and the handle's whole travel (4.4 .. 16.4 across, -0.05 .. 90.85 down, the handle hanging
    below the slot at the bottom), with a little margin. Each slider gets its own filmstrip of exactly this box, so
    whatever of the panel lies in it (a bracket's end, the slot's shading) is drawn into every frame."""
    return (3.2, -0.4, 14.0, 91.6)


def widget_box(widget, art):
    """(dx, dy, w, h) of the drawn widget relative to its panel position, in panel pixels."""
    if widget in ("slider", "step"):
        return slider_slot()
    if widget == "knob":
        size = KNOB[art[0]][3]
        return (0.0, 0.0, size, size)
    if widget == "ckss":
        return (0.0, 0.0) + CKSS["size"]
    if widget == "ckss3":
        return (0.0, 0.0) + CKSS3["size"]
    if widget == "led":
        return (0.0, 0.0) + LED_BUTTON["size"]
    raise SystemExit("panel: unknown widget %s" % widget)


def placements():
    """Every control as it lands on its page: dicts with key, widget, art, tab, page index, and the screen box
    (x, y, w, h) of the drawn widget in skin pixels (ints), plus the panel box."""
    out = []
    for t, page in enumerate(PAGES):
        for key, widget, art, pos in CONTROLS[page["tab"]]:
            pc = piece_for(page, pos)
            dx, dy, w, h = widget_box(widget, art)
            x0, y0 = to_screen(page, pc, pos[0] + dx, pos[1] + dy)
            x1, y1 = to_screen(page, pc, pos[0] + dx + w, pos[1] + dy + h)
            k = page["k"]
            box = (int(round(x0)), int(round(y0)), int(round(w * k)), int(round(h * k)))   # one size per widget kind
            out.append({"key": key, "widget": widget, "art": art, "tab": page["tab"], "page": t, "box": box,
                        "panel": (pos[0] + dx, pos[1] + dy, w, h), "k": page["k"], "piece": pc})
    return out


NO_VALUE = ("lfo_fine", "lfo_slew")   # no room above them under the Wave knob's label


def shows_value(c):
    """Sliders and the small knobs show their value above them; switches and detent knobs show it on the panel."""
    if c["key"] in NO_VALUE:
        return False
    return c["widget"] == "slider" or (c["widget"] == "knob" and c["art"][1] is None)


def value_width(c, placed):
    """Width of the control's touch box with its value text: up to 110 px, never reaching a neighbour's box (a
    neighbour with value text takes its half of the space between them)."""
    x, y, w, h = c["box"]
    if not shows_value(c):
        return w
    cx = x + w / 2.0
    width = 110.0
    for o in placed:
        if o is c or o["page"] != c["page"]:
            continue
        ox, oy, ow, oh = o["box"]
        if not (oy - VALUE_H < y + h and y - VALUE_H < oy + oh):   # not side by side
            continue
        dx = abs(ox + ow / 2.0 - cx)
        width = min(width, dx - 4 if shows_value(o) else 2 * (dx - ow / 2.0) - 4)
    return int(max(w, width))
