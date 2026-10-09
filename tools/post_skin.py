#!/usr/bin/env python3
"""Repaint every page image of the built skin in the look of the module each page stands for.

    python3 tools/post_skin.py "<skin dir>/Plugin Skins" layout.conf

mpc-vst-plugins draws one background per tab (sh_bg_<t>.png) and, for widgets with banks=, an image per sub-page
(sh_mode_<t>_<m>.png) in one shared theme. This redraws each of those images from layout.conf instead, per tab:

  #@panel tab=N color=RRGGBB ink=RRGGBB          the tab's panel colour and lettering colour
  #@title tab=N x= y= label=".."                 a frame title (22 px, on the frame's rule), as on the main page
  #@text  tab=N cx= cy= label=".." [size=] [color=] [weight=light] [align=left|right] [italic=1] [font=serif]
          [spacing=px]
  #@box   tab=N x= y= w= h= fill=RRGGBB [outline=RRGGBB]
  #@line  tab=N x1= y1= x2= y2= [color=] [width=]
  #@dots  tab=N x1= y1= x2= y2=                  a dotted cable, as on the Mutable panels
  #@dotcurve tab=N x1= y1= x2= y2=               Marbles' dotted wave between t and X
  #@braid tab=N y=                               the Mutable panels' braided rule under the header

It also redraws the popup lists' option buttons (sh_po_<n>_on/off.png) with Titillium Web lettering.

Any of these, and frames and popups, can carry banks="A|B" like layout lines: then they are drawn only on the
images of those sub-pages. Frames, popup fields and the "opens a list" marker are redrawn in the tab's style
(Plateau: dark fields; Bogaudio: white fields; Mutable: dark fields with coloured text). Coordinates are Force Shadow
pixels (skin y = shadow y - 86). Fonts: Titillium Web (art/fonts) and DejaVu Serif for the Mutable italics.
"""
import json
import math
import os
import re
import shlex
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
FONT = os.path.join(ROOT, "art", "fonts", "TitilliumWeb-SemiBold.ttf")
SERIF_CANDIDATES = ["/usr/share/fonts/truetype/dejavu/DejaVuSerif-Italic.ttf",
                    "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf"]
Y_OFF = 86
W, H = 1280, 628


def rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def attrs(text):
    out = {}
    for tok in shlex.split(text):
        if "=" in tok:
            k, v = tok.split("=", 1)
            out[k] = v
    return out


def parse_layout(path):
    """-> list of tabs: {"name", "panel", "ink", "widgets": [(kind, attrs)], "decor": [(kind, attrs)]}"""
    tabs = []
    for raw in open(path):
        line = raw.strip()
        m = re.match(r"\[tab (.+)\]$", line)
        if m:
            tabs.append({"name": m.group(1), "panel": "282828", "ink": "ffffff", "widgets": [], "decor": []})
            continue
        if not tabs or not line:
            continue
        if line.startswith("#@"):
            kind, _, rest = line[2:].partition(" ")
            a = attrs(rest)
            t = tabs[int(a.get("tab", len(tabs) - 1))]
            if kind == "panel":
                t["panel"], t["ink"] = a["color"], a.get("ink", t["ink"])
            else:
                t["decor"].append((kind, a))
            continue
        if line.startswith("#"):
            continue
        kind, _, rest = line.partition(" ")
        if kind in ("frame", "popup"):
            tabs[-1]["widgets"].append((kind, attrs(rest)))
    return tabs


def banks(a):
    b = a.get("banks")
    return set(b.split("|")) if b else None


def page_images(skin):
    """-> {image file: (tab index, set of sub-page titles or None for the whole tab, (x, y, w, h))}"""
    p = json.load(open(os.path.join(skin, "TUI.json")))["pageData"]
    defs = {d["key"]: d["value"] for d in p["componentDefinitions"]["localComponentDefinitions"]}
    images = {}
    for tab in p["tabs"]:
        t, title = tab["fnKeyIndex"], tab["tabName"]
        for c in defs[tab["componentName"]]["componentsData"]:
            cd = c["componentData"]
            img = cd.get("data", {}).get("image", "") if cd["type"] == "Image" else ""
            if not (img.startswith("sh_bg_") or img.startswith("sh_mode_")):
                continue
            box = tuple(int(v) for v in c["bounds"]["bounds"].split())
            entry = images.setdefault(img, [t, set(), box, img.startswith("sh_bg_")])
            entry[1].add(title)
    return {k: (v[0], None if v[3] else v[1], v[2]) for k, v in images.items()}


