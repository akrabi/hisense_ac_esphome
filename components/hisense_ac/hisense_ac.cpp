#include "hisense_ac.h"
#include "temperature.h"
#include <cmath>
#ifdef USE_ESP32
#include "esphome/components/uart/uart_component_esp_idf.h"
#include "soc/soc_caps.h"
static_assert(SOC_UART_FIFO_LEN >= esphome::hisense_ac::transport::MAX_PACKET_SIZE,
              "Hisense packets must fit the hardware TX FIFO");
#endif

namespace esphome {
namespace hisense_ac {
static const char *const TAG = "hisense_ac";
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
        if (queue_rejections_ != UINT32_MAX) ++queue_rejections_;
        ESP_LOGW("hisense_ac", "Display request rejected: operation queue full.");
        operation_warning_ = true;
        update_warning_();
        return false;
    }
    accepted_(request, generation);
    return true;
}

void HisenseACDisplaySwitch::write_state(bool state) {
    parent_->set_display(state);
}

void HisenseAC::setup() {
    started_at_ = millis();
    request_update();
    update_warning_();
    publish_diagnostics_();
}

void HisenseAC::dump_config() {
    LOG_CLIMATE("", "Hisense AC", this);
    ESP_LOGCONFIG("hisense_ac", "  Protocol temperatures: %s", temp_unit == FAHRENHEIT ? "Fahrenheit" : "Celsius");
    ESP_LOGCONFIG("hisense_ac", "  State reporting: %s", optimistic_ ? "optimistic with reconciliation" : "device-reported");
    ESP_LOGCONFIG("hisense_ac", "  UART: dedicated ESP32 hardware, 9600 8N1");
    ESP_LOGCONFIG("hisense_ac", "  Poll interval: %u ms; response timeout: %u ms; operation lifetime: %u ms",
                  static_cast<unsigned>(get_update_interval()), static_cast<unsigned>(transport::RESPONSE_TIMEOUT_MS),
                  static_cast<unsigned>(transport::OPERATION_TIMEOUT_MS));
    ESP_LOGCONFIG("hisense_ac", "  Pending operation capacity: %u", static_cast<unsigned>(transport::QUEUE_CAPACITY));
}

void HisenseAC::loop() {
    parser_.expire(millis());
    if (pending_.expire(millis())) {
        presentation_dirty_ = optimistic_;
        operation_warning_ = true;
        request_update();
        ESP_LOGW("hisense_ac", "Pending state expired without confirmation.");
    }
    // Bound RX work so a noisy line cannot starve deadlines or other components.
    for (size_t budget = 0; budget < 512 && available(); ++budget) {
        if (get_response(read())) apply_status_();
    }
    transport_.tick(millis());
    if (parser_.invalid_frames() != reported_frame_errors_ &&
        (!has_frame_error_log_ || millis() - frame_error_log_at_ >= 30000)) {
        reported_frame_errors_ = parser_.invalid_frames();
        frame_error_log_at_ = millis();
        has_frame_error_log_ = true;
        ESP_LOGW(TAG, "Discarded invalid UART frames: %u; last reason: %s",
                 static_cast<unsigned>(reported_frame_errors_), parser_.last_error_name());
    }
    uint64_t stale = static_cast<uint64_t>(get_update_interval()) * 3;
    if (stale < 10000) stale = 10000;
    if (stale > 0x7FFFFFFF) stale = 0x7FFFFFFF;
    if (static_cast<uint32_t>(millis() - (has_status_ ? last_status_at_ : started_at_)) >= stale)
        communication_warning_ = true;
    update_warning_();
    publish_diagnostics_();
    if (presentation_dirty_) publish_presentation_();
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
    communication_warning_ = false;
    transport_.receive(status_, last_status_at_);
    return true;
}

