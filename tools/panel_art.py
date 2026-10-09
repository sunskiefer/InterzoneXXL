#!/usr/bin/env python3
"""Draw InterzoneXXL's skin from Interzone's own artwork, over the skin mpc-vst-plugins built from layout.conf.

    python3 tools/panel_art.py "<skin dir>/Plugin Skins"

For the Interzone pages (tools/panel.py):
  - each page's background (sh_bg_<t>.png) becomes its piece(s) of Valley's dark panel (InterzonePanelDark.svg),
    with the module's widgets drawn at their defaults;
  - every control gets its own images, drawn the way VCV Rack draws the module's widget, over exactly the part of the
    panel it covers: Valley's sliders (a filmstrip of the handle along its travel), Valley's Rogan knobs (turning
    between the background and highlight layers, VCV's sweep or the module's detents), VCV's CKSS and CKSSThree
    switches, and the Man. button with its red light;
  - the control's definition in TUI.json is replaced by one of its own: those images, the widget's exact bounds, and
    its live value text above it (sliders and small knobs; switches and detent knobs show their value on the panel).
On the VOICE tab it redraws the knob filmstrip (r=44) with Valley's orange Rogan knob and draws the Panic button. Images the new definitions no longer use are deleted. Needs cairosvg and Pillow.

Filmstrips follow mpc-vst-plugins' device-checked layouts (docs/NOTES.md): a knob is 128 square frames with
numFrames 127; a slider is frames of its own w x h, numFrames = their count, at most 12288 px tall. A switch, the
stepped slider and a detent knob are such slider strips with one frame per position.
"""
import copy
import io
import json
import math
import os
import re
import sys
import xml.etree.ElementTree as ET

import cairosvg
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import layout  # noqa: E402
import panel  # noqa: E402

ROOT = panel.ROOT
SS = 3                     # draw this many times larger, then scale down
MAX_STRIP = 12288          # tallest slider filmstrip (px) seen drawing right (mpc-vst-plugins shadow_skin.py)
KNOB_FRAMES = 128
FONT = os.path.join(ROOT, "art", "fonts", "TitilliumWeb-SemiBold.ttf")
VALUE_COLOUR = "ffcfcfcf"


# ------------------------------------------------------------------------------------------------ drawing
_svg_cache = {}


def svg_image(path, w, h):
    """An SVG drawn at w x h pixels (RGBA)."""
    w, h = max(1, int(round(w))), max(1, int(round(h)))
    key = (path, w, h)
    if key not in _svg_cache:
        png = cairosvg.svg2png(url=path, output_width=w, output_height=h)
        _svg_cache[key] = Image.open(io.BytesIO(png)).convert("RGBA")
    return _svg_cache[key]


def panel_piece(piece, k):
    """The piece's panel rectangle drawn at k x SS px per panel px, masks painted over."""
    x0, y0, x1, y1 = piece["rect"]
    ET.register_namespace("", "http://www.w3.org/2000/svg")
    tree = ET.parse(panel.PANEL_SVG)
    root = tree.getroot()
    root.set("viewBox", "%g %g %g %g" % (x0, y0, x1 - x0, y1 - y0))
    wpx, hpx = int(round((x1 - x0) * k * SS)), int(round((y1 - y0) * k * SS))
    root.set("width", str(wpx))
    root.set("height", str(hpx))
    png = cairosvg.svg2png(bytestring=ET.tostring(root), output_width=wpx, output_height=hpx)
    img = Image.new("RGBA", (wpx, hpx), panel.PANEL_RGB + (255,))
    img.alpha_composite(Image.open(io.BytesIO(png)).convert("RGBA"))
    d = ImageDraw.Draw(img)
    for poly in piece["masks"]:
        d.polygon([((x - x0) * k * SS, (y - y0) * k * SS) for x, y in poly], fill=panel.PANEL_RGB + (255,))
    return img


