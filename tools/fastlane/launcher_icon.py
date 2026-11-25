#!/usr/bin/env python3
"""Write the launcher icon (#98) as the app's foreground and monochrome vector drawables: a fanned
hand of three cards, the front one with a title, a sunset over hills and text lines.

    tools/fastlane/launcher_icon.py [res-dir]

then tools/fastlane/images.py for the store listing images. Needs shapely (pip install shapely):
the monochrome layer is one colour, so each back card is cut to the part that shows.
"""
import sys
from pathlib import Path
RES = sys.argv[1] if len(sys.argv) > 1 else str(Path(__file__).resolve().parents[2] / "android/app/src/main/res")
def rr(x, y, w, h, r):
    """a rounded rectangle as path data"""
    return (f"M{x+r:g},{y:g}h{w-2*r:g}a{r:g},{r:g} 0,0 1,{r:g},{r:g}v{h-2*r:g}a{r:g},{r:g} 0,0 1,{-r:g},{r:g}"
            f"h{-(w-2*r):g}a{r:g},{r:g} 0,0 1,{-r:g},{-r:g}v{-(h-2*r):g}a{r:g},{r:g} 0,0 1,{r:g},{-r:g}z")
def rect(x, y, w, h): return f"M{x:g},{y:g}h{w:g}v{h:g}h{-w:g}z"
def p(fill, d): return f'        <path android:fillColor="{fill}" android:pathData="{d}" />\n'

X, Y, W, H = 36.5, 30, 35, 49   # a card
B = 2.2                          # its white border
fx, fy, fw, fh = X+B, Y+B, W-2*B, H-2*B
P = 1.8
title = (fx+P, fy+P, fw-2*P, fh*0.1)
ay = fy+P+fh*0.1+1.4; ah = fh*0.42
art = (fx+P, ay, fw-2*P, ah)
ty = ay+ah+1.6; th = fh-(ty-fy)-P
box = (fx+P, ty, fw-2*P, th)
lines = [(fx+P+2, ty+2.2+i*3.4, (fw-2*P-4)*(0.62 if i == 2 else 1), 1.6) for i in range(3)]

def card(frame, art_fill, mono=False):
    k = "#FF000000"
    if mono:
        # outline, title, art and text box as cut-outs: an even-odd frame with holes
        d = rr(X, Y, W, H, 3) + rect(*title) + rect(*art) + rect(*box)
        s = f'        <path android:fillColor="{k}" android:fillType="evenOdd" android:pathData="{d}" />\n'
        s += p(k, "".join(rect(*l) for l in lines))
        ax, ayy, aw, ahh = art
        s += p(k, f"M{ax:g},{ayy+ahh:g}v-4.5l8,-7 5,4 6,-6 {aw-19:g},{8.5:g}v{0.5:g}z")
        return s
    s = p("#FFFFFFFF", rr(X, Y, W, H, 3)) + p(frame, rr(fx, fy, fw, fh, 2))
    s += p("#FFF7F1E3", rect(*title)) + p("#FFF7F1E3", rect(*box))
    s += p("#FFCFC6B4", "".join(rect(*l) for l in lines))
    s += art_fill
    return s

ax, ayy, aw, ahh = art
sunset = (p("#FF7A2E5C", rect(*art)) + p("#FFF0663F", rect(ax, ayy, aw, ahh*0.72))
          + p("#FFFFB347", rect(ax, ayy, aw, ahh*0.42))
          + p("#FFFFE9A8", f"M{ax+aw-6.6:g},{ayy+4.4:g}m-3,0a3,3 0,1 0,6,0a3,3 0,1 0,-6,0")
          + p("#D93B1F3F", f"M{ax:g},{ayy+ahh:g}v-4.5l8,-7 5,4 6,-6 {aw-19:g},{8.5:g}v{0.5:g}z"))

def doc(body, comment):
    return f'''<?xml version="1.0" encoding="utf-8"?>
<!-- Mobile Set Editor - SPDX-License-Identifier: GPL-3.0-or-later
     {comment} -->
<vector xmlns:android="http://schemas.android.com/apk/res/android"
    android:width="108dp"
    android:height="108dp"
    android:viewportWidth="108"
    android:viewportHeight="108">
    <!-- scaled about the centre so the fanned corners stay inside the 66dp safe zone -->
    <group android:pivotX="54" android:pivotY="54" android:scaleX="0.84" android:scaleY="0.84" android:translateY="-1">
{body}    </group>
</vector>
'''
def fan(mono):
    back = lambda rot, frame, art_c: (f'    <group android:pivotX="54" android:pivotY="86" android:rotation="{rot}">\n'
        + card(frame, p(art_c, rect(*art)), mono) + '    </group>\n')
    front = f'    <group android:translateY="-2">\n' + card("#FF2A2A3C", sunset, mono) + '    </group>\n'
    if mono:  # each back card only where it shows: minus what is drawn over it, with a gap
        from shapely.geometry import box as sbox
        from shapely import affinity
        def shape(rot, dy=0.0):
            g = sbox(X, Y, X+W, Y+H).buffer(-3).buffer(3, quad_segs=6)
            return affinity.translate(affinity.rotate(g, rot, origin=(54, 86)), 0, dy)
        front_s, right_s, left_s = shape(0, -2), shape(18), shape(-20)
        gap = 1.3
        right_v = right_s.difference(front_s.buffer(gap))
        left_v = left_s.difference(right_s.buffer(gap)).difference(front_s.buffer(gap))
        def d(g):
            polys = getattr(g, "geoms", [g])
            out = ""
            for poly in polys:
                for ring in [poly.exterior, *poly.interiors]:
                    pts = list(ring.coords)[:-1]
                    out += "M" + "L".join(f"{x:.2f},{y:.2f}" for x, y in pts) + "z"
            return out
        return (p("#FF000000", d(left_v)) + p("#FF000000", d(right_v))
                + '    <group android:translateY="-2">\n' + card(None, None, True) + '    </group>\n')
    if False:
        back = lambda rot, *_: (f'    <group android:pivotX="54" android:pivotY="86" android:rotation="{rot}">\n'
            + p("#80000000", rr(X, Y, W, H, 3)) + "    </group>\n")
        front = (f'    <group android:translateY="-2">\n' + card(None, None, True) + '    </group>\n')
    return back(-20, "#FFE9C46A", "#FFF4DE9C") + back(18, "#FF5FA8D3", "#FFA7D3EC") + front

open(f"{RES}/drawable/ic_launcher_foreground.xml", "w").write(doc(fan(False),
  "The launcher icon's foreground (#98): a fanned hand of three cards, the front one with a title,\n"
  "     a sunset over hills and text lines. Original artwork; nothing from any Template pack."))
open(f"{RES}/drawable/ic_launcher_monochrome.xml", "w").write(doc(fan(True),
  "The launcher icon's monochrome layer, for themed icons (#98): the fanned cards in one colour,\n"
  "     the front card's title, art and text box cut out, which the launcher tints."))
