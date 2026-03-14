#!/usr/bin/env python3
"""
Convert pixel art PNG images to raw RGB565 binary files for TFT_eSPI / LittleFS.

Transparent pixels → magenta chroma key (0xF81F) so the firmware skips them.
Output images are 240×240 px, stored as big-endian RGB565 (2 bytes per pixel).

Usage:
    pip install Pillow
    python convert_images.py

Input:  firmware/tools/images/<name>.png  (transparent PNG, any size — will be scaled)
Output: firmware/data/img_<name>.bin      (flashed to LittleFS via PlatformIO)
"""

import struct
from pathlib import Path
import sys

try:
    from PIL import Image
except ImportError:
    print("Missing dependency — run:  pip install Pillow")
    sys.exit(1)

DISPLAY_SIZE = 240
CHROMA_KEY   = 0xF81F  # magenta — transparent pixels become this in the output

def to_rgb565_be(r: int, g: int, b: int) -> int:
    """Convert 8-bit RGB to big-endian RGB565."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def convert(src_path: str, dst_path: str):
    img = Image.open(src_path).convert("RGBA")
    img = img.resize((DISPLAY_SIZE, DISPLAY_SIZE), Image.LANCZOS)

    with open(dst_path, "wb") as f:
        for y in range(DISPLAY_SIZE):
            for x in range(DISPLAY_SIZE):
                r, g, b, a = img.getpixel((x, y))
                if a < 128:
                    pixel = CHROMA_KEY
                else:
                    pixel = to_rgb565_be(r, g, b)
                    # Avoid accidentally producing the chroma key color
                    if pixel == CHROMA_KEY:
                        pixel = 0xF820
                f.write(struct.pack(">H", pixel))  # big-endian

    size_kb = Path(dst_path).stat().st_size / 1024
    print(f"  ✓  {Path(dst_path).name}  ({size_kb:.0f} KB)")


if __name__ == "__main__":
    tools_dir = Path(__file__).parent
    images_dir = tools_dir / "images"
    data_dir   = tools_dir / ".." / "data"
    data_dir.mkdir(exist_ok=True)

    # (source filename, output name)
    conversions = [
        ("feed_evie.png",      "img_feed_evie"),
        ("water_plants.png",   "img_water_plants"),
        ("eat_vitamins.png",   "img_eat_vitamins"),
        ("take_out_trash.png", "img_take_out_trash"),
        ("logo.png",           "img_logo"),
        ("checkmark.png",      "img_checkmark"),
    ]

    print(f"Converting images → {data_dir.resolve()}\n")
    any_missing = False
    for src_name, out_name in conversions:
        src = images_dir / src_name
        if not src.exists():
            print(f"  ✗  Not found: {src}")
            any_missing = True
            continue
        convert(str(src), str(data_dir / f"{out_name}.bin"))

    if any_missing:
        print(f"\nPlace PNG files in:  {images_dir.resolve()}")