def paste(img, layer, x, y):
    """Composite layer at (x, y) (floats, rounded), clipped to img."""
    xi, yi = int(round(x)), int(round(y))
    # PIL's alpha_composite needs the layer inside the image: clip it
    sx0, sy0 = max(0, -xi), max(0, -yi)
    sx1, sy1 = min(layer.size[0], img.size[0] - xi), min(layer.size[1], img.size[1] - yi)
    if sx1 <= sx0 or sy1 <= sy0:
        return
    img.alpha_composite(layer.crop((sx0, sy0, sx1, sy1)), (xi + sx0, yi + sy0))


def art(*parts):
    return os.path.join(ROOT, "art", *parts)


class PageCanvas:
    """One page piece at SS x resolution: the panel, with what stays still drawn on it."""

    def __init__(self, page, piece):
        self.page, self.piece, self.k = page, piece, page["k"]
        self.img = panel_piece(piece, self.k)

    def px(self, x, y):
        """Panel point -> pixel of this canvas."""
        return ((x - self.piece["rect"][0]) * self.k * SS, (y - self.piece["rect"][1]) * self.k * SS)

    def scale(self, v):
        return v * self.k * SS


def slider_bg(canvas, pos):
    m = panel.SLIDER["margin"]
    bw, bh = panel.SLIDER["bg_size"]
    x, y = canvas.px(pos[0] + m[0], pos[1] + m[1])
    paste(canvas.img, svg_image(art("valley", panel.SLIDER["bg"]), canvas.scale(bw), canvas.scale(bh)), x, y)


def draw_widget(canvas, img, origin, c, value):
    """Draw control c's moving parts at `value` (0..1, or an option index for switches) onto img, whose top-left is
    canvas pixel `origin`."""
    pos = c["pos"]
    ox, oy = origin

    def at(x, y):
        X, Y = canvas.px(x, y)
        return X - ox, Y - oy

    w = c["widget"]
    if w in ("slider", "step"):
        m = panel.SLIDER["margin"]
        hw, hh = panel.SLIDER["handle_size"]
        top, bottom = panel.SLIDER_TRAVEL[w]
        hx = pos[0] + hw * 0.45 + m[0]
        hy = pos[1] + bottom + m[1] + (top - bottom) * value      # Rack: lerp(minHandlePos, maxHandlePos, value)
        x, y = at(hx, hy)
        paste(img, svg_image(art("valley", panel.SLIDER_HANDLE[c["art"]]), canvas.scale(hw), canvas.scale(hh)), x, y)
    elif w == "knob":
        bg, knob, fg, size = panel.KNOB[c["art"][0]]
        sweep = c["art"][1] if c["art"][1] is not None else panel.ROGAN_SWEEP
        s = canvas.scale(size)
        x, y = at(pos[0], pos[1])
        paste(img, svg_image(art("valley", bg), s, s), x, y)
        angle = -sweep + 2.0 * sweep * value
        layer = svg_image(art("valley", knob), s, s).rotate(-math.degrees(angle), resample=Image.BICUBIC)
        paste(img, layer, x, y)
        paste(img, svg_image(art("valley", fg), s, s), x, y)
    elif w in ("ckss", "ckss3"):
        spec = panel.CKSS if w == "ckss" else panel.CKSS3
        sw, sh = spec["size"]
        x, y = at(pos[0], pos[1])
        paste(img, svg_image(art("vcv", spec["frames"][int(value)]), canvas.scale(sw), canvas.scale(sh)), x, y)
    elif w == "led":
        spec = panel.LED_BUTTON
        bw, bh = spec["size"]
        x, y = at(pos[0], pos[1])
        paste(img, svg_image(art("valley", spec["svg"]), canvas.scale(bw), canvas.scale(bh)), x, y)
        if value:
            lx, ly = at(pos[0] + spec["light_at"][0], pos[1] + spec["light_at"][1])
            d = canvas.scale(spec["light_d"])
            glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
            gd = ImageDraw.Draw(glow)
            r, g, b = spec["light_rgb"]
            cx, cy = lx + d / 2, ly + d / 2
            for i in range(8, 0, -1):   # Rack's halo: a soft disc around the light
                rr = d / 2 * (1.0 + 0.12 * i)
                gd.ellipse((cx - rr, cy - rr, cx + rr, cy + rr), fill=(r, g, b, int(18 + 4 * (8 - i))))
            gd.ellipse((lx, ly, lx + d, ly + d), fill=(r, g, b, 255))
            img.alpha_composite(glow)
    else:
        raise SystemExit("panel_art: unknown widget %s" % w)


