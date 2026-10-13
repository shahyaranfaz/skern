#include "framebuffer.hpp"

#include <skern/boot_info.h>

namespace {

FramebufferInfo fb;
volatile uint32_t *pixels;

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
    pixels[index] = color;
    return true;
}

void clear(uint32_t color) {
    for (uint32_t y = 0; y < fb.height; y++) {
        volatile uint32_t *row = pixels + static_cast<uint64_t>(y) * fb.pixels_per_scanline;
        for (uint32_t x = 0; x < fb.width; x++) {
            row[x] = color;
        }
    }
}

} // namespace skern::framebuffer
