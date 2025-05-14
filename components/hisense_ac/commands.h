#pragma once

#include <cstdint>

namespace esphome {
namespace hisense_ac {

// Common command sizes
constexpr uint8_t CMD_SIZE = 52;

// Power commands
extern uint8_t on[CMD_SIZE];
extern uint8_t off[CMD_SIZE];

// Mode commands
extern uint8_t mode_cool[CMD_SIZE];
extern uint8_t mode_heat[CMD_SIZE];
extern uint8_t mode_fan[CMD_SIZE];
extern uint8_t mode_dry[CMD_SIZE];

// Fan speed commands
extern uint8_t speed_mute[CMD_SIZE];
extern uint8_t speed_low[CMD_SIZE];
extern uint8_t speed_med[CMD_SIZE];
extern uint8_t speed_max[CMD_SIZE];
extern uint8_t speed_auto[CMD_SIZE];

// Temperature commands for Celsius
extern uint8_t temp_16_C[CMD_SIZE+1];
extern uint8_t temp_17_C[CMD_SIZE];
extern uint8_t temp_18_C[CMD_SIZE];
extern uint8_t temp_19_C[CMD_SIZE];
extern uint8_t temp_20_C[CMD_SIZE];
extern uint8_t temp_21_C[CMD_SIZE];
extern uint8_t temp_22_C[CMD_SIZE];
extern uint8_t temp_23_C[CMD_SIZE];
extern uint8_t temp_24_C[CMD_SIZE];
extern uint8_t temp_25_C[CMD_SIZE];
extern uint8_t temp_26_C[CMD_SIZE];
extern uint8_t temp_27_C[CMD_SIZE];
extern uint8_t temp_28_C[CMD_SIZE];
extern uint8_t temp_29_C[CMD_SIZE];
extern uint8_t temp_30_C[CMD_SIZE];
extern uint8_t temp_31_C[CMD_SIZE];
extern uint8_t temp_32_C[CMD_SIZE];

// Temperature commands for Fahrenheit
extern uint8_t temp_61_F[CMD_SIZE];
extern uint8_t temp_62_F[CMD_SIZE];
extern uint8_t temp_63_F[CMD_SIZE];
extern uint8_t temp_64_F[CMD_SIZE];
extern uint8_t temp_65_F[CMD_SIZE];
extern uint8_t temp_66_F[CMD_SIZE];
extern uint8_t temp_67_F[CMD_SIZE];
extern uint8_t temp_68_F[CMD_SIZE];
extern uint8_t temp_69_F[CMD_SIZE];
extern uint8_t temp_70_F[CMD_SIZE];
extern uint8_t temp_71_F[CMD_SIZE];
extern uint8_t temp_72_F[CMD_SIZE];
extern uint8_t temp_73_F[CMD_SIZE];
extern uint8_t temp_74_F[CMD_SIZE];
extern uint8_t temp_75_F[CMD_SIZE];
extern uint8_t temp_76_F[CMD_SIZE];
extern uint8_t temp_77_F[CMD_SIZE];
extern uint8_t temp_78_F[CMD_SIZE];
extern uint8_t temp_79_F[CMD_SIZE];
extern uint8_t temp_80_F[CMD_SIZE];
extern uint8_t temp_81_F[CMD_SIZE];
extern uint8_t temp_82_F[CMD_SIZE];
extern uint8_t temp_83_F[CMD_SIZE];
extern uint8_t temp_84_F[CMD_SIZE];
extern uint8_t temp_85_F[CMD_SIZE];
extern uint8_t temp_86_F[CMD_SIZE];
extern uint8_t temp_87_F[CMD_SIZE];
extern uint8_t temp_88_F[CMD_SIZE];
extern uint8_t temp_89_F[CMD_SIZE];
extern uint8_t temp_90_F[CMD_SIZE];

// Other commands
extern uint8_t turbo_on[CMD_SIZE];
extern uint8_t turbo_off[CMD_SIZE];
extern uint8_t energysave_on[CMD_SIZE];
extern uint8_t energysave_off[CMD_SIZE];
extern uint8_t display_on[CMD_SIZE];
extern uint8_t display_off[CMD_SIZE];
extern uint8_t sleep_1[CMD_SIZE];
extern uint8_t sleep_2[CMD_SIZE];
extern uint8_t sleep_3[CMD_SIZE];
extern uint8_t sleep_4[CMD_SIZE];
extern uint8_t sleep_off[CMD_SIZE];
extern uint8_t vert_dir[CMD_SIZE];
extern uint8_t vert_swing[CMD_SIZE];
extern uint8_t hor_dir[CMD_SIZE];
extern uint8_t hor_swing[CMD_SIZE];
extern uint8_t temp_to_F[CMD_SIZE];
extern uint8_t temp_to_F_reset_temp[CMD_SIZE];
extern uint8_t temp_to_C[CMD_SIZE];
extern uint8_t temp_to_C_reset_temp[CMD_SIZE];

} // namespace hisense_ac
} // namespace esphome