class Painter:
    def __init__(self, tab):
        self.tab = tab
        self.panel, self.ink = rgb(tab["panel"]), rgb(tab["ink"])
        self.light = sum(self.panel) > 3 * 160
        self.serif = next((f for f in SERIF_CANDIDATES if os.path.exists(f)), FONT)

    def font(self, size, a):
        if a.get("font") == "serif":
            return ImageFont.truetype(self.serif, size)
        return ImageFont.truetype(FONT, size)

    def text(self, dr, a):
        size = int(float(a.get("size", 20)))
        font = self.font(size, a)
        label = a.get("label", "")
        spacing = int(a.get("spacing", 0))
        color = rgb(a["color"]) if "color" in a else self.ink
        if a.get("weight") == "light":   # Titillium SemiBold only: a lighter tone of the ink reads as a thin face
            color = tuple(int(c * 0.75 + p * 0.25) for c, p in zip(color, self.panel))
        widths = [dr.textbbox((0, 0), ch, font=font)[2] for ch in label]
        total = sum(widths) + spacing * max(0, len(label) - 1) if spacing else dr.textbbox((0, 0), label, font=font)[2]
        b = dr.textbbox((0, 0), label or " ", font=font)
        cx, cy = float(a["cx"]), float(a["cy"]) - Y_OFF
        align = a.get("align", "center")
        x = cx - total / 2 if align == "center" else (cx if align == "left" else cx - total)
        y = cy - (b[3] + b[1]) / 2
        if spacing:
            for ch, w in zip(label, widths):
                dr.text((x, y), ch, font=font, fill=color)
                x += w + spacing
        else:
            dr.text((x, y), label, font=font, fill=color)

    def frame(self, dr, a):
        x, y, w, h = (int(a[k]) for k in ("x", "y", "w", "h"))
        if w >= W and h >= H:   # a full-page frame only gives a sub-page its own image: nothing to draw
            return
        y -= Y_OFF
        dr.rectangle((x, y, x + w - 1, y + h - 1), outline=self.ink, width=1)
        dr.line((x + 18, y + 42, x + w - 18, y + 42), fill=self.ink, width=1)

    def title(self, dr, a):
        font = ImageFont.truetype(FONT, 22)
        b = dr.textbbox((0, 0), a["label"], font=font)
        dr.text((int(a["x"]) + 18 - b[0], int(a["y"]) - Y_OFF + 30 - b[3]), a["label"], font=font, fill=self.ink)

    def popup(self, dr, a):
        w, h = int(a["w"]), int(a["h"])
        x, y = int(a["cx"]) - w // 2, int(a["cy"]) - Y_OFF - h // 2
        accent = rgb(a.get("accent", "307dee"))
        if self.panel == (221, 221, 221):     # Bogaudio: white field, dark border and marker
            dr.rectangle((x, y, x + w - 1, y + h - 1), fill=(250, 250, 250), outline=(26, 26, 26), width=2)
            marker = (26, 26, 26)
        elif self.light:                      # Mutable: dark display field, coloured text and marker
            dr.rounded_rectangle((x, y, x + w - 1, y + h - 1), radius=6, fill=(33, 30, 30))
            marker = accent
        else:                                 # Plateau: dark field between two rules
            dr.rectangle((x, y, x + w - 1, y + h - 1), fill=(28, 28, 28))
            dr.line((x, y, x + w - 1, y), fill=(224, 224, 224), width=1)
            dr.line((x, y + h - 1, x + w - 1, y + h - 1), fill=(224, 224, 224), width=1)
            marker = accent if a.get("accent") else (48, 125, 238)
        mx, my = x + w - 22, y + h // 2
        dr.polygon([(mx - 8, my - 4), (mx + 8, my - 4), (mx, my + 5)], fill=marker)

    def box(self, dr, a):
        x, y, w, h = (int(a[k]) for k in ("x", "y", "w", "h"))
        y -= Y_OFF
        dr.rectangle((x, y, x + w - 1, y + h - 1), fill=rgb(a["fill"]),
                     outline=rgb(a["outline"]) if "outline" in a else None)

    def line(self, dr, a):
        dr.line((int(a["x1"]), int(a["y1"]) - Y_OFF, int(a["x2"]), int(a["y2"]) - Y_OFF),
                fill=rgb(a["color"]) if "color" in a else self.ink, width=int(a.get("width", 1)))

    def dots(self, dr, a, curve=False):
        x1, y1, x2, y2 = (float(a[k]) for k in ("x1", "y1", "x2", "y2"))
        y1 -= Y_OFF
        y2 -= Y_OFF
        n = max(2, int(math.hypot(x2 - x1, y2 - y1) / 7))
        for i in range(n + 1):
            f = i / float(n)
            x = x1 + (x2 - x1) * f
            y = y1 + (y2 - y1) * f + (math.sin(f * 2.0 * math.pi) * 14.0 if curve else 0.0)
            dr.ellipse((x - 1.6, y - 1.6, x + 1.6, y + 1.6), fill=self.ink)

    def braid(self, dr, a):
        y = int(a["y"]) - Y_OFF
        dark = tuple(max(0, c - 40) for c in self.panel)
        dr.rectangle((0, y - 7, W, y + 7), fill=tuple(max(0, c - 18) for c in self.panel))
        for x in range(0, W + 16, 16):   # a chain of small diamonds, after the Mutable panels' braided rule
            dr.polygon([(x, y - 5), (x + 8, y), (x, y + 5), (x - 8, y)], outline=dark)

    def paint(self, scope):
        """The whole page for one image: scope None = the tab's background, else the image's sub-pages."""
        im = Image.new("RGB", (W, H), self.panel)
        dr = ImageDraw.Draw(im)

        def shown(a):
            b = banks(a)
            if scope is None:
                return b is None
            return b is None or scope <= b

        for kind, a in self.tab["decor"]:
            if kind == "box" and shown(a):
                self.box(dr, a)
        for kind, a in self.tab["widgets"]:
            if kind == "frame" and shown(a):
                self.frame(dr, a)
        for kind, a in self.tab["decor"]:
            if not shown(a):
                continue
            if kind == "title":
                self.title(dr, a)
            elif kind == "text":
                self.text(dr, a)
            elif kind == "line":
                self.line(dr, a)
            elif kind == "dots":
                self.dots(dr, a)
            elif kind == "dotcurve":
                self.dots(dr, a, curve=True)
            elif kind == "braid":
                self.braid(dr, a)
        for kind, a in self.tab["widgets"]:
            if kind == "popup" and shown(a):
                self.popup(dr, a)
        return im