bool HisenseAC::send_packet(const uint8_t *data, size_t size) {
    // Final YAML validation requires this backend and exclusive ownership.
    auto *backend = static_cast<uart::IDFUARTComponent *>(parent_);
    if (size > transport::MAX_PACKET_SIZE || backend->is_failed()) return false;
#if defined(USE_ESP32) && SOC_UART_LP_NUM >= 1
    // Some ESP32 variants also expose a smaller LP UART. Do not apply the
    // high-power FIFO assumption to whichever UART ESPHome actually assigned.
    if (backend->get_hw_serial_number() >= SOC_UART_HP_NUM && size > SOC_LP_UART_FIFO_LEN) {
        ESP_LOGW("hisense_ac", "Assigned LP UART FIFO is too small for this packet; transmission refused.");
        return false;
    }
#endif
    const uint32_t started = millis();
    write_array(data, size);
    if (millis() - started >= 10)
        ESP_LOGW("hisense_ac", "UART write exceeded 10 ms; check exclusive bus ownership and backend.");
    return !backend->is_failed();
}

void HisenseAC::operation_finished(uint32_t generation, transport::Result result,
                                  const transport::Request &request) {
    (void) request;
    if (result == transport::Result::TIMEOUT && response_timeouts_ != UINT32_MAX) ++response_timeouts_;
    const bool changed = pending_.complete(generation);
    presentation_dirty_ |= changed && optimistic_;
    if (result == transport::Result::UNVERIFIED) {
        ESP_LOGW("hisense_ac", "Preset bytes sent; preset feedback is unverified, not confirmed.");
        operation_warning_ = true;
    } else if (result == transport::Result::CONFIRMED) {
        if (generation == latest_generation_) operation_warning_ = false;
    } else if (result != transport::Result::SUPERSEDED) {
        ESP_LOGW("hisense_ac", "Operation %u ended without confirmation (reason %u).",
                 static_cast<unsigned>(generation), static_cast<unsigned>(result));
        if (generation == 0) communication_warning_ = true;
        else operation_warning_ = true;
    }
}

void HisenseAC::apply_status_() {
    const float target = status_.indoor_temperature_setting;
    const float current = status_.indoor_temperature_status;
    bool unknown = false;
    if ((temp_unit == CELSIUS && target > 7 && target < 33) ||
        (temp_unit == FAHRENHEIT && target > 45 && target < 91)) {
        confirmed_.temperature = temperature::from_device(status_.indoor_temperature_setting, temp_unit == FAHRENHEIT);
        confirmed_.fahrenheit = temp_unit == FAHRENHEIT;
        confirmed_.fields |= transport::TEMPERATURE;
    } else unknown = true;
    if ((temp_unit == CELSIUS && current > 1 && current < 49) ||
        (temp_unit == FAHRENHEIT && current > 34 && current < 120))
        reported_current_ = temperature::from_device(status_.indoor_temperature_status, temp_unit == FAHRENHEIT);
    else unknown = true;
    const bool running = status_.compressor_frequency > 0;
    if (status_.run_status == 0) {
        confirmed_.mode = transport::MODE_OFF;
        confirmed_.fields |= transport::MODE;
        reported_action_ = climate::CLIMATE_ACTION_OFF;
        has_reported_action_ = true;
    } else if (status_.mode_status <= 3) {
        confirmed_.mode = status_.mode_status;
        confirmed_.fields |= transport::MODE;
        has_reported_action_ = true;
        switch (status_.mode_status) {
            case 0: reported_action_ = climate::CLIMATE_ACTION_FAN; break;
            case 1: reported_action_ = running ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_IDLE; break;
            case 2: reported_action_ = running ? climate::CLIMATE_ACTION_COOLING : climate::CLIMATE_ACTION_IDLE; break;
            case 3: reported_action_ = running ? climate::CLIMATE_ACTION_DRYING : climate::CLIMATE_ACTION_IDLE; break;
        }
    } else unknown = true;
    confirmed_.swing = (status_.left_right ? 1 : 0) | (status_.up_down ? 2 : 0);
    confirmed_.display = status_.back_led;
    confirmed_.fields |= transport::SWING | transport::FIELD_DISPLAY;
    switch (status_.wind_status) {
        case 0: case 2: case 10: case 14: case 18:
            confirmed_.fan = status_.wind_status;
            confirmed_.fields |= transport::FAN;
            break;
        default: unknown = true; break;
    }
    if (unknown && (!has_unknown_warning_ || millis() - unknown_warning_at_ >= 30000)) {
        has_unknown_warning_ = true;
        unknown_warning_at_ = millis();
        ESP_LOGW("hisense_ac", "Unsupported status fields: mode=%u fan=%u target=%u current=%u; retaining known fields.",
                 status_.mode_status, status_.wind_status, status_.indoor_temperature_setting,
                 status_.indoor_temperature_status);
    }
    if (!unknown && !transport_.busy() && !(pending_.fields() & (transport::MODE | transport::TEMPERATURE)))
        save_target_temperture();
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
    publish_presentation_();
}

