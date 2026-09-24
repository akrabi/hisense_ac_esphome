#pragma once

#include <cstdint>

namespace esphome {
namespace hisense_ac {

// Auto reports as 1 on the captured five-speed unit; 0 is the legacy mapping.
// Preserve other raw values for exact command confirmation.
constexpr uint8_t normalize_fan_status(uint8_t raw) {
    return raw == 1 ? 0 : raw;
}

// Decoded values, not a wire layout. Only fields consumed by this component
// are mapped; unused flags in the former packed struct remain unverified.
struct DeviceStatus {
    uint8_t wind_status{0};
    uint8_t run_status{0};
    uint8_t mode_status{0};
    uint8_t indoor_temperature_setting{0};
    uint8_t temperature_compensation_raw{0};  // Byte 26; signed upper nibble applies only in Dry.
    uint8_t indoor_temperature_status{0};
    uint8_t indoor_pipe_temperature{0};
    int16_t indoor_humidity_setting{0};
    int16_t indoor_humidity_status{0};
    bool left_right{false};
    bool up_down{false};
    bool back_led{false};
    uint8_t compressor_frequency{0};
    uint8_t compressor_frequency_setting{0};
    uint8_t compressor_frequency_send{0};
    int16_t outdoor_temperature{0};
    int16_t outdoor_condenser_temperature{0};
    int16_t compressor_exhaust_temperature{0};
    int16_t target_exhaust_temperature{0};
};

}  // namespace hisense_ac
}  // namespace esphome
