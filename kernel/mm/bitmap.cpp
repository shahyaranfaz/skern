#include "bitmap.hpp"

namespace skern {

bool Bitmap::get(uint64_t index) const {
    if (index >= bit_count) return false;
    return data[index / 8] & (1U << (index % 8));
}

void Bitmap::set(uint64_t index) const {
    if (index >= bit_count) return;
    data[index / 8] |= 1U << (index % 8);
}

void Bitmap::unset(uint64_t index) const {
    if (index >= bit_count) return;
    data[index / 8] &= ~(1U << (index % 8));
}

void Bitmap::clear() const {
    uint64_t byte_count = bit_count / 8 + (bit_count % 8 != 0);
    for (uint64_t i = 0; i < byte_count; i++) {
        data[i] = 0;
    }
}

} // namespace skern
