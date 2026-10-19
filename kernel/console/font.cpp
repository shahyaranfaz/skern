#include "font.hpp"

#include <cstddef>
#include <cstdint>

extern "C" uint8_t __skernfont_start[];
extern "C" uint8_t __skernfont_end[];

namespace {

using namespace skern;

enum : uint32_t {
    FONT_WIDTH = 16,
    FONT_HEIGHT = 32,
    FONT_BASELINE = 25,
    FIRST_CHARACTER = 32,
    GLYPH_COUNT = 95,
    BYTES_PER_GLYPH = FONT_WIDTH * FONT_HEIGHT,
    FONT_SIZE = GLYPH_COUNT * BYTES_PER_GLYPH,
};

font::Font embedded_font;
bool initialized;

} // namespace

namespace skern::font {

bool init() {
    size_t size = static_cast<size_t>(__skernfont_end - __skernfont_start);
    if (size != FONT_SIZE) return false;

    embedded_font.width = FONT_WIDTH;
    embedded_font.height = FONT_HEIGHT;
    embedded_font.baseline = FONT_BASELINE;
    embedded_font.first_character = FIRST_CHARACTER;
    embedded_font.glyph_count = GLYPH_COUNT;
    embedded_font.bitmap = __skernfont_start;
    initialized = true;
    return true;
}

const Font *get() {
    if (!initialized) return nullptr;
    return &embedded_font;
}

const uint8_t *glyph(char ch) {
    const Font *font = get();
    if (font == nullptr) return nullptr;

    uint32_t character = static_cast<unsigned char>(ch);
    if (character < font->first_character || character >= font->first_character + font->glyph_count) {
        character = static_cast<uint32_t>('?');
    }
    return font->bitmap + (character - font->first_character) * font->width * font->height;
}

} // namespace skern::font
