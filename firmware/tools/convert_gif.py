#!/usr/bin/env python3
"""
Convert a GIF animation to a single RGB565 binary for LittleFS playback.

Output format:
  - 2 bytes: frame count (uint16 LE)
  - 2 bytes: frame width (uint16 LE)
  - 2 bytes: frame height (uint16 LE)
  - For each frame: 2 bytes duration in ms (uint16 LE)
  - For each frame: width * height * 2 bytes RGB565 big-endian pixel data

Usage:
    pip install Pillow
    python convert_gif.py
"""

import struct
from pathlib import Path
from PIL import Image, ImageChops

# Config
GIF_PATH = Path(__file__).parent.parent.parent / "eye.gif"
OUTPUT_PATH = Path(__file__).parent.parent / "data" / "anim_eye.bin"
FRAME_SIZE = 120  # store at 120x120, display scaled 2x to 240x240
BG_COLOR = (0, 0, 0)  # black background for transparency
PAUSE_BETWEEN_LOOPS_MS = 5000  # 5 seconds between loops
SKIP_EVERY_OTHER = True  # drop every other frame for speed


def rgb_to_565_be(r, g, b):
    """Convert RGB888 to big-endian RGB565 stored as uint16."""
    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return struct.pack(">H", rgb565)


def find_content_bbox(gif_path):
    """Find the bounding box of non-transparent content across all frames."""
    img = Image.open(str(gif_path))
    min_x, min_y = img.size
    max_x, max_y = 0, 0

    try:
        while True:
            rgba = img.convert("RGBA")
            bg = Image.new("RGBA", rgba.size, (*BG_COLOR, 255))
            comp = Image.alpha_composite(bg, rgba).convert("RGB")
            bg_img = Image.new("RGB", comp.size, BG_COLOR)
            diff = ImageChops.difference(comp, bg_img)
            bbox = diff.getbbox()
            if bbox:
                min_x = min(min_x, bbox[0])
                min_y = min(min_y, bbox[1])
                max_x = max(max_x, bbox[2])
                max_y = max(max_y, bbox[3])
            img.seek(img.tell() + 1)
    except EOFError:
        pass

    # Make it square (use the larger dimension), centered
    w = max_x - min_x
    h = max_y - min_y
    size = max(w, h)
    cx = (min_x + max_x) // 2
    cy = (min_y + max_y) // 2
    half = size // 2 + 4  # small padding

    x1 = max(0, cx - half)
    y1 = max(0, cy - half)
    x2 = min(img.size[0], cx + half)
    y2 = min(img.size[1], cy + half)

    return (x1, y1, x2, y2)


def convert():
    # Find content area across all frames
    crop_box = find_content_bbox(GIF_PATH)
    print(f"  Crop box: {crop_box} ({crop_box[2]-crop_box[0]}x{crop_box[3]-crop_box[1]})")

    img = Image.open(str(GIF_PATH))

    frames = []
    durations = []
    frame_idx = 0

    try:
        while True:
            # Skip every other frame if enabled
            if SKIP_EVERY_OTHER and frame_idx % 2 == 1:
                frame_idx += 1
                img.seek(img.tell() + 1)
                continue

            # Convert frame to RGBA, composite onto black background
            rgba = img.convert("RGBA")
            bg = Image.new("RGBA", rgba.size, (*BG_COLOR, 255))
            composited = Image.alpha_composite(bg, rgba)
            rgb = composited.convert("RGB")

            # Crop to content area, then resize to target
            cropped = rgb.crop(crop_box)
            resized = cropped.resize((FRAME_SIZE, FRAME_SIZE), Image.NEAREST)

            # Get duration (double it if skipping frames to maintain timing)
            dur = img.info.get("duration", 100)
            if SKIP_EVERY_OTHER:
                dur *= 2
            durations.append(dur)

            # Convert pixels to RGB565
            pixel_data = bytearray()
            for y in range(FRAME_SIZE):
                for x in range(FRAME_SIZE):
                    r, g, b = resized.getpixel((x, y))
                    pixel_data.extend(rgb_to_565_be(r, g, b))

            frames.append(bytes(pixel_data))
            frame_idx += 1
            img.seek(img.tell() + 1)
    except EOFError:
        pass

    # Override last frame duration with pause time
    if durations:
        durations[-1] = PAUSE_BETWEEN_LOOPS_MS

    # Write binary
    OUTPUT_PATH.parent.mkdir(exist_ok=True)
    with open(str(OUTPUT_PATH), "wb") as f:
        f.write(struct.pack("<H", len(frames)))
        f.write(struct.pack("<H", FRAME_SIZE))
        f.write(struct.pack("<H", FRAME_SIZE))
        for dur in durations:
            f.write(struct.pack("<H", dur))
        for frame_data in frames:
            f.write(frame_data)

    total_kb = OUTPUT_PATH.stat().st_size / 1024
    print(f"  OK  {OUTPUT_PATH.name}  ({len(frames)} frames, {FRAME_SIZE}x{FRAME_SIZE}, {total_kb:.1f} KB)")
    print(f"  Durations: {durations}")


if __name__ == "__main__":
    print(f"Converting {GIF_PATH.name} -> {OUTPUT_PATH.parent.resolve()}\n")
    convert()
