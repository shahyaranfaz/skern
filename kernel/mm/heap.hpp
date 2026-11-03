#pragma once

#include <cstddef>

namespace skern::heap {

void init();

void *request(size_t bytes);

void release(void *ptr);

} // namespace skern::heap