def control_frames(canvas, c):
    """The control's images: a list of frames (PIL RGB, the control box's size)."""
    x, y, w, h = c["box"]
    ox, oy = (x - c["piece"]["at"][0]) * SS, (y - c["piece"]["at"][1]) * SS
    base = canvas.img.crop((int(ox), int(oy), int(ox) + w * SS, int(oy) + h * SS))
    frames = []
    for v in c["values"]:
        cell = base.copy()
        draw_widget(canvas, cell, (ox, oy), c, v)
        frames.append(cell.resize((w, h), Image.LANCZOS).convert("RGB"))
    return frames


def strip(frames):
    w, h = frames[0].size
    out = Image.new("RGB", (w, h * len(frames)))
    for i, f in enumerate(frames):
        out.paste(f, (0, i * h))
    return out


# ------------------------------------------------------------------------------------------------ controls
def option_count(params, key):
    p = params[key]
    return len(p["options"]) if p.get("options") else 0


def plan_controls(params):
    """Each Interzone control with what its images show: `values` per frame and the TUI kind."""
    out = []
    for c in panel.placements():
        c = dict(c)
        c["pos"] = c["panel"][0] - panel.widget_box(c["widget"], c["art"])[0], \
            c["panel"][1] - panel.widget_box(c["widget"], c["art"])[1]
        n = option_count(params, c["key"])
        w, h = c["box"][2], c["box"][3]
        if c["widget"] in ("ckss", "led"):
            c["tui"], c["values"] = "toggle", [0, 1]
        elif c["widget"] == "ckss3":
            c["tui"], c["values"] = "strip", [0, 1, 2]
        elif c["widget"] == "step":
            c["tui"], c["values"] = "strip", [i / float(n - 1) for i in range(n)]
        elif c["widget"] == "knob" and c["art"][1] is not None:   # detents: one frame per option
            c["tui"], c["values"] = "strip", [i / float(n - 1) for i in range(n)]
        elif c["widget"] == "knob":
            c["tui"], c["values"] = "knob", [i / float(KNOB_FRAMES - 1) for i in range(KNOB_FRAMES)]
        else:
            nfr = max(2, min(128, MAX_STRIP // h))
            c["tui"], c["values"] = "strip", [i / float(nfr - 1) for i in range(nfr)]
        c["show_value"] = panel.shows_value(c)
        if c["tui"] == "knob" and w != h:
            raise SystemExit("panel_art: knob %s is not square" % c["key"])
        if c["tui"] == "strip" and h * len(c["values"]) > MAX_STRIP:
            raise SystemExit("panel_art: %s: a %d px strip is over %d px" % (c["key"], h * len(c["values"]), MAX_STRIP))
        out.append(c)
    return out


# ------------------------------------------------------------------------------------------------ TUI.json
def defs_by_key(tui):
    return {d["key"]: d for d in tui["pageData"]["componentDefinitions"]["localComponentDefinitions"]}


def child_param(child):
    for m in child.get("handle remapping", {}).get("map", []):
        if m.get("key") == "Data" and str(m.get("value", "")).startswith("Parameter "):
            return int(m["value"].split()[1])
    return None


def bounds_str(x, y, w, h):
    return "%d %d %d %d" % (x, y, w, h)


def set_bounds(comp, x, y, w, h):
    comp["bounds"]["bounds"] = bounds_str(x, y, w, h)


def make_def(template, c, image_names):
    """A definition of control c's own from the framework's (template): its images, bounds and value text."""
    d = copy.deepcopy(template)
    d["key"] = "iz_" + c["key"]
    v = d["value"]
    w, h = c["box"][2], c["box"][3]
    vh = panel.VALUE_H if c["show_value"] else 0
    cw = c["value_w"]
    xo = (cw - w) // 2
    comps = []
    for comp in v["componentsData"]:
        cd = comp["componentData"]
        if cd["type"] == "Focus":
            set_bounds(comp, 0, 0, cw, h + vh)
        elif cd["type"] == "Knob":
            cd["data"]["filmStrip"] = image_names[0]
            cd["data"]["numFrames"] = len(c["values"]) - (1 if c["tui"] == "knob" else 0)
            cd["data"]["dragOrientation"] = "Vertical"
            set_bounds(comp, xo, vh, w, h)
        elif cd["type"] == "Button":
            cd["data"]["offImage"], cd["data"]["onImage"] = image_names
            set_bounds(comp, 0, 0, w, h)
        elif cd["type"] == "Label":
            if cd["name"] != "Value" or not vh:
                continue                                  # no name labels; value text only where wanted
            cd["data"]["textStyle"]["colour"] = VALUE_COLOUR
            cd["data"]["textStyle"]["font"]["height"] = float(panel.VALUE_FONT)
            set_bounds(comp, 0, 0, cw, vh)
        else:
            raise SystemExit("panel_art: %s: unexpected %s in the framework's definition" % (c["key"], cd["type"]))
        comps.append(comp)
    v["componentsData"] = comps
    return d, (c["box"][0] - xo, c["box"][1] - vh, cw, h + vh)


def rewrite_tui(skin, controls, params_order):
    path = os.path.join(skin, "TUI.json")
    tui = json.load(open(path))
    defs = defs_by_key(tui)
    by_index = {params_order.index(c["key"]): c for c in controls}
    new_defs = []
    done = set()
    for d in list(defs.values()):
        for child in d["value"].get("componentsData", []):
            i = child_param(child)
            if i is None or i not in by_index:
                continue
            c = by_index[i]
            template = defs[child["componentData"]["type"]]
            names = c["images"]
            nd, rect = make_def(template, c, names)
            new_defs.append(nd)
            child["componentData"]["type"] = nd["key"]
            set_bounds(child, *rect)
            done.add(c["key"])
    missing = set(c["key"] for c in controls) - done
    if missing:
        raise SystemExit("panel_art: not found in TUI.json: %s" % ", ".join(sorted(missing)))
    tui["pageData"]["componentDefinitions"]["localComponentDefinitions"] += new_defs
    # drop the framework definitions nothing places any more
    used = set()
    for d in tui["pageData"]["componentDefinitions"]["localComponentDefinitions"]:
        for child in d["value"].get("componentsData", []):
            used.add(child["componentData"]["type"])
    for tab in tui["pageData"]["tabs"]:
        used.add(tab["componentName"])
    tui["pageData"]["componentDefinitions"]["localComponentDefinitions"] = [
        d for d in tui["pageData"]["componentDefinitions"]["localComponentDefinitions"] if d["key"] in used]
    json.dump(tui, open(path, "w"), indent=1)
    return tui


def check_overlaps(tui):
    """No two of the Interzone controls of a page share touch area (MPC gives a touch to one of them only)."""
    defs = defs_by_key(tui)
    bad = []
    for tab in tui["pageData"]["tabs"]:
        kids = [c for c in defs[tab["componentName"]]["value"]["componentsData"]
                if child_param(c) is not None and c["componentData"]["type"].startswith("iz_")]
        rects = [(c["componentData"]["name"] or str(child_param(c)), [int(v) for v in c["bounds"]["bounds"].split()])
                 for c in kids]
        for i in range(len(rects)):
            for j in range(i + 1, len(rects)):
                (a, (ax, ay, aw, ah)), (b, (bx, by, bw, bh)) = rects[i], rects[j]
                if ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah:
                    bad.append("%s: %s / %s" % (tab["tabName"], a, b))
    if bad:
        raise SystemExit("panel_art: overlapping controls: " + "; ".join(bad))


def prune_images(skin, tui):
    text = json.dumps(tui)
    removed = 0
    for name in os.listdir(skin):
        if name.endswith(".png") and ('"%s"' % name) not in text:
            os.remove(os.path.join(skin, name))
            removed += 1
    return removed


# ------------------------------------------------------------------------------------------------ VOICE page
VOICE_RADIUS = 44   # layout.py: the VOICE tab's knobs


def voice_knobs(skin):
    """The VOICE tab's knob filmstrips (r=44): Valley's orange Rogan, as on the module (128 square frames)."""
    bg, knob, fg, _ = panel.KNOB["med_orange"]
    n = 0
    for name in sorted(os.listdir(skin)):
        m = re.match(r"sh_knob_r(\d+)\.png$", name)
        if not m or int(m.group(1)) != VOICE_RADIUS:
            continue
        path = os.path.join(skin, name)
        frame = Image.open(path).size[0]
        size = (frame - 2) * SS
        b, kn, f = svg_image(art("valley", bg), size, size), svg_image(art("valley", knob), size, size), \
            svg_image(art("valley", fg), size, size)
        out = Image.new("RGB", (frame, frame * KNOB_FRAMES), panel.PANEL_RGB)
        for i in range(KNOB_FRAMES):
            angle = -panel.ROGAN_SWEEP + 2.0 * panel.ROGAN_SWEEP * i / (KNOB_FRAMES - 1)
            cell = Image.new("RGBA", (size, size), panel.PANEL_RGB + (255,))
            cell.alpha_composite(b)
            cell.alpha_composite(kn.rotate(-math.degrees(angle), resample=Image.BICUBIC))
            cell.alpha_composite(f)
            small = cell.resize((frame - 2, frame - 2), Image.LANCZOS).convert("RGB")
            tile = Image.new("RGB", (frame, frame), panel.PANEL_RGB)
            tile.paste(small, (1, 1))
            out.paste(tile, (0, i * frame))
        out.save(path, optimize=True)
        n += 1
    return n


def section_box(d, x, y, w, h, title, font):
    """A section frame as on the panel: a white rule, the title in a white tab across the top rule."""
    lw = 3
    d.rectangle((x, y, x + w, y + h), outline=(255, 255, 255), width=lw)
    tw = d.textlength(title, font=font)
    pad = 18
    tx0 = x + (w - tw) / 2 - pad
    d.rectangle((tx0, y - 22, tx0 + tw + 2 * pad, y + 22), fill=(240, 240, 240))
    d.text((x + w / 2, y), title, font=font, fill=(30, 30, 30), anchor="mm")


PANIC_BOX = (1100 - 85, 530 - 32 - panel.Y_OFF, 170, 64)   # the VOICE page's Panic button (layout.py)


def voice_panic(skin, tui):
    """Panic as a big red button with Titillium lettering (the framework's is sized to its bitmap label)."""
    x, y, w, h = PANIC_BOX
    font = ImageFont.truetype(FONT, 30)
    found = False
    for d in tui["pageData"]["componentDefinitions"]["localComponentDefinitions"]:
        comps = d["value"].get("componentsData", [])
        for comp in comps:
            cd = comp["componentData"]
            if cd["type"] == "Button" and "panic" in cd["data"].get("onImage", ""):
                for name, fill in ((cd["data"]["offImage"], (210, 58, 42)), (cd["data"]["onImage"], (240, 96, 80))):
                    img = Image.new("RGB", (w, h), panel.PANEL_RGB)
                    dr = ImageDraw.Draw(img)
                    dr.rounded_rectangle((0, 0, w - 1, h - 1), radius=8, fill=fill)
                    dr.text((w / 2, h / 2), "PANIC", font=font, fill=(255, 255, 255), anchor="mm")
                    img.save(os.path.join(skin, name), optimize=True)
                for c2 in comps:
                    set_bounds(c2, 0, 0, w, h)
                found = d["key"]
    if not found:
        raise SystemExit("panel_art: the Panic button is not in TUI.json")
    for d in tui["pageData"]["componentDefinitions"]["localComponentDefinitions"]:
        for child in d["value"].get("componentsData", []):
            if child["componentData"]["type"] == found:
                set_bounds(child, x, y, w, h)
    json.dump(tui, open(os.path.join(skin, "TUI.json"), "w"), indent=1)


# ------------------------------------------------------------------------------------------------ main
def main():
    skin = sys.argv[1]
    pj = json.load(open(os.path.join(ROOT, "params.json")))["params"]
    params = {p["key"]: p for p in pj}
    order = [p["key"] for p in pj]
    controls = plan_controls(params)
    for c in controls:
        c["value_w"] = panel.value_width(c, controls)
    tabs = [t["tabName"] for t in json.load(open(os.path.join(skin, "TUI.json")))["pageData"]["tabs"]]
    count = 0
    for t, page in enumerate(panel.PAGES):
        tab_index = tabs.index(page["tab"])
        bg = Image.new("RGB", (panel.W, panel.H), panel.PANEL_RGB)
        for pc in page["pieces"]:
            canvas = PageCanvas(page, pc)
            mine = [c for c in controls if c["page"] == t and c["piece"] is pc]
            for c in mine:
                if c["widget"] in ("slider", "step"):
                    slider_bg(canvas, c["pos"])
            # every control's frames over the panel as it is now (the slider slots drawn, no widget on top)
            for c in mine:
                frames = control_frames(canvas, c)
                if c["tui"] == "toggle":
                    names = ["iz_%s_off.png" % c["key"], "iz_%s_on.png" % c["key"]]
                    frames[0].save(os.path.join(skin, names[0]), optimize=True)
                    frames[1].save(os.path.join(skin, names[1]), optimize=True)
                else:
                    names = ["iz_%s.png" % c["key"]]
                    strip(frames).save(os.path.join(skin, names[0]), optimize=True)
                c["images"] = names
                count += 1
            # the page background: the panel with the widgets at their defaults
            for c in mine:
                p = params[c["key"]]
                if p.get("options"):
                    default = p["default"]
                    v = default if c["widget"] in ("ckss", "ckss3", "led") else default / float(len(p["options"]) - 1)
                else:
                    v = (p["default"] - p["min"]) / float(p["max"] - p["min"])
                draw_widget(canvas, canvas.img, (0, 0), c, v)
            small = canvas.img.resize((int(round(canvas.img.size[0] / SS)), int(round(canvas.img.size[1] / SS))),
                                      Image.LANCZOS).convert("RGB")
            bx, by = int(pc["at"][0]), int(pc["at"][1])
            bg.paste(small.crop((0, 0, min(small.size[0], panel.W - bx), min(small.size[1], panel.H - by))), (bx, by))
        bg.save(os.path.join(skin, "sh_bg_%d.png" % tab_index), optimize=True)
    tui = rewrite_tui(skin, controls, order)
    n_knobs = voice_knobs(skin)
    voice_panic(skin, tui)
    check_overlaps(tui)
    removed = prune_images(skin, tui)
    print("panel_art: %d controls drawn on %d Interzone pages, %d VOICE knob strip(s), %d unused image(s) removed"
          % (count, len(panel.PAGES), n_knobs, removed))


if __name__ == "__main__":
    main()
