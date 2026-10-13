#pragma once

#include <cstdint>

namespace skern::font {

struct Font {
    uint32_t width;
    uint32_t height;
    uint32_t baseline;
    uint32_t first_character;
    uint32_t glyph_count;
    uint8_t *bitmap;
};

bool init();

const Font *get();

const uint8_t *glyph(char ch);

} // namespace skern::font