void HisenseAC::publish_presentation_() {
    presentation_dirty_ = false;
    const auto shown = pending_.present(confirmed_, optimistic_);
    // back_led is verified only on ACOND ASTI-09UW4RVEDC00 / AEH-W4B1.
    if (display_switch_ != nullptr && (shown.fields & transport::FIELD_DISPLAY))
        display_switch_->publish_state(shown.display);
    if (shown.fields & transport::TEMPERATURE) target_temperature = shown.temperature;
    else target_temperature = NAN;
    current_temperature = reported_current_;
    fan_mode.reset();
    if (shown.fields & transport::FAN) {
        switch (shown.fan) {
            case 0: fan_mode = climate::CLIMATE_FAN_AUTO; break;
            case 2: fan_mode = climate::CLIMATE_FAN_QUIET; break;
            case 10: fan_mode = climate::CLIMATE_FAN_LOW; break;
            case 14: fan_mode = climate::CLIMATE_FAN_MEDIUM; break;
            case 18: fan_mode = climate::CLIMATE_FAN_HIGH; break;
        }
    }
    if (shown.fields & transport::SWING) {
        const climate::ClimateSwingMode swings[] = {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_HORIZONTAL,
                                                    climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_BOTH};
        swing_mode = swings[shown.swing];
    }
    preset.reset(); // Never fabricate a confirmed preset from unverified bits.
    if (shown.fields & transport::PRESET) {
        const climate::ClimatePreset presets[] = {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_BOOST,
                                                 climate::CLIMATE_PRESET_ECO};
        preset = presets[shown.preset];
    }
    if (has_reported_action_) action = reported_action_;
    // Climate mode/action have no unknown representation. Before the first
    // report, keep measurements unpublished rather than synthesizing Off.
    if (!(shown.fields & transport::MODE) || !has_reported_action_) return;
    const climate::ClimateMode modes[] = {climate::CLIMATE_MODE_FAN_ONLY, climate::CLIMATE_MODE_HEAT,
                                          climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_DRY,
                                          climate::CLIMATE_MODE_OFF};
    mode = modes[shown.mode];
    publish_state();
}

void HisenseAC::update_warning_() {
    if (operation_warning_) status_set_warning("AC operation not confirmed");
    else if (communication_warning_) status_set_warning("Waiting for valid AC status");
    else status_clear_warning();
}

void HisenseAC::publish_diagnostics_() {
    const bool connected = has_status_ && !communication_warning_;
    if (communication_connected_ != nullptr &&
        (!communication_connected_->has_state() || communication_connected_->state != connected))
        communication_connected_->publish_state(connected);
    if (has_status_)
        set_sensor(last_status_age_, static_cast<uint32_t>(millis() - last_status_at_) / 1000);
    set_sensor(invalid_frame_count_, parser_.invalid_frames());
    set_sensor(response_timeout_count_, response_timeouts_);
    set_sensor(queue_rejection_count_, queue_rejections_);
}

void HisenseAC::accepted_(const transport::Request &request, uint32_t generation) {
    latest_generation_ = generation;
    pending_.accept(request, generation, millis());
    if (optimistic_) publish_presentation_();
}

void HisenseAC::request_update() { transport_.request_poll(); }

void HisenseAC::update() {
    request_update();
}

