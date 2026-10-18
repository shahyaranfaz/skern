#include "console.hpp"

#include "font.hpp"
#include "framebuffer.hpp"

namespace {

using namespace skern;

uint32_t rows;
uint32_t columns;
uint32_t cursor_row;
uint32_t cursor_column;

uint32_t foreground;
uint32_t background;
bool initialized;

uint32_t blend(uint32_t background, uint32_t foreground, uint8_t coverage) {
    uint32_t inverse = 255 - coverage;

    uint32_t red =
        (((background >> 16) & 0xFF) * inverse +
         ((foreground >> 16) & 0xFF) * coverage) /
        255;

    uint32_t green =
        (((background >> 8) & 0xFF) * inverse +
         ((foreground >> 8) & 0xFF) * coverage) /
        255;

    uint32_t blue =
        ((background & 0xFF) * inverse +
         (foreground & 0xFF) * coverage) /
        255;

    return red << 16 | green << 8 | blue;
}

void draw_glyph(uint32_t column, uint32_t row, char ch) {
    const font::Font *font_info = font::get();
    const uint8_t *glyph = font::glyph(ch);

    uint32_t origin_x = column * font_info->width;
    uint32_t origin_y = row * font_info->height;

    for (uint32_t y = 0; y < font_info->height; y++) {
        for (uint32_t x = 0; x < font_info->width; x++) {
            uint8_t coverage = glyph[y * font_info->width + x];

            // Blend foreground and background using coverage.
            uint32_t color = blend(background, foreground, coverage);
            framebuffer::put_pixel(origin_x + x, origin_y + y, color);
        }
    }
}

} // namespace

namespace skern::console {


bool init(const FramebufferInfo *fb) {
    initialized = false;
    if (!framebuffer::init(fb) || !font::init()) return false;

    const font::Font *font_info = font::get();
    if (font_info == nullptr) return false;

    rows = fb->height / font_info->height;
    columns = fb->width / font_info->width;
    if (columns == 0 || rows == 0) return false;

    cursor_column = cursor_row = 0;
    foreground = 0x00FFFFFF;
    background = 0x00000000;
    framebuffer::clear(background);
    initialized = true;
    return true;
}

void put(char ch) {
    if (!initialized) return;

    switch (ch) {
        case '\n':
            cursor_column = 0;
            cursor_row++;
            break;
        case '\r':
            cursor_column = 0;
            break;
        case '\t':
            do {
                put(' ');
            } while (cursor_column % 4 != 0);
            break;
        case '\b':
            if (cursor_column > 0) {
                cursor_column--;
                draw_glyph(cursor_column, cursor_row, ' ');
            }
            break;
        default:
            draw_glyph(cursor_column, cursor_row, ch);
            cursor_column++;

            if (cursor_column >= columns) {
                cursor_column = 0;
                cursor_row++;
            }
    }

    if (cursor_row >= rows) {
        clear(); // Temporary until scrolling exists.
    }
}

void write(const char *text) {
    if (!initialized || text == nullptr) return;

    while (*text != '\0') {
        put(*text);
        text++;
    }
}

void clear() {
    if (!initialized) return;

    framebuffer::clear(background);
    cursor_column = 0;
    cursor_row = 0;
}

} // namespace skern::console
