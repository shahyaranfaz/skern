import unittest

from common import ROOT


FONT_PATH = ROOT / "kernel" / "console" / "font" / ".skernfont"
WIDTH = 16
HEIGHT = 32
GLYPH_COUNT = 95
BYTES_PER_GLYPH = WIDTH * HEIGHT


class SkernFontTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.data = FONT_PATH.read_bytes()

    def test_contains_one_bitmap_for_each_printable_ascii_character(self) -> None:
        self.assertEqual(len(self.data), GLYPH_COUNT * BYTES_PER_GLYPH)

    def test_glyph_cells_have_blank_edge_columns(self) -> None:
        for glyph_index in range(GLYPH_COUNT):
            glyph = self.data[
                glyph_index * BYTES_PER_GLYPH : (glyph_index + 1) * BYTES_PER_GLYPH
            ]
            for row in range(HEIGHT):
                self.assertEqual(glyph[row * WIDTH], 0)
                self.assertEqual(glyph[row * WIDTH + WIDTH - 1], 0)


if __name__ == "__main__":
    unittest.main()
