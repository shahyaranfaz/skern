#pragma once

#include <stdint.h>

#define SKERN_BOOT_INFO_MAGIC UINT64_C(0x7961796E72656B73) // "skernyay" in little-endian
#define SKERN_BOOT_INFO_VERSION 2

typedef struct {
    uint64_t base;
    uint64_t size;
} BootstrapStackInfo;

typedef struct {
    uint64_t address;
    uint64_t byte_size;
    uint32_t width;
    uint32_t height;
    uint32_t pixels_per_scanline;
    uint32_t pixel_format;
} FramebufferInfo;

typedef struct {
    uint64_t descriptors;
    uint64_t byte_size;
    uint64_t descriptor_size;
    uint32_t descriptor_version;
    uint32_t reserved;
} MemoryMapInfo;

typedef struct {
    uint64_t magic;
    uint32_t version;
    uint32_t size;

    MemoryMapInfo memory_map;
    FramebufferInfo framebuffer;
    BootstrapStackInfo bootstrap_stack;

    uint64_t rsdp;
} BootInfo;
