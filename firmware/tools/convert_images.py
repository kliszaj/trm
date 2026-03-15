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

def convert(src_path: str, dst_path: str, target_size: int = DISPLAY_SIZE, fill: float = 1.0):
    img = Image.open(src_path).convert("RGBA")

    # Scale to fit target while preserving aspect ratio, then center on transparent canvas
    w, h = img.size
    scale = min(target_size / w, target_size / h) * fill
    new_w, new_h = int(w * scale), int(h * scale)
    img = img.resize((new_w, new_h), Image.LANCZOS)

    canvas = Image.new("RGBA", (target_size, target_size), (0, 0, 0, 0))
    canvas.paste(img, ((target_size - new_w) // 2, (target_size - new_h) // 2))
    img = canvas

    with open(dst_path, "wb") as f:
        for y in range(target_size):
            for x in range(target_size):
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
    print(f"  OK  {Path(dst_path).name}  ({size_kb:.0f} KB)")


if __name__ == "__main__":
    tools_dir = Path(__file__).parent
    images_dir = tools_dir / "images"
    data_dir   = tools_dir / ".." / "data"
    data_dir.mkdir(exist_ok=True)

    # (source filename, output name)
    conversions = [
        ("feed_evie.png",      "img_feed_evie",      0.65),
        ("water_plants.png",   "img_water_plants",   0.65),
        ("eat_vitamins.png",   "img_eat_vitamins",   0.65),
        ("take_out_trash.png", "img_take_out_trash", 0.65),
        ("logo.png",           "img_logo",           0.65),
        ("checkmark.png",      "img_checkmark",      0.65),
        ("pay_bills.png",      "img_pay_bills",      0.65),
    ]

    print(f"Converting images -> {data_dir.resolve()}\n")
    any_missing = False
    for entry in conversions:
        src_name, out_name = entry[0], entry[1]
        fill = entry[2] if len(entry) > 2 else 1.0
        src = images_dir / src_name
        if not src.exists():
            print(f"  MISSING  Not found: {src}")
            any_missing = True
            continue
        convert(str(src), str(data_dir / f"{out_name}.bin"), fill=fill)

    if any_missing:
        print(f"\nPlace PNG files in:  {images_dir.resolve()}")
