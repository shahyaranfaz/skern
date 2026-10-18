#pragma once

#include <skern/boot_info.h>

namespace skern::framebuffer {

bool init(const FramebufferInfo *info);

bool put_pixel(uint32_t x, uint32_t y, uint32_t color);

bool copy_rows(uint32_t destination_y, uint32_t source_y, uint32_t count);

bool clear_rows(uint32_t y, uint32_t count, uint32_t color);

void clear(uint32_t color);

} // namespace skern::framebuffer
