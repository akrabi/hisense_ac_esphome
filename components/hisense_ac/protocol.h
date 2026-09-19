#pragma once

#include <cstddef>
#include <cstdint>
#include "device_status.h"

namespace esphome {
namespace hisense_ac {
namespace protocol {

constexpr size_t MAX_FRAME_SIZE = 128;
constexpr size_t STATUS_FRAME_SIZE = 82;
// Software recovery threshold, not a hardware-verified bus timing constraint.
constexpr uint32_t INTER_BYTE_TIMEOUT_MS = 100;
enum class ParseError { NONE, HEADER, LENGTH, ESCAPE, FOOTER, CHECKSUM, TIMEOUT };

class FrameParser {
public:
    // Returns a complete decoded frame length, otherwise zero. data() remains
    // valid until the next feed(); this parser never acknowledges commands.
    size_t feed(uint8_t byte, uint32_t now);
    const uint8_t *data() const { return buffer_; }
    void reset();
    void expire(uint32_t now);
    uint32_t invalid_frames() const { return invalid_frames_; }
    const char *last_error_name() const;

private:
    void restart_(uint8_t byte);
    void reject_(ParseError error);
    uint8_t buffer_[MAX_FRAME_SIZE]{};
    size_t size_{0};
    size_t expected_size_{0};
    bool escape_pending_{false};
    uint32_t last_byte_at_{0};
    uint32_t invalid_frames_{0};
    ParseError last_error_{ParseError::NONE};
};

// Validates the envelope, checksum, class and supported 82-byte status layout.
// Decodes only consumed fields; the remaining payload is opaque.
// On failure, leaves the caller's last decoded snapshot unchanged.
bool decode_status(const uint8_t *frame, size_t size, DeviceStatus &status);

}  // namespace protocol
}  // namespace hisense_ac
}  // namespace esphome
