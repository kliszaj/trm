#!/usr/bin/env python3
"""
Convert TTF/OTF fonts to TFT_eSPI VLW smooth font format.

Usage:
    pip install freetype-py
    python convert_fonts.py

Input:  firmware/tools/fonts/<fontname>.otf  (or .ttf)
Output: firmware/data/<name>-<size>.vlw      (flashed to LittleFS)
"""

import struct
from pathlib import Path
import sys

try:
    import freetype
except ImportError:
    print("Missing dependency — run:  pip install freetype-py")
    sys.exit(1)

# Printable ASCII (space → ~). Covers all reminder names and labels.
CHARSET = [chr(c) for c in range(0x20, 0x7F)]


def convert(font_path: str, size_px: int, output_path: str):
    face = freetype.Face(font_path)
    face.set_pixel_sizes(0, size_px)

    glyphs = []
    for ch in CHARSET:
        face.load_char(ch, freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_NORMAL)
        g = face.glyph
        bm = g.bitmap
        glyphs.append({
            "code":       ord(ch),
            "width":      bm.width,
            "height":     bm.rows,
            "xAdvance":   g.advance.x >> 6,
            "xOffset":    g.bitmap_left,
            "yOffset":    -g.bitmap_top,   # negative = above baseline
            "bitmap_top": g.bitmap_top,
            "data":       bytes(bm.buffer),
        })

    sz         = face.size
    ascent     = sz.ascender >> 6
    descent    = sz.descender >> 6          # negative
    yAdvance   = (sz.ascender - sz.descender) >> 6

    face.load_char(" ")
    spaceWidth = face.glyph.advance.x >> 6

    non_empty  = [g for g in glyphs if g["width"] > 0]
    maxAscent  = max(g["bitmap_top"]                for g in non_empty) if non_empty else ascent
    maxDescent = max(g["height"] - g["bitmap_top"]  for g in non_empty) if non_empty else -descent

    with open(output_path, "wb") as f:
        # ── File header (7 × 4-byte big-endian) ──────────────────────────
        f.write(struct.pack(">I", len(glyphs)))
        f.write(struct.pack(">I", yAdvance))
        f.write(struct.pack(">I", spaceWidth))
        f.write(struct.pack(">i", ascent))
        f.write(struct.pack(">i", descent))
        f.write(struct.pack(">i", maxAscent))
        f.write(struct.pack(">i", maxDescent))

        # ── Glyph headers (6 × 4-byte big-endian per glyph) ──────────────
        for g in glyphs:
            f.write(struct.pack(">I", g["code"]))
            f.write(struct.pack(">I", g["height"]))
            f.write(struct.pack(">I", g["width"]))
            f.write(struct.pack(">I", g["xAdvance"]))
            f.write(struct.pack(">i", g["xOffset"]))
            f.write(struct.pack(">i", g["yOffset"]))

        # ── Bitmap data (8-bit grayscale, in same order as headers) ───────
        for g in glyphs:
            f.write(g["data"])

    size_kb = Path(output_path).stat().st_size / 1024
    print(f"  ✓  {Path(output_path).name}  ({len(glyphs)} glyphs, {size_px}px, {size_kb:.1f} KB)")


if __name__ == "__main__":
    tools_dir = Path(__file__).parent
    fonts_dir = tools_dir / "fonts"
    data_dir  = tools_dir / ".." / "data"
    data_dir.mkdir(exist_ok=True)

    # ── Conversions ───────────────────────────────────────────────────────
    # (font_file, size_px, output_name)
    conversions = [
        ("PPMondwest-Regular.otf", 20, "PPMondwest-20"),   # "Next" label + time string
        ("PPMondwest-Regular.otf", 36, "PPMondwest-36"),   # Reminder name (idle)
        ("PPMondwest-Regular.otf", 42, "PPMondwest-42"),   # Reminder name (active)
    ]

    print(f"Converting fonts → {data_dir.resolve()}\n")
    any_missing = False
    for font_file, size, name in conversions:
        path = fonts_dir / font_file
        if not path.exists():
            print(f"  ✗  Not found: {path}")
            any_missing = True
            continue
        convert(str(path), size, str(data_dir / f"{name}.vlw"))

    if any_missing:
        print(f"\nPlace font files in:  {fonts_dir.resolve()}")
