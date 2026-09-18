#include "hisense_ac.h"
#include <cmath>
#ifdef USE_ESP32
#include "esphome/components/uart/uart_component_esp_idf.h"
#include "soc/soc_caps.h"
static_assert(SOC_UART_FIFO_LEN >= esphome::hisense_ac::transport::MAX_PACKET_SIZE,
              "Hisense packets must fit the hardware TX FIFO");
#endif

namespace esphome {
namespace hisense_ac {
namespace {
bool encode_mode(climate::ClimateMode mode, uint8_t &out) {
    switch (mode) {
        case climate::CLIMATE_MODE_OFF: out = transport::MODE_OFF; return true;
        case climate::CLIMATE_MODE_FAN_ONLY: out = 0; return true;
        case climate::CLIMATE_MODE_HEAT: out = 1; return true;
        case climate::CLIMATE_MODE_COOL: out = 2; return true;
        case climate::CLIMATE_MODE_DRY: out = 3; return true;
        default: return false;
    }
}
bool encode_fan(climate::ClimateFanMode fan, uint8_t &out) {
    switch (fan) {
        case climate::CLIMATE_FAN_AUTO: out = 0; return true;
        case climate::CLIMATE_FAN_QUIET: out = 2; return true;
        case climate::CLIMATE_FAN_LOW: out = 10; return true;
        case climate::CLIMATE_FAN_MEDIUM: out = 14; return true;
        case climate::CLIMATE_FAN_HIGH: out = 18; return true;
        default: return false;
    }
}
bool encode_swing(climate::ClimateSwingMode swing, uint8_t &out) {
    switch (swing) {
        case climate::CLIMATE_SWING_OFF: out = 0; return true;
        case climate::CLIMATE_SWING_HORIZONTAL: out = 1; return true;
        case climate::CLIMATE_SWING_VERTICAL: out = 2; return true;
        case climate::CLIMATE_SWING_BOTH: out = 3; return true;
        default: return false;
    }
}
}  // namespace

HisenseAC::HisenseAC(uart::UARTComponent *parent) : PollingComponent(5000), uart::UARTDevice(parent) {}
void HisenseAC::set_temperature_unit(Temperature_Unit unit) { temp_unit = unit; }
void HisenseAC::set_compressor_frequency(sensor::Sensor *sensor) { compressor_frequency = sensor; }
void HisenseAC::set_compressor_frequency_setting(sensor::Sensor *sensor) { compressor_frequency_setting = sensor; }
void HisenseAC::set_compressor_frequency_send(sensor::Sensor *sensor) { compressor_frequency_send = sensor; }
void HisenseAC::set_outdoor_temperature(sensor::Sensor *sensor) { outdoor_temperature = sensor; }
void HisenseAC::set_outdoor_condenser_temperature(sensor::Sensor *sensor) { outdoor_condenser_temperature = sensor; }
void HisenseAC::set_compressor_exhaust_temperature(sensor::Sensor *sensor) { compressor_exhaust_temperature = sensor; }
void HisenseAC::set_target_exhaust_temperature(sensor::Sensor *sensor) { target_exhaust_temperature = sensor; }
void HisenseAC::set_indoor_pipe_temperature(sensor::Sensor *sensor) { indoor_pipe_temperature = sensor; }
void HisenseAC::set_indoor_humidity_setting(sensor::Sensor *sensor) { indoor_humidity_setting = sensor; }
void HisenseAC::set_indoor_humidity_status(sensor::Sensor *sensor) { indoor_humidity_status = sensor; }
void HisenseAC::set_display_switch(switch_::Switch *display_switch) { display_switch_ = display_switch; }

bool HisenseAC::set_display(bool state) {
    transport::Request request;
    request.fields = transport::FIELD_DISPLAY;
    request.display = state;
    uint32_t generation;
    if (!transport_.enqueue(request, millis(), generation)) {
        ESP_LOGW("hisense_ac", "Display request rejected: operation queue full.");
        return false;
    }
    display_generation_ = generation;
    display_state_pending_ = true;
    display_target_state_ = state;
    display_state_pending_since_ = millis();
    return true;
}

void HisenseACDisplaySwitch::write_state(bool state) {
    if (parent_->set_display(state)) publish_state(state);
}

void HisenseAC::setup() {
    for (auto *sensor : {compressor_frequency, compressor_frequency_setting, compressor_frequency_send,
                        outdoor_temperature, outdoor_condenser_temperature, compressor_exhaust_temperature,
                        target_exhaust_temperature, indoor_pipe_temperature, indoor_humidity_setting,
                        indoor_humidity_status}) {
        if (sensor != nullptr) sensor->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    request_update();
}

void HisenseAC::loop() {
    parser_.expire(millis());
    // Bound RX work so a noisy line cannot starve deadlines or other components.
    for (size_t budget = 0; budget < 512 && available(); ++budget) {
        if (get_response(read())) apply_status_();
    }
    transport_.tick(millis());
    if (display_state_pending_ && millis() - display_state_pending_since_ >= DISPLAY_STATE_TIMEOUT_MS) {
        display_state_pending_ = false;
        ESP_LOGW("hisense_ac", "Display confirmation expired.");
    }
}

bool HisenseAC::get_response(uint8_t input) {
    const size_t size = parser_.feed(input, millis());
    if (size == 0) return false;
    if (!protocol::decode_status(parser_.data(), size, status_)) {
        ESP_LOGD("hisense_ac", "Ignoring unsupported response (%u bytes).", static_cast<unsigned>(size));
        return false;
    }
    has_status_ = true;
    last_status_at_ = millis();
    transport_.receive(status_, last_status_at_);
    return true;
}

bool HisenseAC::send_packet(const uint8_t *data, size_t size) {
    // Final YAML validation requires this backend and exclusive ownership.
    auto *backend = static_cast<uart::IDFUARTComponent *>(parent_);
    if (size > transport::MAX_PACKET_SIZE || backend->is_failed()) return false;
    const uint32_t started = millis();
    write_array(data, size);
    if (millis() - started >= 10)
        ESP_LOGW("hisense_ac", "UART write exceeded 10 ms; check exclusive bus ownership and backend.");
    return !backend->is_failed();
}

void HisenseAC::operation_finished(uint32_t generation, transport::Result result,
                                  const transport::Request &request) {
    if (result == transport::Result::UNVERIFIED)
        ESP_LOGW("hisense_ac", "Preset bytes sent; preset feedback is unverified, not confirmed.");
    else if (result != transport::Result::CONFIRMED && result != transport::Result::SUPERSEDED) {
        ESP_LOGW("hisense_ac", "Operation %u ended without confirmation (reason %u).",
                 static_cast<unsigned>(generation), static_cast<unsigned>(result));
        status_set_warning("AC operation not confirmed");
    }
    if ((request.fields & transport::FIELD_DISPLAY) && generation == display_generation_ &&
        result != transport::Result::SUPERSEDED)
        display_state_pending_ = false;
}

void HisenseAC::apply_status_() {
    float target = status_.indoor_temperature_setting;
    float current = status_.indoor_temperature_status;
    if ((temp_unit == CELSIUS && target > 7 && target < 33) ||
        (temp_unit == FAHRENHEIT && target > 45 && target < 91))
        target_temperature = target;
    if ((temp_unit == CELSIUS && current > 1 && current < 49) ||
        (temp_unit == FAHRENHEIT && current > 34 && current < 120))
        current_temperature = current;
    const bool running = status_.compressor_frequency > 0;
    if (status_.run_status == 0) {
        mode = climate::CLIMATE_MODE_OFF;
        action = climate::CLIMATE_ACTION_OFF;
    } else {
        switch (status_.mode_status) {
            case 0: mode = climate::CLIMATE_MODE_FAN_ONLY; action = climate::CLIMATE_ACTION_FAN; break;
            case 1: mode = climate::CLIMATE_MODE_HEAT; action = running ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_IDLE; break;
            case 2: mode = climate::CLIMATE_MODE_COOL; action = running ? climate::CLIMATE_ACTION_COOLING : climate::CLIMATE_ACTION_IDLE; break;
            case 3: mode = climate::CLIMATE_MODE_DRY; action = running ? climate::CLIMATE_ACTION_DRYING : climate::CLIMATE_ACTION_IDLE; break;
        }
    }
    swing_mode = status_.left_right ? (status_.up_down ? climate::CLIMATE_SWING_BOTH : climate::CLIMATE_SWING_HORIZONTAL) :
                                    (status_.up_down ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF);
    switch (status_.wind_status) {
        case 0: fan_mode = climate::CLIMATE_FAN_AUTO; break;
        case 2: fan_mode = climate::CLIMATE_FAN_QUIET; break;
        case 10: fan_mode = climate::CLIMATE_FAN_LOW; break;
        case 14: fan_mode = climate::CLIMATE_FAN_MEDIUM; break;
        case 18: fan_mode = climate::CLIMATE_FAN_HIGH; break;
    }
    // back_led is verified only on ACOND ASTI-09UW4RVEDC00 / AEH-W4B1.
    if (display_switch_ != nullptr &&
        (!display_state_pending_ || status_.back_led == display_target_state_)) {
        display_state_pending_ = false;
        display_switch_->publish_state(status_.back_led);
    }
    if (!transport_.busy()) save_target_temperture();
}

void HisenseAC::request_update() { transport_.request_poll(); }

void HisenseAC::update() {
    request_update();
    publish_state();
    set_sensor(compressor_frequency, status_.compressor_frequency);
    set_sensor(compressor_frequency_setting, status_.compressor_frequency_setting);
    set_sensor(compressor_frequency_send, status_.compressor_frequency_send);
    set_sensor(outdoor_temperature, status_.outdoor_temperature);
    set_sensor(outdoor_condenser_temperature, status_.outdoor_condenser_temperature);
    set_sensor(compressor_exhaust_temperature, status_.compressor_exhaust_temperature);
    set_sensor(target_exhaust_temperature, status_.target_exhaust_temperature);
    set_sensor(indoor_pipe_temperature, status_.indoor_pipe_temperature);
    set_sensor(indoor_humidity_setting, status_.indoor_humidity_setting);
    set_sensor(indoor_humidity_status, status_.indoor_humidity_status);
}

void HisenseAC::control(const climate::ClimateCall &call) {
    transport::Request request;
    request.fahrenheit = temp_unit == FAHRENHEIT;
    bool valid = true;
    if (call.get_mode().has_value()) {
        request.fields |= transport::MODE;
        valid &= encode_mode(*call.get_mode(), request.mode);
    }
    if (call.get_target_temperature().has_value() ||
        ((request.fields & transport::MODE) && (request.mode == 1 || request.mode == 2))) {
        const float target = call.get_target_temperature().has_value() ? *call.get_target_temperature() :
                             request.mode == 1 ? heat_tgt_temp : cool_tgt_temp;
        valid &= std::isfinite(target) && target >= (request.fahrenheit ? 61 : 16) &&
                 target <= (request.fahrenheit ? 90 : 32);
        if (valid) {
            request.fields |= transport::TEMPERATURE;
            request.temperature = static_cast<uint8_t>(roundf(target));
        }
    }
    if (call.get_fan_mode().has_value()) {
        request.fields |= transport::FAN;
        valid &= encode_fan(*call.get_fan_mode(), request.fan);
    }
    if (call.get_swing_mode().has_value()) {
        request.fields |= transport::SWING;
        valid &= encode_swing(*call.get_swing_mode(), request.swing);
    }
    if (call.get_preset().has_value()) {
        request.fields |= transport::PRESET;
        switch (*call.get_preset()) {
            case climate::CLIMATE_PRESET_NONE: request.preset = 0; break;
            case climate::CLIMATE_PRESET_BOOST: request.preset = 1; break;
            case climate::CLIMATE_PRESET_ECO: request.preset = 2; break;
            default: valid = false; break;
        }
    }
    uint32_t generation;
    if (!valid || !transport_.enqueue(request, millis(), generation)) {
        ESP_LOGW("hisense_ac", "Climate call rejected: invalid controls or full operation queue.");
        return;
    }
    // Compatibility presentation retained here; state-sync changes the default.
    if (call.get_mode().has_value()) mode = *call.get_mode();
    if (request.fields & transport::TEMPERATURE) target_temperature = request.temperature;
    if (call.get_fan_mode().has_value()) fan_mode = *call.get_fan_mode();
    if (call.get_swing_mode().has_value()) swing_mode = *call.get_swing_mode();
    if (call.get_preset().has_value()) preset = *call.get_preset();
    publish_state();
}

void HisenseAC::save_target_temperture() {
    if (mode == climate::CLIMATE_MODE_COOL && target_temperature > 0) cool_tgt_temp = target_temperature;
    else if (mode == climate::CLIMATE_MODE_HEAT && target_temperature > 0) heat_tgt_temp = target_temperature;
}

void HisenseAC::set_sensor(sensor::Sensor *sensor, float value) {
    if (sensor != nullptr && (!sensor->has_state() || sensor->get_raw_state() != value))
        sensor->publish_state(value);
}

climate::ClimateTraits HisenseAC::traits() {
    climate::ClimateTraits traits;
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_ACTION);
    traits.set_visual_min_temperature(16);
    traits.set_visual_max_temperature(30);
    traits.set_visual_temperature_step(1);
    traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_COOL,
                               climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_FAN_ONLY, climate::CLIMATE_MODE_DRY});
    traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_BOTH,
                                     climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL});
    traits.set_supported_fan_modes({climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW,
                                   climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_QUIET});
    traits.set_supported_presets({climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_BOOST, climate::CLIMATE_PRESET_ECO});
    return traits;
}

}  // namespace hisense_ac
}  // namespace esphome
