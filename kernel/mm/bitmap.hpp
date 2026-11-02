#pragma once

#include <cstdint>

namespace skern {
struct Bitmap {
    uint8_t *data;
    uint64_t bit_count;

    bool get(uint64_t index) const;
    void set(uint64_t index) const;
    void unset(uint64_t index) const;
    void clear() const;
};

} // namespace skern
