#pragma once
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <deque>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGV(...) ((void)0)
#define ESP_LOGCONFIG(...) ((void)0)
#define LOG_CLIMATE(...) ((void)0)
namespace esphome {
extern uint32_t test_clock;
inline uint32_t millis() { return test_clock; }
class Component {
public:
    virtual void setup() {}
    virtual void dump_config() {}
    virtual void loop() {}
    bool is_failed() const { return failed; }
    void status_set_warning(const char * = "") { warning = true; }
    void status_clear_warning() { warning = false; }
    bool failed{false}, warning{false};
};
class PollingComponent : public Component {
public:
    explicit PollingComponent(uint32_t interval) : interval_(interval) {}
    virtual void update() {}
    uint32_t get_update_interval() const { return interval_; }
    uint32_t interval_;
};
namespace sensor {
constexpr int STATE_CLASS_MEASUREMENT = 1;
class Sensor {
public:
    std::vector<float> publications;
    void set_state_class(int) {}
    bool has_state() const { return !publications.empty(); }
    float get_raw_state() const { return publications.back(); }
    void publish_state(float value) { publications.push_back(value); }
};
}
namespace switch_ {
class Switch {
public:
    std::vector<bool> publications;
    bool state{false};
    void publish_state(bool value) { state = value; publications.push_back(value); }
    void control(bool value) { write_state(value); }
protected:
    virtual void write_state(bool) {}
};
}
namespace uart {
class UARTComponent : public Component {
public:
    std::deque<uint8_t> rx;
    std::vector<std::vector<uint8_t>> tx;
    std::vector<uint32_t> sent_at;
};
using IDFUARTComponent = UARTComponent;
class UARTDevice {
public:
    explicit UARTDevice(UARTComponent *parent) : parent_(parent) {}
    bool available() const { return !parent_->rx.empty(); }
    uint8_t read() { const auto byte = parent_->rx.front(); parent_->rx.pop_front(); return byte; }
    void write_array(const uint8_t *data, size_t size) {
        parent_->tx.emplace_back(data, data + size);
        parent_->sent_at.push_back(millis());
    }
protected:
    UARTComponent *parent_;
};
}
namespace climate {
enum ClimateMode { CLIMATE_MODE_OFF, CLIMATE_MODE_COOL, CLIMATE_MODE_HEAT, CLIMATE_MODE_FAN_ONLY, CLIMATE_MODE_DRY, CLIMATE_MODE_AUTO };
enum ClimateFanMode { CLIMATE_FAN_AUTO, CLIMATE_FAN_QUIET, CLIMATE_FAN_LOW, CLIMATE_FAN_MEDIUM, CLIMATE_FAN_HIGH };
enum ClimateSwingMode { CLIMATE_SWING_OFF, CLIMATE_SWING_HORIZONTAL, CLIMATE_SWING_VERTICAL, CLIMATE_SWING_BOTH };
enum ClimatePreset { CLIMATE_PRESET_NONE, CLIMATE_PRESET_BOOST, CLIMATE_PRESET_ECO };
enum ClimateAction { CLIMATE_ACTION_OFF, CLIMATE_ACTION_IDLE, CLIMATE_ACTION_FAN, CLIMATE_ACTION_COOLING, CLIMATE_ACTION_HEATING, CLIMATE_ACTION_DRYING };
constexpr int CLIMATE_SUPPORTS_CURRENT_TEMPERATURE = 1, CLIMATE_SUPPORTS_ACTION = 2;
class ClimateCall {
public:
    std::optional<ClimateMode> requested_mode;
    std::optional<ClimateFanMode> requested_fan;
    std::optional<ClimateSwingMode> requested_swing;
    std::optional<ClimatePreset> requested_preset;
    std::optional<float> requested_temperature;
    const auto &get_mode() const { return requested_mode; }
    const auto &get_fan_mode() const { return requested_fan; }
    const auto &get_swing_mode() const { return requested_swing; }
    const auto &get_preset() const { return requested_preset; }
    const auto &get_target_temperature() const { return requested_temperature; }
};
class ClimateTraits {
public:
    void add_feature_flags(int) {}
    void set_visual_min_temperature(float) {}
    void set_visual_max_temperature(float) {}
    void set_visual_temperature_step(float) {}
    void set_supported_modes(std::initializer_list<ClimateMode>) {}
    void set_supported_fan_modes(std::initializer_list<ClimateFanMode>) {}
    void set_supported_swing_modes(std::initializer_list<ClimateSwingMode>) {}
    void set_supported_presets(std::initializer_list<ClimatePreset>) {}
};
struct Publication {
    ClimateMode mode;
    ClimateAction action;
    float target, current;
    std::optional<ClimateFanMode> fan;
    ClimateSwingMode swing;
    std::optional<ClimatePreset> preset;
};
class Climate {
public:
    virtual void control(const ClimateCall &) {}
    virtual ClimateTraits traits() { return {}; }
    ClimateMode mode{CLIMATE_MODE_OFF};
    ClimateAction action{CLIMATE_ACTION_OFF};
    float target_temperature{NAN}, current_temperature{NAN};
    std::optional<ClimateFanMode> fan_mode;
    ClimateSwingMode swing_mode{CLIMATE_SWING_OFF};
    std::optional<ClimatePreset> preset;
    std::vector<Publication> publications;
    void publish_state() {
        publications.push_back({mode, action, target_temperature, current_temperature, fan_mode, swing_mode, preset});
    }
};
}
}
