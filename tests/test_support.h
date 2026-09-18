#pragma once
#include "protocol.h"
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <vector>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        std::exit(1); \
    } \
} while (false)

using Bytes = std::vector<uint8_t>;
using esphome::hisense_ac::DeviceStatus;
namespace protocol = esphome::hisense_ac::protocol;

inline Bytes capture() {
    std::ifstream input(FIXTURE_PATH);
    CHECK(input.good());
    Bytes result;
    unsigned byte;
    while (input >> std::hex >> byte)
        result.push_back(static_cast<uint8_t>(byte));
    CHECK(result.size() == 82);
    return result;
}

// Test-side construction of synthetic frames, never a replacement decoder.
inline void seal(Bytes &decoded) {
    CHECK(decoded.size() >= 9);
    decoded[4] = static_cast<uint8_t>(decoded.size() - 9);
    unsigned sum = 0;
    for (size_t i = 2; i + 4 < decoded.size(); ++i)
        sum += decoded[i];
    decoded[decoded.size() - 4] = static_cast<uint8_t>(sum >> 8);
    decoded[decoded.size() - 3] = static_cast<uint8_t>(sum);
    decoded[decoded.size() - 2] = 0xF4;
    decoded[decoded.size() - 1] = 0xFB;
}

inline Bytes synthetic(size_t size = 82) {
    Bytes result(size, 0);
    result[0] = 0xF4;
    result[1] = 0xF5;
    result[2] = 1;
    result[3] = 0x40;
    if (size >= 18)
        result[13] = 0x66;
    seal(result);
    return result;
}

inline Bytes wire(const Bytes &decoded) {
    Bytes result;
    for (size_t i = 0; i < decoded.size(); ++i) {
        result.push_back(decoded[i]);
        if (i >= 2 && i + 2 < decoded.size() && decoded[i] == 0xF4)
            result.push_back(0xF4);
    }
    return result;
}

inline unsigned feed(protocol::FrameParser &parser, const Bytes &bytes, uint32_t &now) {
    unsigned count = 0;
    for (uint8_t byte : bytes) {
        if (parser.feed(byte, now++) != 0)
            ++count;
    }
    return count;
}
