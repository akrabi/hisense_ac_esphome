#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "device_status.h"

namespace esphome {
namespace hisense_ac {

enum Temperature_Unit {
    CELSIUS = 0,
    FAHRENHEIT = 1,
};

class HisenseAC : public PollingComponent, public climate::Climate, public uart::UARTDevice {
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

    void setup() override;
    void loop() override;
    void update() override;
    void control(const climate::ClimateCall &call) override;
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
    Temperature_Unit temp_unit;
    float heat_tgt_temp = 16.1111f;
    float cool_tgt_temp = 26.6667f;
    static const int UART_BUF_SIZE = 128;
    uint8_t uart_buf[UART_BUF_SIZE];
    bool wait_for_rx = false;

    int get_response(const uint8_t input, uint8_t *out);
    void blocking_send(uint8_t buf[], size_t sz);
    void request_update();
    void set_sensor(sensor::Sensor *sensor, float value);
    void set_temp(float temp);
};

} // namespace hisense_ac
} // namespace esphome
