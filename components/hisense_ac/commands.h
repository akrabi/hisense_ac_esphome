#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome {
namespace hisense_ac {

constexpr uint8_t CMD_SIZE = 50;
constexpr size_t MAX_COMMAND_WIRE_SIZE = 64;

struct CommandPacket {
    uint8_t data[MAX_COMMAND_WIRE_SIZE]{};
    size_t size{0};
};

// body is decoded [mode, control, length, payload...], excluding delimiters and
// checksum. Only the existing 00/40 command format is supported. Preflights the
// complete escaped size; on failure size is zero and output bytes are unchanged.
bool encode_command(const uint8_t *body, size_t body_size, uint8_t *output,
                    size_t capacity, size_t &size);

// device_temperature is already normalized to the device's integer unit.
// Preserves the original C16..32/F61..90 packets without switching device units.
bool encode_temperature(int device_temperature, bool fahrenheit, CommandPacket &packet);

// Sign-and-magnitude for the verified Dry readback range (-7..7).
inline uint8_t dry_offset_nibble(int8_t offset) {
    return offset < 0 ? static_cast<uint8_t>(0x08 | -offset) : static_cast<uint8_t>(offset);
}

// Offset-only KTWDBC command, restricted to -7..7.
bool encode_dry_offset(int offset, CommandPacket &packet);

// Opaque original packets remain immutable, including unused command variants.
extern const uint8_t on[CMD_SIZE];
extern const uint8_t off[CMD_SIZE];
extern const uint8_t mode_cool[CMD_SIZE];
extern const uint8_t mode_heat[CMD_SIZE];
extern const uint8_t mode_fan[CMD_SIZE];
extern const uint8_t mode_dry[CMD_SIZE];
extern const uint8_t speed_mute[CMD_SIZE];
extern const uint8_t speed_low[CMD_SIZE];
extern const uint8_t speed_med[CMD_SIZE];
extern const uint8_t speed_max[CMD_SIZE];
extern const uint8_t speed_auto[CMD_SIZE];
extern const uint8_t turbo_on[CMD_SIZE];
extern const uint8_t turbo_off[CMD_SIZE];
extern const uint8_t energysave_on[CMD_SIZE];
extern const uint8_t energysave_off[CMD_SIZE];
extern const uint8_t display_on[CMD_SIZE];
extern const uint8_t display_off[CMD_SIZE];
extern const uint8_t sleep_1[CMD_SIZE];
extern const uint8_t sleep_2[CMD_SIZE];
extern const uint8_t sleep_3[CMD_SIZE];
extern const uint8_t sleep_4[CMD_SIZE];
extern const uint8_t sleep_off[CMD_SIZE];
extern const uint8_t vert_dir[CMD_SIZE];
extern const uint8_t vert_swing[CMD_SIZE];
extern const uint8_t hor_dir[CMD_SIZE];
extern const uint8_t hor_swing[CMD_SIZE];
extern const uint8_t temp_to_F[CMD_SIZE];
extern const uint8_t temp_to_F_reset_temp[CMD_SIZE];
extern const uint8_t temp_to_C[CMD_SIZE];
extern const uint8_t temp_to_C_reset_temp[CMD_SIZE];

} // namespace hisense_ac
} // namespace esphome
