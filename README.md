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

- **Advanced Monitoring**
  - Compressor frequency monitoring and control
  - Multiple temperature sensors
  - Indoor humidity monitoring
  - System status tracking

## Hardware

### Requirements
A compatible AC unit with a Hisense Wifi Module (see [compatible devices](doc/hardware/COMPATIBLE_DEVICES.md) for tested models)

### Setup
See the [hardware](doc/hardware/README.md) documentation for further details.

## Configuration

For a complete example configuration including ESP32 setup, WiFi configuration, and all available options, see [configuration](doc/configuration/README.md).


## Contributing

Feel free to submit issues and pull requests on GitHub.

## Acknowledgments

This project was built based on [esphome_airconintl](https://github.com/pslawinski/esphome_airconintl).

## License

This project is released under the MIT License.
