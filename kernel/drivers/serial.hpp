#pragma once

#include <cstddef>

namespace skern::serial {

bool init();

bool write(char c);

bool write(const char *str);

bool write(const char *data, size_t size);

} // namespace skern::serial
