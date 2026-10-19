#include "console/console.hpp"
#include "console/font.hpp"
#include "console/framebuffer.hpp"

#include <skern/boot_info.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

enum : uint32_t {
    FONT_WIDTH = 2,
    FONT_HEIGHT = 2,
    FIRST_CHARACTER = 32,
    GLYPH_COUNT = 95,
    PIXELS_PER_GLYPH = FONT_WIDTH * FONT_HEIGHT,
};

uint8_t font_bitmap[GLYPH_COUNT * PIXELS_PER_GLYPH];
skern::font::Font test_font;

bool expect(bool condition, const char *message) {
    if (condition) return true;
    std::fprintf(stderr, "%s\n", message);
    return false;
}

FramebufferInfo make_framebuffer(std::vector<uint32_t> &pixels, uint32_t width, uint32_t height,
                                 uint32_t stride, uint32_t format = 1) {
    FramebufferInfo info{};
    info.address = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pixels.data()));
    info.byte_size = static_cast<uint64_t>(pixels.size()) * sizeof(uint32_t);
    info.width = width;
    info.height = height;
    info.pixels_per_scanline = stride;
    info.pixel_format = format;
    return info;
}

uint32_t gray(char ch) {
    uint32_t value = static_cast<unsigned char>(ch);
    return value << 16 | value << 8 | value;
}

bool framebuffer_rejects_invalid_information() {
    std::vector<uint32_t> pixels(16);
    FramebufferInfo info = make_framebuffer(pixels, 4, 4, 4);

    if (!expect(!skern::framebuffer::init(nullptr), "accepted a null framebuffer")) return false;

    info.address = 0;
    if (!expect(!skern::framebuffer::init(&info), "accepted address zero")) return false;

    info = make_framebuffer(pixels, 4, 4, 4);
    info.byte_size = 0;
    if (!expect(!skern::framebuffer::init(&info), "accepted size zero")) return false;

    info = make_framebuffer(pixels, 0, 4, 4);
    if (!expect(!skern::framebuffer::init(&info), "accepted zero width")) return false;

    info = make_framebuffer(pixels, 4, 0, 4);
    if (!expect(!skern::framebuffer::init(&info), "accepted zero height")) return false;

    info = make_framebuffer(pixels, 4, 4, 3);
    if (!expect(!skern::framebuffer::init(&info), "accepted a short scanline")) return false;

    info = make_framebuffer(pixels, 4, 4, 4, 2);
    if (!expect(!skern::framebuffer::init(&info), "accepted an unsupported pixel format"))
        return false;

    info = make_framebuffer(pixels, 4, 4, 4);
    info.address++;
    if (!expect(!skern::framebuffer::init(&info), "accepted an unaligned address")) return false;

    info = make_framebuffer(pixels, 4, 4, 4);
    info.byte_size--;
    if (!expect(!skern::framebuffer::init(&info), "accepted an undersized framebuffer"))
        return false;

    info = make_framebuffer(pixels, 1, 1, 1);
    info.address = UINT64_MAX - 3;
    info.byte_size = 8;
    return expect(!skern::framebuffer::init(&info), "accepted an overflowing address range");
}

bool framebuffer_encodes_colors_and_checks_pixel_bounds() {
    std::vector<uint32_t> pixels(4, 0xDEADBEEF);
    FramebufferInfo info = make_framebuffer(pixels, 2, 2, 2, 1);
    if (!expect(skern::framebuffer::init(&info), "BGR framebuffer initialization failed"))
        return false;
    if (!expect(skern::framebuffer::put_pixel(1, 0, 0x00112233), "valid pixel was rejected"))
        return false;
    if (!expect(pixels[1] == 0x00112233, "BGR color was encoded incorrectly")) return false;
    if (!expect(!skern::framebuffer::put_pixel(2, 0, 0), "accepted x at the width bound"))
        return false;
    if (!expect(!skern::framebuffer::put_pixel(0, 2, 0), "accepted y at the height bound"))
        return false;

    std::fill(pixels.begin(), pixels.end(), 0);
    info = make_framebuffer(pixels, 2, 2, 2, 0);
    if (!expect(skern::framebuffer::init(&info), "RGB framebuffer initialization failed"))
        return false;
    if (!expect(skern::framebuffer::put_pixel(0, 0, 0x00112233), "RGB pixel write failed"))
        return false;
    if (!expect(pixels[0] == 0x00332211, "RGB color did not swap red and blue")) return false;

    skern::framebuffer::clear(0x00112233);
    return expect(std::all_of(pixels.begin(), pixels.end(),
                              [](uint32_t pixel) { return pixel == 0x00332211; }),
                  "framebuffer clear did not encode RGB color");
}

