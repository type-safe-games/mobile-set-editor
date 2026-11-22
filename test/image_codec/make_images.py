#!/usr/bin/env python3
"""Regenerate the codec test images in images/ (needs Pillow). They are self-made and free.

Run from anywhere: python3 test/image_codec/make_images.py
"""
import random
import struct
import zlib
from pathlib import Path

from PIL import Image, PngImagePlugin

OUT = Path(__file__).resolve().parent / "images"
OUT.mkdir(exist_ok=True)
random.seed(41)
W, H = 23, 17


def save(im, name, **kw):
    im.save(OUT / name, **kw)


a = Image.new("RGBA", (W, H))
a.putdata([(random.randrange(256), random.randrange(256), random.randrange(256),
            random.choice([0, 40, 128, 255, 255])) for _ in range(W * H)])
save(a, "rgba.png")
save(a.convert("RGB"), "rgb.png")
# An RGBA PNG whose pixels are all opaque: wx (and so the Skia codec) gives no alpha plane
save(a.convert("RGB").convert("RGBA"), "rgba-opaque.png")
g = a.convert("L")
save(g, "gray.png")
save(a.convert("LA"), "gray-alpha.png")
save(g.convert("1"), "gray-1bit.png")
save(a.convert("RGB").quantize(16), "palette-4bit.png", bits=4)
trns = bytes([0, 128] + [255] * 198)
save(a.convert("RGB").quantize(200), "palette-trns.png", transparency=trns)


def png16(name, color_type, channels):
    """A 16 bit per channel PNG, written by hand (Pillow can't)."""
    raw = b""
    for _ in range(H):
        raw += b"\0" + b"".join(struct.pack(">H", random.randrange(65536)) for _ in range(W * channels))

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    (OUT / name).write_bytes(b"\x89PNG\r\n\x1a\n"
                             + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 16, color_type, 0, 0, 0))
                             + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


png16("rgb-16bit.png", 2, 3)
png16("rgba-16bit.png", 6, 4)
png16("gray-16bit.png", 0, 1)

# A gAMA chunk must not change the stored colours
info = PngImagePlugin.PngInfo()
info.add(b"gAMA", struct.pack(">I", 100000))
save(a.convert("RGB"), "rgb-gamma.png", pnginfo=info)
# A card description written by another encoder, next to another tEXt chunk
info = PngImagePlugin.PngInfo()
info.add_text("Description", "<mse-card-data>[x]</mse-card-data>")
info.add_text("Comment", "other")
save(a, "described.png", pnginfo=info)

b = Image.new("RGB", (64, 48))
b.putdata([((x * 4) % 256, (y * 5) % 256, (x * y) % 256) for y in range(48) for x in range(64)])
save(b, "baseline.jpg", quality=85)
save(b, "progressive.jpg", quality=85, progressive=True)
save(b.convert("L"), "gray.jpg", quality=85)
save(b, "subsampling-444.jpg", quality=90, subsampling=0)
save(b.convert("CMYK"), "cmyk.jpg", quality=90)
