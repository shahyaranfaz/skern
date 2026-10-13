#!/usr/bin/env python3
"""Generate skern's embedded early-console font.

Usage: python tools/generate_font.py IBMPlexMono-Regular.ttf
Requires: Pillow. The source font is distributed under SIL OFL 1.1.
"""

import argparse
import hashlib
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, __version__ as pillow_version


CELL_W, CELL_H = 16, 32
BASELINE = 25
INNER_W = CELL_W - 2
FONT_SIZE = 24
FIRST, LAST = 32, 126
DEFAULT_OUTPUT = (
    Path(__file__).resolve().parents[1] / "kernel" / "console" / "font" / ".skernfont"
)


def rasterize(font, char):
    work = Image.new("L", (64, CELL_H), 0)
    ImageDraw.Draw(work).text((16, BASELINE), char, font=font, fill=255, anchor="ls")
    bbox = work.getbbox()
    result = Image.new("L", (CELL_W, CELL_H), 0)

    if bbox is None:
        if char != " ":
            raise ValueError(f"Non-space glyph is empty: {char!r}")
    else:
        left, _, right, _ = bbox
        glyph = work.crop((left, 0, right, CELL_H))
        if glyph.width > INNER_W:
            glyph = glyph.resize((INNER_W, CELL_H), Image.Resampling.LANCZOS)
        x = 1 + (INNER_W - glyph.width) // 2
        result.paste(glyph, (x, 0))

    samples = bytes(result.get_flattened_data())
    assert len(samples) == CELL_W * CELL_H
    assert all(
        samples[y * CELL_W] == 0 and samples[y * CELL_W + CELL_W - 1] == 0
        for y in range(CELL_H)
    ), char
    assert char == " " or any(samples), char
    return samples


def generate(font_path, output):
    font = ImageFont.truetype(str(font_path), FONT_SIZE)
    source_hash = hashlib.sha256(font_path.read_bytes()).hexdigest()
    glyphs = [rasterize(font, chr(code)) for code in range(FIRST, LAST + 1)]
    bitmap = b"".join(glyphs)

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(bitmap)
    print(f"Wrote {output} ({len(bitmap):,} bytes)")
    print(f"IBM Plex Mono Regular 2.3; generated with Pillow {pillow_version}")
    print(f"Source font SHA-256: {source_hash}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("font", type=Path)
    parser.add_argument("-o", "--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    generate(args.font, args.output)
