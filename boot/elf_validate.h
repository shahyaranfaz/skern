#pragma once

#include <stdbool.h>
#include <stddef.h>

bool elf_validate(const void *file, size_t file_size);
