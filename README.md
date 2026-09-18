# Hisense Air Conditioner Component for ESPHome

This project is a replacement hardware and a matching ESPHome external component for  Hisense Air Conditioners Wifi Module. While these Wifi modules allow control over the AC (even local only) with [integration to HA](https://github.com/deiger/AirCon), the module itself is highly unreliable.
By replacing the existing Hisense module with our custom one we achieve better reliabilty, faster response time and additional information about the AC unit.

## Disclaimer

**USE AT YOUR OWN RISK**: This component is provided "as is" without warranty of any kind, express or implied. The author(s) and contributors of this component take no responsibility for any damage to your air conditioning unit, ESP32 device, or any other equipment that may occur as a result of using this component. By using this component, you acknowledge and agree that you are doing so at your own risk and that you will be solely responsible for any damage that may occur to your equipment.

## Features

- **Basic Climate Control**
  - Operating modes: Heat, Cool, Fan Only, Dry
  - Fan speeds: Auto, Low, Medium, High, Quiet
  - Swing modes: Off, Vertical, Horizontal, Both
  - Temperature control
  - Presets: None, Boost (Turbo), Eco (Energy Save)
  - Optional display switch

- **Advanced Monitoring**
  - Compressor frequency monitoring
  - Multiple temperature sensors
  - Indoor humidity monitoring
  - System status tracking
  - Optional communication health, status age and error counters

## Hardware

### Requirements
A compatible AC unit with a Hisense Wifi Module (see [compatible devices](doc/hardware/COMPATIBLE_DEVICES.md) for tested models)

### Setup
See the [hardware](doc/hardware/README.md) documentation for further details.

## Configuration

For a complete example configuration including ESP32 setup, WiFi configuration, and all available options, see [configuration](doc/configuration/README.md).

Communication uses a dedicated ESP32 hardware UART with a bounded asynchronous
command queue. State is reported by the AC by default; `optimistic: true` enables
immediate control feedback with subsequent reconciliation. See the
[migration notes](doc/configuration/README.md#state-reporting-and-migration).

```yaml
external_components:
  - source: github://akrabi/hisense_ac_esphome
    components: [hisense_ac]
uart:
  id: uart_bus
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600
climate:
  - platform: hisense_ac
    name: "Air Conditioner"
    uart_id: uart_bus
    temperature_unit: CELSIUS
    display:
      name: "Display"
```

## Contributing

Feel free to submit issues and pull requests on GitHub.

See [regression tests](tests/README.md) for credential-free ESP32 Arduino/ESP-IDF
builds and native tests, and [UART protocol evidence](doc/protocol.md) for supported
response layouts and remaining model-specific verification gaps.

The development dependency baseline is ESPHome 2026.8.2. CI covers both ESP32
framework configurations and two independent AC instances. Hardware timing,
additional model capabilities, and named fault/preset feedback still require
device-specific evidence.

## Acknowledgments

This project was built based on [esphome_airconintl](https://github.com/pslawinski/esphome_airconintl).

## License

This project is released under the MIT License.
