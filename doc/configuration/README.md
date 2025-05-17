# Configuration

For a full working example configuration see the configuration folder.

## Installation

Add the external component to your ESPHome configuration:

```yaml
external_components:
  - source: github://akrabi/hisense_ac_esphome
    components: [hisense_ac]
```


## Basic Configuration

```yaml
climate:
  - platform: hisense_ac
    name: "Air Conditioner"
    temperature_unit: CELSIUS
    uart:
      tx_pin: GPIO16
      rx_pin: GPIO17
      baud_rate: 9600
```

## Full Configuration with All Sensors

```yaml
climate:
  - platform: hisense_ac
    name: "Air Conditioner"
    temperature_unit: CELSIUS
    uart_id: uart_bus
    # Optional sensor configurations
    compressor_frequency:
      name: "Compressor Frequency"
      unit_of_measurement: "Hz"
    compressor_frequency_setting:
      name: "Compressor Frequency Setting"
      unit_of_measurement: "Hz"
    compressor_frequency_send:
      name: "Compressor Frequency Send"
      unit_of_measurement: "Hz"
    outdoor_temperature:
      name: "Outdoor Temperature"
      unit_of_measurement: "°C"
    outdoor_condenser_temperature:
      name: "Outdoor Condenser Temperature"
      unit_of_measurement: "°C"
    compressor_exhaust_temperature:
      name: "Compressor Exhaust Temperature"
      unit_of_measurement: "°C"
    target_exhaust_temperature:
      name: "Target Exhaust Temperature"
      unit_of_measurement: "°C"
    indoor_pipe_temperature:
      name: "Indoor Evaporator Inlet Temperature"
      unit_of_measurement: "°C"
    indoor_humidity_setting:
      name: "Indoor Humidity Setting"
      unit_of_measurement: "%"
    indoor_humidity_status:
      name: "Indoor Humidity Status"
      unit_of_measurement: "%"
```

## Configuration Variables

### Required Configuration
- **name** (*Required*, string): The name of the climate device
- **temperature_unit** (*Optional*, string): The temperature unit to use. Can be `CELSIUS` or `FAHRENHEIT`. Defaults to `CELSIUS`
- **uart** (*Required*): UART bus configuration
  - **tx_pin** (*Required*, pin): TX pin
  - **rx_pin** (*Required*, pin): RX pin
  - **baud_rate** (*Required*, int): Baud rate (must be 9600)

### Optional Sensor Configuration
All sensor configurations follow the same pattern and are optional:
- **name** (*Required if sensor enabled*, string): Name of the sensor
- **unit_of_measurement** (*Optional*, string): Unit for the sensor
- **device_class** (*Optional*, string): Home Assistant device class

# Example ESP32 Setup

```yaml
# Basic ESP32 setup
esp32:
  board: esp32dev
  framework:
    type: arduino

# UART Configuration
uart:
  id: uart_bus
  tx_pin: GPIO16
  rx_pin: GPIO17
  baud_rate: 9600
  data_bits: 8
  parity: NONE
  stop_bits: 1
```