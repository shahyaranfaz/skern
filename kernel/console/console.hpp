#pragma once

#include <skern/boot_info.h>

namespace skern::console {

bool init(const FramebufferInfo *fb);

void put(char ch);

void write(const char *text);

void clear();

} // namespace skern::console
