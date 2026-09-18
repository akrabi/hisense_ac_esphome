#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/uart/uart.h"
#include "protocol.h"
#include "transport.h"

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
    const std::string trace_tag = "hisense_ac";
    Temperature_Unit temp_unit{CELSIUS};
    float heat_tgt_temp = 25.0f;
    float cool_tgt_temp = 25.0f;
    protocol::FrameParser parser_;
    DeviceStatus status_{};
    transport::Engine transport_{this};
    bool has_status_{false};
    uint32_t last_status_at_{0};
    switch_::Switch *display_switch_{nullptr};
    bool display_state_pending_{false};
    bool display_target_state_{false};
    uint32_t display_state_pending_since_{0};
    uint32_t display_generation_{0};
    static constexpr uint32_t DISPLAY_STATE_TIMEOUT_MS = 10000;

    bool get_response(uint8_t input);
    bool send_packet(const uint8_t *data, size_t size) override;
    void operation_finished(uint32_t generation, transport::Result result, const transport::Request &request) override;
    void apply_status_();
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
