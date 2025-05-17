# Hisense Air Conditioner Component for ESPHome

An ESPHome external component for controlling Hisense air conditioners. This component provides comprehensive control and monitoring capabilities for your Hisense AC unit through Home Assistant integration.

## Disclaimer

**USE AT YOUR OWN RISK**: This component is provided "as is" without warranty of any kind, express or implied. The author(s) and contributors of this component take no responsibility for any damage to your air conditioning unit, ESP32 device, or any other equipment that may occur as a result of using this component. By using this component, you acknowledge and agree that you are doing so at your own risk and that you will be solely responsible for any damage that may occur to your equipment.

## Features

- **Basic Climate Control**
  - Operating modes: Heat, Cool, Fan Only, Dry
  - Fan speeds: Auto, Low, Medium, High, Quiet
  - Swing modes: Off, Vertical, Horizontal, Both
  - Temperature control
  - Presets: None, Boost (Turbo), Eco (Energy Save)

- **Advanced Monitoring**
  - Compressor frequency monitoring and control
  - Multiple temperature sensors
  - Indoor humidity monitoring
  - System status tracking

## Hardware

### Requirements
- ESP32 development board (tested with ESP32-DevKitC)
- RS485<->Uart(TTL) converter
- UART connection to AC unit

### Setup
TODO - add schematic and pictures

## Configuration

### Installation

Add the external component to your ESPHome configuration:

```yaml
external_components:
  - source: github://akrabi/hisense_ac_esphome
    components: [hisense_ac]
```


### Basic Configuration

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

### Full Configuration with All Sensors

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

For a complete example configuration including ESP32 setup, WiFi configuration, and all available options, see [examples](examples/README.md).

### Configuration Variables

#### Required Configuration
- **name** (*Required*, string): The name of the climate device
- **temperature_unit** (*Optional*, string): The temperature unit to use. Can be `CELSIUS` or `FAHRENHEIT`. Defaults to `CELSIUS`
- **uart** (*Required*): UART bus configuration
  - **tx_pin** (*Required*, pin): TX pin
  - **rx_pin** (*Required*, pin): RX pin
  - **baud_rate** (*Required*, int): Baud rate (must be 9600)

#### Optional Sensor Configuration
All sensor configurations follow the same pattern and are optional:
- **name** (*Required if sensor enabled*, string): Name of the sensor
- **unit_of_measurement** (*Optional*, string): Unit for the sensor
- **device_class** (*Optional*, string): Home Assistant device class

## Example ESP32 Setup

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

## Troubleshooting

### Common Issues

1. **No Communication**
   - Verify TX/RX pins are correctly connected (TX → RX, RX → TX)
   - Confirm UART settings (baud rate, data bits, parity, stop bits)
   - Check ground connection between ESP32 and AC unit

2. **Erratic Behavior**
   - Enable debug logging to monitor communication
   - Verify power supply is stable
   - Check for interference on UART lines

### Debug Logging

Enable detailed logging by adding to your configuration:

```yaml
logger:
  level: DEBUG
  baud_rate: 0
```

## Contributing

Feel free to submit issues and pull requests on GitHub.

## Acknowledgments

This project was built based on [esphome_airconintl](https://github.com/pslawinski/esphome_airconintl).

## License

This project is released under the MIT License.