void HisenseAC::control(const climate::ClimateCall &call) {
    transport::Request request;
    request.fahrenheit = temp_unit == FAHRENHEIT;
    bool valid = true;
    if (call.get_mode().has_value()) {
        request.fields |= transport::MODE;
        valid &= supported_modes_.count(*call.get_mode()) && encode_mode(*call.get_mode(), request.mode);
    }
    const float remembered = request.mode == 1 ? heat_tgt_temp : cool_tgt_temp;
    if (call.get_target_temperature().has_value() ||
        ((request.fields & transport::MODE) && (request.mode == 1 || request.mode == 2) && std::isfinite(remembered))) {
        const float target = call.get_target_temperature().has_value() ? *call.get_target_temperature() :
                             remembered;
        uint8_t encoded;
        const bool encodable = temperature::normalize(target, request.fahrenheit, request.temperature, encoded);
        valid &= encodable;
        if (encodable) {
            request.fields |= transport::TEMPERATURE;
        }
    }
    if (call.get_fan_mode().has_value()) {
        request.fields |= transport::FAN;
        valid &= encode_fan(*call.get_fan_mode(), request.fan);
    }
    if (call.get_swing_mode().has_value()) {
        request.fields |= transport::SWING;
        valid &= supported_swing_modes_.count(*call.get_swing_mode()) && encode_swing(*call.get_swing_mode(), request.swing);
    }
    if (call.get_preset().has_value()) {
        valid &= supported_presets_.count(*call.get_preset()) != 0;
        request.fields |= transport::PRESET;
        switch (*call.get_preset()) {
            case climate::CLIMATE_PRESET_NONE: request.preset = 0; break;
            case climate::CLIMATE_PRESET_BOOST: request.preset = 1; break;
            case climate::CLIMATE_PRESET_ECO: request.preset = 2; break;
            default: valid = false; break;
        }
    }
    uint32_t generation;
    if (!valid || request.fields == 0) {
        ESP_LOGW("hisense_ac", "Climate call rejected: invalid or unsupported controls.");
        operation_warning_ = true;
        update_warning_();
        return;
    }
    if (!transport_.enqueue(request, millis(), generation)) {
        if (queue_rejections_ != UINT32_MAX) ++queue_rejections_;
        ESP_LOGW("hisense_ac", "Climate call rejected: operation queue full.");
        operation_warning_ = true;
        update_warning_();
        return;
    }
    if (call.get_target_temperature().has_value()) {
        const auto context = pending_.present(confirmed_, true);
        const auto memory_mode = (request.fields & transport::MODE) ? request.mode :
                                 (context.fields & transport::MODE) ? context.mode : transport::MODE_OFF;
        if (memory_mode == 1) heat_tgt_temp = request.temperature;
        else if (memory_mode == 2) cool_tgt_temp = request.temperature;
    }
    accepted_(request, generation);
}

void HisenseAC::save_target_temperture() {
    if ((confirmed_.fields & (transport::MODE | transport::TEMPERATURE)) != (transport::MODE | transport::TEMPERATURE))
        return;
    if (confirmed_.mode == 2) cool_tgt_temp = confirmed_.temperature;
    else if (confirmed_.mode == 1) heat_tgt_temp = confirmed_.temperature;
}

void HisenseAC::set_sensor(sensor::Sensor *sensor, float value) {
    if (sensor != nullptr && (!sensor->has_state() || sensor->get_raw_state() != value))
        sensor->publish_state(value);
}

climate::ClimateTraits HisenseAC::traits() {
    climate::ClimateTraits traits;
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_ACTION);
    traits.set_visual_min_temperature(temp_unit == FAHRENHEIT ? temperature::from_device(61, true) : 16);
    traits.set_visual_max_temperature(30);
    traits.set_visual_temperature_step(temp_unit == FAHRENHEIT ? 5.0f / 9.0f : 1.0f);
    traits.set_supported_modes(supported_modes_);
    traits.set_supported_swing_modes(supported_swing_modes_);
    traits.set_supported_fan_modes({climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW,
                                   climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_QUIET});
    traits.set_supported_presets(supported_presets_);
    return traits;
}

}  // namespace hisense_ac
}  // namespace esphome
