#pragma once
#include "esphome/testing.h"

namespace esphome {
inline char *format_hex_pretty_to(char *buffer, size_t capacity, const uint8_t *data, size_t size, char separator) {
    if (size == 0 || capacity < size * 3) std::abort();
    for (size_t i = 0; i < size; ++i) {
        std::snprintf(buffer + i * 3, 3, "%02X", static_cast<unsigned>(data[i]));
        if (i + 1 < size) buffer[i * 3 + 2] = separator;
    }
    return buffer;
}
}
