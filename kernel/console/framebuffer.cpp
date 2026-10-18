#include "framebuffer.hpp"

#include <skern/boot_info.h>

namespace {

FramebufferInfo fb;
volatile uint32_t *pixels;

uint32_t encode_color(uint32_t color) {
    if (fb.pixel_format == 1) return color;

    uint32_t red = (color >> 16) & 0xFF;
    uint32_t green = color & 0x0000FF00;
    uint32_t blue = color & 0xFF;
    return blue << 16 | green | red;
}

} // namespace

namespace skern::framebuffer {

bool init(const FramebufferInfo *info) {
    if (info == nullptr) return false;
    if (info->address == 0 || info->byte_size == 0) return false;
    if (info->width == 0 || info->height == 0) return false;
    if (info->pixels_per_scanline < info->width) return false;

    // UEFI: 0 = RGB, 1 = BGR. BitMask and BltOnly are not supported yet.
    if (info->pixel_format > 1) return false;

    if (info->address % alignof(uint32_t) != 0) return false;

    uint64_t scanline_size = static_cast<uint64_t>(info->pixels_per_scanline) * sizeof(uint32_t);
    if (info->height > UINT64_MAX / scanline_size) return false;

    uint64_t required_size = scanline_size * info->height;
    if (required_size > info->byte_size) return false;

    if (info->address > UINT64_MAX - info->byte_size) return false;

    fb = *info;
    pixels = reinterpret_cast<volatile uint32_t *>(fb.address);
    return true;
}

bool put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb.width || y >= fb.height) return false;

    uint64_t index = static_cast<uint64_t>(y) * fb.pixels_per_scanline + x;
    pixels[index] = encode_color(color);
    return true;
}

bool copy_rows(uint32_t destination_y, uint32_t source_y, uint32_t count) {
    if (source_y > fb.height || count > fb.height - source_y) return false;
    if (destination_y > fb.height || count > fb.height - destination_y) return false;

    for (uint32_t row = 0; row < count; row++) {
        uint64_t source_index = static_cast<uint64_t>(source_y + row) * fb.pixels_per_scanline;
        uint64_t destination_index = static_cast<uint64_t>(destination_y + row) * fb.pixels_per_scanline;

        for (uint32_t x = 0; x < fb.width; x++) {
            pixels[destination_index + x] = pixels[source_index + x];
        }
    }

    return true;
}

bool clear_rows(uint32_t y, uint32_t count, uint32_t color) {
    if (y > fb.height || count > fb.height - y) return false;

    uint32_t encoded_color = encode_color(color);

    for (uint32_t row = 0; row < count; row++) {
        uint64_t index = static_cast<uint64_t>(y + row) * fb.pixels_per_scanline;

        for (uint32_t x = 0; x < fb.width; x++) {
            pixels[index + x] = encoded_color;
        }
    }

    return true;
}

void clear(uint32_t color) {
    uint32_t encoded_color = encode_color(color);

    for (uint32_t y = 0; y < fb.height; y++) {
        volatile uint32_t *row = pixels + static_cast<uint64_t>(y) * fb.pixels_per_scanline;
        for (uint32_t x = 0; x < fb.width; x++) {
            row[x] = encoded_color;
        }
    }
}

} // namespace skern::framebuffer
