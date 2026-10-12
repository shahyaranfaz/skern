#!/usr/bin/env python3
"""Generate skern's fixed-cell, antialiased IBM Plex Mono ASCII font.

Usage: python tools/generate_font.py IBMPlexMono-Regular.ttf
Requires: Pillow. The font file is distributed separately under SIL OFL 1.1.
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
DEFAULT_OUTPUT = Path(__file__).resolve().parents[1] / "kernel" / "console" / "font.hpp"

OFL_NOTICE = '''/* Copyright © 2017 IBM Corp. with Reserved Font Name "Plex"

This Font Software is licensed under the SIL Open Font License, Version 1.1.
This license is copied below, and is also available with a FAQ at:
https://openfontlicense.org


-----------------------------------------------------------
SIL OPEN FONT LICENSE Version 1.1 - 26 February 2007
-----------------------------------------------------------

PREAMBLE
The goals of the Open Font License (OFL) are to stimulate worldwide
development of collaborative font projects, to support the font creation
efforts of academic and linguistic communities, and to provide a free and
open framework in which fonts may be shared and improved in partnership
with others.

The OFL allows the licensed fonts to be used, studied, modified and
redistributed freely as long as they are not sold by themselves. The
fonts, including any derivative works, can be bundled, embedded,
redistributed and/or sold with any software provided that any reserved
names are not used by derivative works. The fonts and derivatives,
however, cannot be released under any other type of license. The
requirement for fonts to remain under this license does not apply
to any document created using the fonts or their derivatives.

DEFINITIONS
"Font Software" refers to the set of files released by the Copyright
Holder(s) under this license and clearly marked as such. This may
include source files, build scripts and documentation.

"Reserved Font Name" refers to any names specified as such after the
copyright statement(s).

"Original Version" refers to the collection of Font Software components as
distributed by the Copyright Holder(s).

"Modified Version" refers to any derivative made by adding to, deleting,
or substituting -- in part or in whole -- any of the components of the
Original Version, by changing formats or by porting the Font Software to a
new environment.

"Author" refers to any designer, engineer, programmer, technical
writer or other person who contributed to the Font Software.

PERMISSION & CONDITIONS
Permission is hereby granted, free of charge, to any person obtaining
a copy of the Font Software, to use, study, copy, merge, embed, modify,
redistribute, and sell modified and unmodified copies of the Font
Software, subject to the following conditions:

1) Neither the Font Software nor any of its individual components,
in Original or Modified Versions, may be sold by itself.

2) Original or Modified Versions of the Font Software may be bundled,
redistributed and/or sold with any software, provided that each copy
contains the above copyright notice and this license. These can be
included either as stand-alone text files, human-readable headers or
in the appropriate machine-readable metadata fields within text or
binary files as long as those fields can be easily viewed by the user.

3) No Modified Version of the Font Software may use the Reserved Font
Name(s) unless explicit written permission is granted by the corresponding
Copyright Holder. This restriction only applies to the primary font name as
presented to the users.

4) The name(s) of the Copyright Holder(s) or the Author(s) of the Font
Software shall not be used to promote, endorse or advertise any
Modified Version, except to acknowledge the contribution(s) of the
Copyright Holder(s) and the Author(s) or with their explicit written
permission.

5) The Font Software, modified or unmodified, in part or in whole,
must be distributed entirely under this license, and must not be
distributed under any other license. The requirement for fonts to
remain under this license does not apply to any document created
using the Font Software.

TERMINATION
This license becomes null and void if any of the above conditions are
not met.

DISCLAIMER
THE FONT SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
OF COPYRIGHT, PATENT, TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL THE
COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
INCLUDING ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL
DAMAGES, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM
OTHER DEALINGS IN THE FONT SOFTWARE.
*/'''


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

    samples = list(result.get_flattened_data())
    assert len(samples) == CELL_W * CELL_H
    assert all(
        samples[y * CELL_W] == 0 and samples[y * CELL_W + CELL_W - 1] == 0
        for y in range(CELL_H)
    ), char
    assert char == " " or any(samples), char
    return samples


def generate(font_path, output):
    font = ImageFont.truetype(str(font_path), FONT_SIZE)
    sha = hashlib.sha256(font_path.read_bytes()).hexdigest()
    glyphs = [rasterize(font, chr(code)) for code in range(FIRST, LAST + 1)]
    assert len(glyphs) == 95

    lines = [
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "// IBM Plex Mono Regular, version 2.3; SIL Open Font License 1.1.",
        f"// Source font SHA-256: {sha}",
        f"// Generated with Pillow {pillow_version}, size {FONT_SIZE}px, grayscale 8-bit.",
        "// Printable ASCII 0x20..0x7E; fixed 16x32 cells with blank edge columns; baseline y=25.",
        "",
        OFL_NOTICE,
        "",
        "namespace skern::font {",
        "",
        f"inline constexpr int WIDTH = {CELL_W};",
        f"inline constexpr int HEIGHT = {CELL_H};",
        f"inline constexpr int BASELINE = {BASELINE};",
        f"inline constexpr int FIRST = {FIRST};",
        f"inline constexpr int LAST = {LAST};",
        "inline constexpr int GLYPH_COUNT = LAST - FIRST + 1;",
        "",
        "inline constexpr uint8_t BITMAP[GLYPH_COUNT][HEIGHT * WIDTH] = {",
    ]

    for code, samples in zip(range(FIRST, LAST + 1), glyphs):
        lines.append(f"    {{ // U+{code:04X} {chr(code)!r}")
        for y in range(CELL_H):
            pixels = samples[y * CELL_W : (y + 1) * CELL_W]
            suffix = "," if y < CELL_H - 1 else ""
            lines.append("        " + ", ".join(f"0x{pixel:02X}" for pixel in pixels) + suffix)
        lines.append("    },")

    lines += [
        "};",
        "",
        "inline constexpr const uint8_t *glyph(char ch) {",
        "    unsigned int c = static_cast<unsigned char>(ch);",
        "    if (c < FIRST || c > LAST) c = static_cast<unsigned int>('?');",
        "    return BITMAP[c - FIRST];",
        "}",
        "} // namespace skern::font",
        "",
    ]

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print(f"Wrote {output} ({len(glyphs)} glyphs, {len(glyphs) * CELL_W * CELL_H:,} raw bytes)")
    print(f"Font SHA-256: {sha}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("font", type=Path)
    parser.add_argument("-o", "--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    generate(args.font, args.output)
