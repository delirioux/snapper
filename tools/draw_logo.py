#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Snapper's logo as SVG (assets/logo.svg): render it at 1024 x 1024 to assets/logo.png (any browser, rsvg-convert or
cairosvg).
Usage: draw_logo.py [ACCENT_COLOUR] [OUT.svg]"""
# The look (family style):
# black ground, a ring, audio bars, a line-art fish in one accent colour + cream,
# music notes, a little knob box (Snapper: two 3D knobs), a wide italic name.
# Snapper's own touches: snapper red accent with a cold steel-blue second colour; the ring
# throws three echo rings that are cut off dead by a straight line, and the audio bars ring
# out and then stop at once (a gated burst); a deep-bodied red snapper with a spiny dorsal
# and a few sharp teeth; a heavy 80s wordmark sliced by black gate lines.
import math, sys

ACC = sys.argv[1] if len(sys.argv) > 1 else "#ff3b2f"   # the accent colour (snapper red)
OUT = sys.argv[2] if len(sys.argv) > 2 else str(__import__("pathlib").Path(__file__).resolve().parent.parent / "assets" / "logo.svg")
STEEL = "#5ab8ff"                                       # second colour: cold steel blue
CREAM, INK, BG = "#fff1ec", "#0a0405", "#000000"
W = H = 1024
CX, CY, R = 512, 440, 362

# ---- letters on a 6 x 7 grid, chamfered like the rest of the family (holes via even-odd)
HOLE = [(1.7, 1.6), (4.3, 1.6), (4.3, 3.0), (1.7, 3.0)]
L = {
    "S": [[(0.9,0),(6,0),(6,1.6),(1.7,1.6),(1.7,2.7),(5.1,2.7),(6,3.6),(6,6.1),(5.1,7),
           (0,7),(0,5.4),(4.3,5.4),(4.3,4.3),(0.9,4.3),(0,3.4),(0,0.9)]],
    "N": [[(0,0),(1.8,0),(4.3,4.1),(4.3,0),(6,0),(6,7),(4.2,7),(1.7,2.9),(1.7,7),(0,7)]],
    "A": [[(0.9,0),(5.1,0),(6,0.9),(6,7),(4.3,7),(4.3,4.6),(1.7,4.6),(1.7,7),(0,7),(0,0.9)], HOLE],
    "P": [[(0,0),(5.1,0),(6,0.9),(6,3.7),(5.1,4.6),(1.7,4.6),(1.7,7),(0,7)], HOLE],
    "E": [[(0.9,0),(6,0),(6,1.6),(1.7,1.6),(1.7,2.7),(5,2.7),(5,4.3),(1.7,4.3),(1.7,5.4),
           (6,5.4),(6,7),(0.9,7),(0,6.1),(0,0.9)]],
    "R": [[(0,0),(5.1,0),(6,0.9),(6,3.7),(5.4,4.3),(6,4.9),(6,7),(4.3,7),(4.3,5.3),(3.6,4.6),
           (1.7,4.6),(1.7,7),(0,7)], HOLE],
}
WID = {c: 6 for c in L}
GAP, WORD, SK = 0.75, "SNAPPER", 0.30
total = sum(WID[c] for c in WORD) + GAP * (len(WORD) - 1)
U = 18.0
TY0 = 826


def word_path(dx=0.0, dy=0.0):
    x0 = (W - total * U - 7 * U * SK) / 2
    d, x = [], 0.0
    for c in WORD:
        for poly in L[c]:
            pts = [f"{x0 + (x + px) * U + (7 - py) * U * SK + dx:.1f},{TY0 + py * U + dy:.1f}" for px, py in poly]
            d.append("M" + " L".join(pts) + " Z")
        x += WID[c] + GAP
    return " ".join(d)


# ---- echo rings round the main ring, gated: a straight cut ends them dead
echoes = "".join(
    f'<circle cx="{CX}" cy="{CY}" r="{R + 26 + 24*i}" fill="none" stroke="{ACC if i % 2 == 0 else STEEL}" '
    f'stroke-width="{6 - 1.5*i:.1f}" opacity="{0.55 - 0.15*i:.2f}"/>' for i in range(3))

# ---- audio bars: a burst that rings out, then the gate slams shut (stubs only)
bars = []
n = 24
for i in range(n):
    x = 176 + i * 29
    if i >= 15:
        break                                           # the gate: nothing after the cut
    h = 440 * (1 - i / 21) * (0.8 + 0.2 * abs(math.sin(i * 1.7 + 0.4)))
    bars.append(f'<rect x="{x}" y="{CY - 10 - h * 0.6:.0f}" width="13" height="{h:.0f}" rx="2"/>')

# ---- the snapper (red snapper), facing right: deep body, sloping forehead, pointed snout, spiny dorsal
body = ("M 880 392 "
        "C 850 340, 800 290, 740 266 "                  # snout up the sloping forehead
        "C 660 236, 560 230, 470 252 "                  # the high arched back
        "C 400 270, 340 310, 300 346 "                  # back down to the tail stem
        "Q 240 300, 168 262 Q 196 330, 214 380 "        # upper tail lobe
        "Q 196 430, 168 498 Q 240 460, 300 414 "        # lower tail lobe
        "C 360 460, 440 498, 540 506 "                  # deep belly
        "C 660 514, 780 470, 836 430 "                  # throat
        "C 860 414, 874 402, 880 392 Z")
belly = ("M 836 430 C 780 470, 660 514, 540 506 C 440 498, 360 460, 300 414 "
         "C 400 444, 500 462, 600 460 C 700 458, 790 444, 846 420 Z")
back_band = ("M 740 266 C 660 236, 560 230, 470 252 C 400 270, 340 310, 300 346 "
             "C 380 310, 460 290, 560 286 C 640 284, 700 292, 760 300 Z")   # the deep red back
dorsal = ("M 404 268 L 424 214 L 448 256 L 472 202 L 496 248 L 520 196 L 544 242 L 568 198 "
          "L 592 242 L 616 206 L 640 246 L 666 222 L 704 262 Z")
d_rays = "".join(f'<path d="M {x0} {y0} L {x1} {y1}"/>' for x0, y0, x1, y1 in
                 [(432, 262, 424, 222), (478, 252, 472, 210), (524, 246, 520, 204),
                  (570, 246, 568, 206), (616, 250, 616, 214)])
pelvic = "M 600 504 L 588 560 L 652 500 Z"
anal = "M 392 462 L 366 516 L 474 496 Z"
pect = "M 720 404 Q 662 424, 610 432 Q 650 392, 716 384 Z"
gill = "M 742 284 C 714 330, 714 400, 748 452"
lat = "M 730 330 C 620 300, 460 300, 310 370"     # pale lateral line
mouth = "M 880 392 C 860 404, 838 410, 816 408"   # the snapping jaw
teeth = "".join(f'<path d="M {x} {y} l 7 16 l 7 -17 z"/>' for x, y in [(828, 403), (850, 399)])
scales = "".join(
    f'<path d="M {x} {y} q 12 10 0 20"/>'
    for x in range(360, 720, 40) for y in range(320, 470, 28)
    if (x - 540) ** 2 / 200 ** 2 + (y - 392) ** 2 / 78 ** 2 < 1)


def note(x, y, s, double=False):
    """An eighth (or a pair of eighths) note."""
    g = [f'<ellipse cx="{x}" cy="{y}" rx="{13*s}" ry="{9.5*s}" transform="rotate(-22 {x} {y})"/>',
         f'<rect x="{x + 9*s:.1f}" y="{y - 58*s:.1f}" width="{4.5*s:.1f}" height="{58*s:.1f}"/>']
    if double:
        x2 = x + 44 * s
        g += [f'<ellipse cx="{x2}" cy="{y - 6*s}" rx="{13*s}" ry="{9.5*s}" transform="rotate(-22 {x2} {y - 6*s})"/>',
              f'<rect x="{x2 + 9*s:.1f}" y="{y - 64*s:.1f}" width="{4.5*s:.1f}" height="{58*s:.1f}"/>',
              f'<polygon points="{x + 9*s:.1f},{y - 58*s:.1f} {x2 + 13.5*s:.1f},{y - 64*s:.1f} {x2 + 13.5*s:.1f},{y - 52*s:.1f} {x + 9*s:.1f},{y - 46*s:.1f}"/>']
    else:
        g.append(f'<path d="M {x + 13.5*s:.1f} {y - 58*s:.1f} q {22*s:.1f} {14*s:.1f} {16*s:.1f} {38*s:.1f} '
                 f'q {-2*s:.1f} {-16*s:.1f} {-16*s:.1f} {-22*s:.1f} z"/>')
    return "".join(g)


def glint(x, y, r):
    """A four-point sparkle."""
    k = r * 0.22
    return (f'<path d="M {x} {y - r} Q {x + k} {y - k} {x + r} {y} Q {x + k} {y + k} {x} {y + r} '
            f'Q {x - k} {y + k} {x - r} {y} Q {x - k} {y - k} {x} {y - r} Z"/>')


def knob(x, y):
    ticks = "".join(
        f'<line x1="{x + 30*math.cos(a):.1f}" y1="{y + 30*math.sin(a):.1f}" x2="{x + 37*math.cos(a):.1f}" y2="{y + 37*math.sin(a):.1f}"/>'
        for a in [math.radians(d) for d in range(135, 406, 45)])
    return (f'<g stroke="{INK}" stroke-width="3" stroke-linecap="round">{ticks}</g>'
            f'<circle cx="{x}" cy="{y}" r="22" fill="{ACC}" stroke="{INK}" stroke-width="5"/>'
            f'<line x1="{x}" y1="{y}" x2="{x}" y2="{y - 16}" stroke="{INK}" stroke-width="5" stroke-linecap="round"/>')


def knob3d(x, y, ang):
    """A 3D knob seen from slightly above: drop shadow, a red skirt cylinder, a domed cap and a pointer."""
    rx, ry, hgt = 36, 21, 28                     # cap ellipse radii and cylinder height
    top = y - hgt / 2                            # centre of the cap
    bot = y + hgt / 2                            # centre of the base ellipse
    px, py = x + 24 * math.cos(math.radians(ang)), top + 14 * math.sin(math.radians(ang))
    ticks = "".join(
        f'<line x1="{x + 42*math.cos(a):.1f}" y1="{bot + 26*math.sin(a):.1f}" x2="{x + 50*math.cos(a):.1f}" y2="{bot + 31*math.sin(a):.1f}"/>'
        for a in [math.radians(d) for d in range(135, 406, 45)])
    return (f'<g stroke="{INK}" stroke-width="3" stroke-linecap="round" opacity="0.7">{ticks}</g>'
            f'<ellipse cx="{x + 6}" cy="{bot + 6}" rx="{rx + 4}" ry="{ry + 3}" fill="{INK}" opacity="0.28"/>'
            f'<path d="M {x - rx} {top} L {x - rx} {bot} A {rx} {ry} 0 0 0 {x + rx} {bot} L {x + rx} {top} Z" '
            f'fill="url(#skirt)" stroke="{INK}" stroke-width="4" stroke-linejoin="round"/>'
            f'<ellipse cx="{x}" cy="{top}" rx="{rx}" ry="{ry}" fill="url(#cap)" stroke="{INK}" stroke-width="4"/>'
            f'<ellipse cx="{x - 10}" cy="{top - 7}" rx="12" ry="5" fill="#ffffff" opacity="0.55" transform="rotate(-12 {x - 10} {top - 7})"/>'
            f'<line x1="{x}" y1="{top}" x2="{px:.1f}" y2="{py:.1f}" stroke="{INK}" stroke-width="6" stroke-linecap="round"/>'
            f'<line x1="{x}" y1="{top}" x2="{px:.1f}" y2="{py:.1f}" stroke="{CREAM}" stroke-width="2" stroke-linecap="round"/>')


word = word_path()
shadow = word_path(9, 9)
# gate lines slicing the lower half of the wordmark
gates = "".join(f'<rect x="0" y="{TY0 + 7*U*f:.1f}" width="{W}" height="{3 + 2*i}"/>'
                for i, f in enumerate([0.56, 0.70, 0.84]))
BX, BY, BW, BH = 512 - 150, 616, 300, 120
CUT = 600   # where the gate cuts the echo rings
svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">
<defs>
<linearGradient id="hot" gradientUnits="userSpaceOnUse" x1="0" y1="{TY0}" x2="0" y2="{TY0 + 7*U}"><stop offset="0" stop-color="#ffffff"/><stop offset="0.18" stop-color="#ffd2c8"/><stop offset="0.48" stop-color="{ACC}"/><stop offset="0.78" stop-color="#a3121a"/><stop offset="1" stop-color="#5a0610"/></linearGradient>
<clipPath id="ring"><circle cx="{CX}" cy="{CY}" r="{R - 14}"/></clipPath>
<linearGradient id="skirt" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#5a0610"/><stop offset="0.35" stop-color="{ACC}"/><stop offset="0.55" stop-color="#ff9a8a"/><stop offset="0.75" stop-color="{ACC}"/><stop offset="1" stop-color="#5a0610"/></linearGradient>
<radialGradient id="cap" cx="0.38" cy="0.3" r="0.8"><stop offset="0" stop-color="#ffb3a8"/><stop offset="0.45" stop-color="{ACC}"/><stop offset="1" stop-color="#8a0d16"/></radialGradient>
<clipPath id="wordclip"><path d="{word}" clip-rule="evenodd"/></clipPath>
<clipPath id="gate"><rect x="0" y="0" width="{W}" height="{CUT}"/></clipPath></defs>
<rect width="{W}" height="{H}" fill="{BG}"/>
<g clip-path="url(#gate)">{echoes}</g>
<g stroke-linecap="round"><line x1="40" y1="{CUT}" x2="140" y2="{CUT}" stroke="{STEEL}" stroke-width="4" opacity="0.7"/><line x1="884" y1="{CUT}" x2="984" y2="{CUT}" stroke="{STEEL}" stroke-width="4" opacity="0.7"/></g>
<circle cx="{CX}" cy="{CY}" r="{R}" fill="#0c0507" stroke="{ACC}" stroke-width="11"/>
<g clip-path="url(#ring)" fill="{ACC}">{"".join(bars)}</g>

<!-- the box with two 3D knobs, wired to the ring -->
<path d="M {BX} {BY + BH/2} C {BX - 60} {BY + BH/2}, {BX - 70} {BY - 40}, {BX - 70} {BY - 100}" fill="none" stroke="{ACC}" stroke-width="7"/>
<path d="M {BX + BW} {BY + BH/2} C {BX + BW + 60} {BY + BH/2}, {BX + BW + 70} {BY - 40}, {BX + BW + 70} {BY - 100}" fill="none" stroke="{ACC}" stroke-width="7"/>
<rect x="{BX}" y="{BY}" width="{BW}" height="{BH}" rx="16" fill="{CREAM}" stroke="{ACC}" stroke-width="9"/>
{knob3d(BX + 82, BY + 58, -60)}{knob3d(BX + BW - 82, BY + 58, -125)}

<!-- the snapper -->
<g transform="rotate(-4 520 380) translate(-6 -12)">
  <path d="{body}" fill="none" stroke="{BG}" stroke-width="26" stroke-linejoin="round"/>
  <path d="{dorsal}" fill="none" stroke="{BG}" stroke-width="20" stroke-linejoin="round"/>
  <path d="{dorsal}" fill="{INK}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <g fill="none" stroke="{CREAM}" stroke-width="3" stroke-linecap="round">{d_rays}</g>
  <path d="{anal}" fill="{INK}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <path d="{pelvic}" fill="{INK}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <path d="{body}" fill="{INK}" stroke="{ACC}" stroke-width="10" stroke-linejoin="round"/>
  <path d="{back_band}" fill="{ACC}" opacity="0.5"/>
  <path d="{belly}" fill="{CREAM}"/>
  <g fill="none" stroke="{ACC}" stroke-width="3" opacity="0.6">{scales}</g>
  <path d="{lat}" fill="none" stroke="{STEEL}" stroke-width="4" stroke-linecap="round" opacity="0.75"/>
  <path d="M 288 340 Q 230 300, 186 276" fill="none" stroke="{CREAM}" stroke-width="5" stroke-linecap="round"/>
  <path d="M 288 420 Q 230 460, 186 484" fill="none" stroke="{CREAM}" stroke-width="5" stroke-linecap="round"/>
  <path d="{gill}" fill="none" stroke="{ACC}" stroke-width="6" stroke-linecap="round"/>
  <path d="{pect}" fill="{CREAM}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <g fill="{CREAM}" stroke="{ACC}" stroke-width="2" stroke-linejoin="round">{teeth}</g>
  <path d="{mouth}" fill="none" stroke="{ACC}" stroke-width="6" stroke-linecap="round"/>
  <circle cx="792" cy="330" r="21" fill="{CREAM}" stroke="{ACC}" stroke-width="5"/>
  <circle cx="797" cy="329" r="10" fill="{INK}"/>
  <circle cx="801" cy="324" r="3" fill="{CREAM}"/>
</g>

<g fill="{CREAM}">{note(756, 186, 0.75)}{note(806, 152, 0.9, True)}{note(880, 226, 0.7)}</g>
<g fill="{STEEL}">{glint(724, 120, 16)}{glint(912, 160, 12)}{glint(860, 92, 9)}</g>

<g stroke-linecap="round"><line x1="40" y1="{TY0 + 36}" x2="150" y2="{TY0 + 36}" stroke="{ACC}" stroke-width="7"/><line x1="24" y1="{TY0 + 72}" x2="140" y2="{TY0 + 72}" stroke="{STEEL}" stroke-width="5" opacity="0.8"/><line x1="60" y1="{TY0 + 106}" x2="134" y2="{TY0 + 106}" stroke="{ACC}" stroke-width="4" opacity="0.5"/><line x1="880" y1="{TY0 + 46}" x2="996" y2="{TY0 + 46}" stroke="{STEEL}" stroke-width="5" opacity="0.8"/><line x1="870" y1="{TY0 + 84}" x2="972" y2="{TY0 + 84}" stroke="{ACC}" stroke-width="4" opacity="0.5"/></g>
<path d="{shadow}" fill="#0b1f3d" stroke="#0b1f3d" stroke-width="20" stroke-linejoin="round" fill-rule="evenodd"/>
<path d="{word}" fill="none" stroke="{BG}" stroke-width="24" stroke-linejoin="round"/>
<path d="{word}" fill="none" stroke="{STEEL}" stroke-width="12" stroke-linejoin="round"/>
<path d="{word}" fill="url(#hot)" stroke="#2a0508" stroke-width="3" stroke-linejoin="round" fill-rule="evenodd"/>
<g fill="{BG}" clip-path="url(#wordclip)">{gates}</g>
<g fill="#ffffff">{glint(W/2 + 330, TY0 + 4, 26)}{glint(W/2 - 380, TY0 + 128, 15)}</g>
</svg>'''
open(OUT, "w").write(svg)
print("ok", OUT)
