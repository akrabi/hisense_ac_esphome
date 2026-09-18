#pragma once

#include <cstddef>
#include <cstdint>
#include "device_status.h"

namespace esphome {
namespace hisense_ac {
namespace protocol {

constexpr size_t MAX_FRAME_SIZE = 255 + 9;
constexpr size_t STATUS_FRAME_SIZE = 82;
constexpr size_t EXTENDED_STATUS_FRAME_SIZE = 160;
// Software recovery threshold, not a hardware-verified bus timing constraint.
constexpr uint32_t INTER_BYTE_TIMEOUT_MS = 100;

class FrameParser {
public:
    // Returns a complete decoded frame length, otherwise zero. data() remains
    // valid until the next feed(); this parser never acknowledges commands.
    size_t feed(uint8_t byte, uint32_t now);
    const uint8_t *data() const { return buffer_; }
    void reset();
    void expire(uint32_t now);

private:
    void restart_(uint8_t byte);
    uint8_t buffer_[MAX_FRAME_SIZE]{};
    size_t size_{0};
    size_t expected_size_{0};
    bool escape_pending_{false};
    uint32_t last_byte_at_{0};
};

// Validates the envelope, checksum, class and evidenced 82/160-byte layouts.
// Decodes only the shared status prefix; the remaining payload is opaque.
// On failure, leaves the caller's last decoded snapshot unchanged.
bool decode_status(const uint8_t *frame, size_t size, DeviceStatus &status);

}  // namespace protocol
}  // namespace hisense_ac
}  // namespace esphome
