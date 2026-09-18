#pragma once
#include <cmath>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/uart/uart.h"
#include "protocol.h"
#include "transport.h"
#include "pending_state.h"

namespace esphome {
namespace hisense_ac {

enum Temperature_Unit {
    CELSIUS = 0,
    FAHRENHEIT = 1,
};

class HisenseAC : public PollingComponent, public climate::Climate, public uart::UARTDevice,
                  private transport::Listener {
public:
    HisenseAC(uart::UARTComponent *parent);
    
    void set_temperature_unit(Temperature_Unit unit);
    void set_optimistic(bool optimistic) { optimistic_ = optimistic; }
    void set_compressor_frequency(sensor::Sensor *sensor);
    void set_compressor_frequency_setting(sensor::Sensor *sensor);
    void set_compressor_frequency_send(sensor::Sensor *sensor);
    void set_outdoor_temperature(sensor::Sensor *sensor);
    void set_outdoor_condenser_temperature(sensor::Sensor *sensor);
    void set_compressor_exhaust_temperature(sensor::Sensor *sensor);
    void set_target_exhaust_temperature(sensor::Sensor *sensor);
    void set_indoor_pipe_temperature(sensor::Sensor *sensor);
    void set_indoor_humidity_setting(sensor::Sensor *sensor);
    void set_indoor_humidity_status(sensor::Sensor *sensor);
    void set_display_switch(switch_::Switch *display_switch);
    bool set_display(bool state);

    void setup() override;
    void dump_config() override;
    void loop() override;
    void update() override;
    void control(const climate::ClimateCall &call) override;
    void save_target_temperture();
    climate::ClimateTraits traits() override;

    sensor::Sensor *compressor_frequency{nullptr};
    sensor::Sensor *compressor_frequency_setting{nullptr};
    sensor::Sensor *compressor_frequency_send{nullptr};
    sensor::Sensor *outdoor_temperature{nullptr};
    sensor::Sensor *outdoor_condenser_temperature{nullptr};
    sensor::Sensor *compressor_exhaust_temperature{nullptr};
    sensor::Sensor *target_exhaust_temperature{nullptr};
    sensor::Sensor *indoor_pipe_temperature{nullptr};
    sensor::Sensor *indoor_humidity_setting{nullptr};
    sensor::Sensor *indoor_humidity_status{nullptr};

private:
    Temperature_Unit temp_unit{CELSIUS};
    float heat_tgt_temp = NAN;
    float cool_tgt_temp = NAN;
    protocol::FrameParser parser_;
    DeviceStatus status_{};
    transport::Engine transport_{this};
    bool has_status_{false};
    uint32_t last_status_at_{0};
    uint32_t started_at_{0};
    bool optimistic_{false};
    PendingState pending_;
    transport::Request confirmed_{};
    float reported_current_{NAN};
    climate::ClimateAction reported_action_{climate::CLIMATE_ACTION_OFF};
    bool has_reported_action_{false};
    bool presentation_dirty_{false};
    bool communication_warning_{true};
    bool operation_warning_{false};
    uint32_t latest_generation_{0};
    uint32_t unknown_warning_at_{0};
    bool has_unknown_warning_{false};
    switch_::Switch *display_switch_{nullptr};

    bool get_response(uint8_t input);
    bool send_packet(const uint8_t *data, size_t size) override;
    void operation_finished(uint32_t generation, transport::Result result, const transport::Request &request) override;
    void apply_status_();
    void publish_presentation_();
    void update_warning_();
    void accepted_(const transport::Request &request, uint32_t generation);
    void request_update();
    void set_sensor(sensor::Sensor *sensor, float value);
};

class HisenseACDisplaySwitch : public switch_::Switch {
public:
    explicit HisenseACDisplaySwitch(HisenseAC *parent) : parent_(parent) {}

protected:
    void write_state(bool state) override;
    HisenseAC *parent_;
};

} // namespace hisense_ac
} // namespace esphome
