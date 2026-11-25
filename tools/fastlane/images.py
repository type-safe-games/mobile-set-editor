#!/usr/bin/env python3
"""The store listing's icon and feature graphic (#79), drawn from the app's adaptive launcher icon
(#76) vector drawables, into fastlane/metadata/android/en-US/images/:

- icon.png, 512 x 512: the icon as launchers show it, with rounded corners;
- featureGraphic.png, 1024 x 500: the icon's card and nib on its background, with the app's name.

    tools/fastlane/images.py [out-dir]

The name is set in Noto Sans, so that font must be installed (fontconfig).

Needs rsvg-convert (librsvg). Understands what the icon's drawables use: <group> with translate,
rotation, scale and pivot, and <path> with fill (and its fill type) and stroke.
"""
from __future__ import annotations

import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
RES = REPO / "android/app/src/main/res"
A = "{http://schemas.android.com/apk/res/android}"


def color(value: str | None) -> str:
    """#RRGGBB or #AARRGGBB as SVG colour and opacity attributes"""
    if not value:
        return 'fill="none"'
    v = value.lstrip("#")
    if len(v) == 8:
        return f'"#{v[2:]}" opacity="{int(v[:2], 16) / 255:.3f}"'
    return f'"#{v}"'


def svg_of(node: ET.Element) -> str:
    out = []
    for child in node:
        if child.tag == "group":
            tx, ty = child.get(A + "translateX", "0"), child.get(A + "translateY", "0")
            rot = child.get(A + "rotation", "0")
            sx, sy = child.get(A + "scaleX", "1"), child.get(A + "scaleY", "1")
            px, py = child.get(A + "pivotX", "0"), child.get(A + "pivotY", "0")
            # VectorDrawable: scale and rotate about the pivot, then translate
            out.append(f'<g transform="translate({tx},{ty}) translate({px},{py}) rotate({rot}) scale({sx},{sy}) '
                       f'translate(-{px},-{py})">{svg_of(child)}</g>')
        elif child.tag == "path":
            attrs = [f'd="{child.get(A + "pathData")}"']
            fill = child.get(A + "fillColor")
            attrs.append(f"fill={color(fill)}" if fill else 'fill="none"')
            if child.get(A + "fillType") == "evenOdd":
                attrs.append('fill-rule="evenodd"')
            stroke = child.get(A + "strokeColor")
            if stroke:
                attrs.append(f"stroke={color(stroke)}")
                attrs.append(f'stroke-width="{child.get(A + "strokeWidth", "1")}"')
                attrs.append(f'stroke-linejoin="{child.get(A + "strokeLineJoin", "miter")}"')
            out.append("<path " + " ".join(attrs) + "/>")
    return "".join(out)


def render(svg: str, width: int, height: int, out: Path) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["rsvg-convert", "-w", str(width), "-h", str(height), "-o", str(out)], input=svg.encode(), check=True)
    print(out)


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else REPO / "fastlane/metadata/android/en-US/images"
    background = ET.parse(RES / "values/ic_launcher_background.xml").getroot().find("color").text
    fg = svg_of(ET.parse(RES / "drawable/ic_launcher_foreground.xml").getroot())
    # the adaptive icon's 108dp canvas; launchers show the middle 72dp, as here
    render(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="18 18 72 72">'
        f'<defs><clipPath id="c"><rect x="18" y="18" width="72" height="72" rx="16"/></clipPath></defs>'
        f'<g clip-path="url(#c)"><rect x="0" y="0" width="108" height="108" fill="{background}"/>{fg}</g></svg>',
        512, 512, out / "icon.png",
    )
    # the card and nib at the left (the icon's 108-unit canvas scaled to 500 high), the name at the right
    render(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="500" viewBox="0 0 1024 500">'
        f'<rect width="1024" height="500" fill="{background}"/>'
        f'<g transform="translate(-30,0) scale({500 / 108})">{fg}</g>'
        f'<text x="450" y="235" font-family="Noto Sans" font-weight="bold" font-size="64" fill="#FFFBF5">Mobile Set</text>'
        f'<text x="450" y="310" font-family="Noto Sans" font-weight="bold" font-size="64" fill="#FFFBF5">Editor</text>'
        f'<text x="452" y="370" font-family="Noto Sans" font-size="27" fill="#A8E0D8">Design your own cards on your phone</text>'
        f'</svg>',
        1024, 500, out / "featureGraphic.png",
    )


if __name__ == "__main__":
    main()