bool framebuffer_copies_and_clears_rows() {
    uint32_t marker = 0xDEADBEEF;
    std::vector<uint32_t> pixels(20, marker);
    FramebufferInfo info = make_framebuffer(pixels, 3, 4, 5);
    if (!expect(skern::framebuffer::init(&info), "framebuffer initialization failed")) return false;

    for (uint32_t y = 0; y < 4; y++) {
        for (uint32_t x = 0; x < 3; x++) {
            pixels[y * 5 + x] = (y + 1) * 10 + x;
        }
    }

    if (!expect(skern::framebuffer::copy_rows(0, 1, 3), "valid row copy failed")) return false;
    for (uint32_t y = 0; y < 3; y++) {
        for (uint32_t x = 0; x < 3; x++) {
            if (!expect(pixels[y * 5 + x] == (y + 2) * 10 + x,
                        "row copy produced the wrong pixel")) {
                return false;
            }
        }
    }

    if (!expect(pixels[3] == marker && pixels[4] == marker, "row copy changed scanline padding"))
        return false;
    if (!expect(!skern::framebuffer::copy_rows(0, 4, 1),
                "accepted a source row at the height bound")) {
        return false;
    }
    if (!expect(!skern::framebuffer::copy_rows(3, 0, 2),
                "accepted an overflowing destination range")) {
        return false;
    }

    if (!expect(skern::framebuffer::clear_rows(2, 2, 0x00123456), "valid row clear failed"))
        return false;
    for (uint32_t y = 2; y < 4; y++) {
        for (uint32_t x = 0; x < 3; x++) {
            if (!expect(pixels[y * 5 + x] == 0x00123456, "row clear produced the wrong pixel")) {
                return false;
            }
        }
    }

    if (!expect(pixels[13] == marker && pixels[14] == marker, "row clear changed scanline padding"))
        return false;
    return expect(!skern::framebuffer::clear_rows(4, 1, 0),
                  "accepted a clear row at the height bound");
}

bool console_is_safe_before_initialization() {
    skern::console::put('A');
    skern::console::write("hello");
    skern::console::write(nullptr);
    skern::console::clear();
    return true;
}

bool console_renders_glyphs_and_wraps() {
    std::vector<uint32_t> pixels(16, 0xDEADBEEF);
    FramebufferInfo info = make_framebuffer(pixels, 4, 4, 4);
    if (!expect(skern::console::init(&info), "console initialization failed")) return false;

    skern::console::write("ABC");
    if (!expect(pixels[0] == gray('A'), "first glyph was not rendered in the first cell"))
        return false;
    if (!expect(pixels[2] == gray('B'), "second glyph was not rendered in the second cell"))
        return false;
    if (!expect(pixels[8] == gray('C'), "third glyph did not wrap to the next row")) return false;
    return expect(pixels[10] == 0, "unused glyph cell was not left blank");
}

