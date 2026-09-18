#include "protocol.h"

namespace esphome {
namespace hisense_ac {
namespace protocol {
namespace {

constexpr uint8_t MARKER = 0xF4;
constexpr uint8_t HEADER_END = 0xF5;
constexpr uint8_t FOOTER_END = 0xFB;
constexpr uint8_t RESPONSE = 0x01;
constexpr uint8_t CONTROL = 0x40;
constexpr size_t ENVELOPE_SIZE = 9;
constexpr size_t CLASS_OFFSET = 13;
constexpr uint8_t STATUS_CLASS = 0x66;

enum StatusOffset : size_t {
    WIND = 16,
    MODE_RUN = 18,
    SETPOINT = 19,
    ROOM_TEMPERATURE = 20,
    PIPE_TEMPERATURE = 21,
    HUMIDITY_SETTING = 22,
    HUMIDITY = 23,
    SWING = 35,
    DISPLAY = 37,
    COMPRESSOR_FREQUENCY = 41,
    COMPRESSOR_SETTING = 42,
    COMPRESSOR_SEND = 43,
    OUTDOOR_TEMPERATURE = 44,
    CONDENSER_TEMPERATURE = 45,
    EXHAUST_TEMPERATURE = 46,
    TARGET_EXHAUST_TEMPERATURE = 47,
};
constexpr uint8_t RUN_MASK = 0x0C;
constexpr uint8_t HORIZONTAL_SWING_MASK = 0x40;
constexpr uint8_t VERTICAL_SWING_MASK = 0x80;
constexpr uint8_t BACK_LED_MASK = 0x80;

uint16_t read_be16(const uint8_t *bytes) {
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]);
}

int16_t signed_byte(uint8_t byte) {
    return byte < 0x80 ? byte : static_cast<int16_t>(byte) - 256;
}

bool valid_frame(const uint8_t *frame, size_t size) {
    if (frame == nullptr || size < ENVELOPE_SIZE || size > MAX_FRAME_SIZE ||
        frame[0] != MARKER || frame[1] != HEADER_END ||
        frame[2] != RESPONSE || frame[3] != CONTROL ||
        static_cast<size_t>(frame[4]) + ENVELOPE_SIZE != size ||
        frame[size - 2] != MARKER || frame[size - 1] != FOOTER_END)
        return false;

    uint16_t checksum = 0;
    for (size_t i = 2; i < size - 4; ++i)
        checksum += frame[i];
    return checksum == read_be16(frame + size - 4);
}

}  // namespace

void FrameParser::reset() {
    size_ = 0;
    expected_size_ = 0;
    escape_pending_ = false;
    last_byte_at_ = 0;
}

void FrameParser::expire(uint32_t now) {
    if (size_ != 0 && static_cast<uint32_t>(now - last_byte_at_) >= INTER_BYTE_TIMEOUT_MS)
        reset();
}

void FrameParser::restart_(uint8_t byte) {
    reset();
    if (byte == MARKER)
        buffer_[size_++] = byte;
}

size_t FrameParser::feed(uint8_t byte, uint32_t now) {
    expire(now);
    if (size_ == 0) {
        restart_(byte);
    } else if (size_ == 1) {
        if (byte == HEADER_END)
            buffer_[size_++] = byte;
        else
            restart_(byte);
    } else if (expected_size_ != 0 && size_ >= expected_size_ - 2) {
        const uint8_t expected = size_ == expected_size_ - 2 ? MARKER : FOOTER_END;
        if (byte != expected) {
            // A new header can overlap the rejected footer.
            const bool new_header = size_ == expected_size_ - 1 && byte == HEADER_END;
            restart_(byte);
            if (new_header) {
                buffer_[0] = MARKER;
                buffer_[1] = HEADER_END;
                size_ = 2;
            }
        } else {
            buffer_[size_++] = byte;
            if (size_ == expected_size_) {
                const size_t complete_size = size_;
                const bool valid = valid_frame(buffer_, complete_size);
                reset();
                return valid ? complete_size : 0;
            }
        }
    } else {
        bool decoded = true;
        if (escape_pending_) {
            escape_pending_ = false;
            if (byte != MARKER) {
                restart_(byte);
                if (byte == HEADER_END) {
                    buffer_[0] = MARKER;
                    buffer_[1] = HEADER_END;
                    size_ = 2;
                }
                decoded = false;
            }
        } else if (byte == MARKER) {
            escape_pending_ = true;
            decoded = false;
        }

        if (decoded) {
            if (size_ >= MAX_FRAME_SIZE) {
                restart_(byte);
            } else {
                buffer_[size_++] = byte;
                if ((size_ == 3 && byte != RESPONSE) || (size_ == 4 && byte != CONTROL)) {
                    restart_(byte);
                } else if (size_ == 5) {
                    expected_size_ = static_cast<size_t>(byte) + ENVELOPE_SIZE;
                    if (expected_size_ > MAX_FRAME_SIZE)
                        restart_(byte);
                }
            }
        }
    }
    last_byte_at_ = now;
    return 0;
}

bool decode_status(const uint8_t *frame, size_t size, DeviceStatus &status) {
    if ((size != STATUS_FRAME_SIZE && size != EXTENDED_STATUS_FRAME_SIZE) ||
        !valid_frame(frame, size) ||
        frame[CLASS_OFFSET] != STATUS_CLASS)
        return false;

    DeviceStatus decoded;
    decoded.wind_status = frame[WIND];
    decoded.run_status = (frame[MODE_RUN] & RUN_MASK) >> 2;
    decoded.mode_status = frame[MODE_RUN] >> 4;
    decoded.indoor_temperature_setting = frame[SETPOINT];
    decoded.indoor_temperature_status = frame[ROOM_TEMPERATURE];
    decoded.indoor_pipe_temperature = frame[PIPE_TEMPERATURE];
    decoded.indoor_humidity_setting = signed_byte(frame[HUMIDITY_SETTING]);
    decoded.indoor_humidity_status = signed_byte(frame[HUMIDITY]);
    decoded.left_right = (frame[SWING] & HORIZONTAL_SWING_MASK) != 0;
    decoded.up_down = (frame[SWING] & VERTICAL_SWING_MASK) != 0;
    decoded.back_led = (frame[DISPLAY] & BACK_LED_MASK) != 0;
    decoded.compressor_frequency = frame[COMPRESSOR_FREQUENCY];
    decoded.compressor_frequency_setting = frame[COMPRESSOR_SETTING];
    decoded.compressor_frequency_send = frame[COMPRESSOR_SEND];
    decoded.outdoor_temperature = signed_byte(frame[OUTDOOR_TEMPERATURE]);
    decoded.outdoor_condenser_temperature = signed_byte(frame[CONDENSER_TEMPERATURE]);
    decoded.compressor_exhaust_temperature = signed_byte(frame[EXHAUST_TEMPERATURE]);
    decoded.target_exhaust_temperature = signed_byte(frame[TARGET_EXHAUST_TEMPERATURE]);
    status = decoded;
    return true;
}

}  // namespace protocol
}  // namespace hisense_ac
}  // namespace esphome
