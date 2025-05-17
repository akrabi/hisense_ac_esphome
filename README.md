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
- A compatible Hisense AC unit (see [COMPATIBLE_DEVICES.md](doc/COMPATIBLE_DEVICES.md) for tested models)
- ESP32 development board (tested with ESP32-DevKitC)
- RS485<->Uart(TTL) converter
- UART connection to AC unit

### Setup
TODO - add schematic and pictures

## Configuration

For a complete example configuration including ESP32 setup, WiFi configuration, and all available options, see [configuration](doc/configuration/README.md).

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