def restyle_popup_options(skin, params_path):
    """Popup list buttons: the generator draws their text in its bitmap font; redraw it in Titillium Web, the
    unpicked option light on dark, the picked one dark on white (the generator's own colours)."""
    options = {p["key"]: p.get("options") or [] for p in json.load(open(params_path))["params"]}
    p = json.load(open(os.path.join(skin, "TUI.json")))["pageData"]
    done = 0
    for d in p["componentDefinitions"]["localComponentDefinitions"]:
        m = re.match(r"shPopOpt_\d+_(.+)_(\d+)$", d["key"])
        if not m:
            continue
        key, i = m.group(1), int(m.group(2))
        label = options.get(key, [])[i] if i < len(options.get(key, [])) else None
        if label is None:
            continue
        data = d["value"]["componentsData"][0]["componentData"]["data"]
        for img, bg, fg in ((data["offImage"], (28, 28, 28), (230, 230, 230)), (data["onImage"], (255, 255, 255), (17, 17, 17))):
            path = os.path.join(skin, img)
            w, h = Image.open(path).size
            im = Image.new("RGB", (w, h), bg)
            dr = ImageDraw.Draw(im)
            font = ImageFont.truetype(FONT, max(12, min(22, int(h * 0.5))))
            b = dr.textbbox((0, 0), label, font=font)
            while b[2] - b[0] > w - 8 and font.size > 10:
                font = ImageFont.truetype(FONT, font.size - 1)
                b = dr.textbbox((0, 0), label, font=font)
            dr.text(((w - (b[2] - b[0])) / 2 - b[0], (h - (b[3] - b[1])) / 2 - b[1]), label, font=font, fill=fg)
            im.save(path, optimize=True)
            done += 1
    return done


def main():
    skin, layout = sys.argv[1], sys.argv[2]
    if os.path.isdir(os.path.join(skin, "Plugin Skins")):
        skin = os.path.join(skin, "Plugin Skins")
    tabs = parse_layout(layout)
    images = page_images(skin)
    if not images:
        raise SystemExit("post_skin: no page images in " + skin)
    for img, (t, scope, (x, y, w, h)) in sorted(images.items()):
        page = Painter(tabs[t]).paint(scope)
        page.crop((x, y, x + w, y + h)).save(os.path.join(skin, img), optimize=True)
    n = restyle_popup_options(skin, os.path.join(os.path.dirname(os.path.abspath(layout)), "params.json"))
    print("post_skin: %d page images repainted (%s), %d popup option images relettered"
          % (len(images), ", ".join(t["name"] for t in tabs), n))


if __name__ == "__main__":
    main()
