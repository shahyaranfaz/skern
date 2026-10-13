#pragma once

#include <skern/boot_info.h>

namespace skern::framebuffer {

bool init(const FramebufferInfo *info);

bool put_pixel(uint32_t x, uint32_t y, uint32_t color);

void clear(uint32_t color);

} // namespace skern::framebuffer