bool console_handles_control_characters() {
    std::vector<uint32_t> pixels(64);
    FramebufferInfo info = make_framebuffer(pixels, 16, 4, 16);
    if (!expect(skern::console::init(&info), "console initialization failed")) return false;

    skern::console::write("A\tB");
    if (!expect(pixels[0] == gray('A'), "tab test did not render its first glyph")) return false;
    if (!expect(pixels[8] == gray('B'), "tab did not advance to the next four-column stop"))
        return false;

    if (!expect(skern::console::init(&info), "console reinitialization failed")) return false;
    skern::console::write("A\rB");
    if (!expect(pixels[0] == gray('B'), "carriage return did not return to column zero"))
        return false;
    if (!expect(pixels[2] == 0, "carriage return unexpectedly advanced a column")) return false;

    if (!expect(skern::console::init(&info), "console reinitialization failed")) return false;
    skern::console::write("A\nB");
    if (!expect(pixels[32] == gray('B'), "newline did not advance to the next row")) return false;

    if (!expect(skern::console::init(&info), "console reinitialization failed")) return false;
    skern::console::write("AB\bC");
    if (!expect(pixels[2] == gray('C'), "backspace did not erase and reuse the previous cell"))
        return false;
    return expect(pixels[4] == 0, "backspace unexpectedly advanced the cursor");
}

bool console_scrolls_one_text_row() {
    std::vector<uint32_t> pixels(24);
    FramebufferInfo info = make_framebuffer(pixels, 4, 6, 4);
    if (!expect(skern::console::init(&info), "console initialization failed")) return false;

    skern::console::write("A\nB\nC\nD");
    if (!expect(pixels[0] == gray('B'), "scroll did not move the second line to the top"))
        return false;
    if (!expect(pixels[8] == gray('C'), "scroll did not move the third line upward")) return false;
    if (!expect(pixels[16] == gray('D'), "scroll did not render into the cleared bottom row"))
        return false;
    return expect(pixels[18] == 0, "scroll did not clear the unused bottom cell");
}

} // namespace

namespace skern::font {

bool init() {
    for (uint32_t character = FIRST_CHARACTER; character < FIRST_CHARACTER + GLYPH_COUNT;
         character++) {
        uint8_t coverage =
            character == static_cast<uint32_t>(' ') ? 0 : static_cast<uint8_t>(character);
        uint32_t offset = (character - FIRST_CHARACTER) * PIXELS_PER_GLYPH;
        for (uint32_t pixel = 0; pixel < PIXELS_PER_GLYPH; pixel++) {
            font_bitmap[offset + pixel] = coverage;
        }
    }

    test_font.width = FONT_WIDTH;
    test_font.height = FONT_HEIGHT;
    test_font.baseline = 1;
    test_font.first_character = FIRST_CHARACTER;
    test_font.glyph_count = GLYPH_COUNT;
    test_font.bitmap = font_bitmap;
    return true;
}

const Font *get() {
    return &test_font;
}

const uint8_t *glyph(char ch) {
    uint32_t character = static_cast<unsigned char>(ch);
    if (character < FIRST_CHARACTER || character >= FIRST_CHARACTER + GLYPH_COUNT) {
        character = static_cast<uint32_t>('?');
    }
    return font_bitmap + (character - FIRST_CHARACTER) * PIXELS_PER_GLYPH;
}

} // namespace skern::font

int main(int argc, char **argv) {
    if (argc != 2) return 2;

    if (std::strcmp(argv[1], "framebuffer-invalid") == 0) {
        return framebuffer_rejects_invalid_information() ? 0 : 1;
    }
    if (std::strcmp(argv[1], "framebuffer-colors") == 0) {
        return framebuffer_encodes_colors_and_checks_pixel_bounds() ? 0 : 1;
    }
    if (std::strcmp(argv[1], "framebuffer-rows") == 0) {
        return framebuffer_copies_and_clears_rows() ? 0 : 1;
    }
    if (std::strcmp(argv[1], "console-uninitialized") == 0) {
        return console_is_safe_before_initialization() ? 0 : 1;
    }
    if (std::strcmp(argv[1], "console-render-wrap") == 0) {
        return console_renders_glyphs_and_wraps() ? 0 : 1;
    }
    if (std::strcmp(argv[1], "console-controls") == 0) {
        return console_handles_control_characters() ? 0 : 1;
    }
    if (std::strcmp(argv[1], "console-scroll") == 0) {
        return console_scrolls_one_text_row() ? 0 : 1;
    }

    return 2;
}
