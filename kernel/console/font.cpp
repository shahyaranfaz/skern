#include "font.hpp"

#include <cstddef>
#include <cstdint>

extern "C" uint8_t __skernfont_start[];
extern "C" uint8_t __skernfont_end[];

namespace {

using namespace skern;

enum : uint32_t {
    font_width = 16,
    font_height = 32,
    font_baseline = 25,
    first_character = 32,
    glyph_count = 95,
    bytes_per_glyph = font_width * font_height,
    font_size = glyph_count * bytes_per_glyph,
};

font::Font embedded_font;
bool initialized;

} // namespace

namespace skern::font {

bool init() {
    size_t size = static_cast<size_t>(__skernfont_end - __skernfont_start);
    if (size != font_size) return false;

    embedded_font.width = font_width;
    embedded_font.height = font_height;
    embedded_font.baseline = font_baseline;
    embedded_font.first_character = first_character;
    embedded_font.glyph_count = glyph_count;
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
    if (character < font->first_character ||
        character >= font->first_character + font->glyph_count) {
        character = static_cast<uint32_t>('?');
    }
    return font->bitmap + (character - font->first_character) * font->width * font->height;
}

} // namespace skern::font
