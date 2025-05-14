# Hisense Air Conditioner Climate Component for ESPHome

An ESPHome external component for controlling Hisense air conditioners.

## Installation

1. Copy the `components/hisense_ac` directory to your ESPHome configuration directory
2. Add the external component to your ESPHome configuration:

```yaml
external_components:
  - source: components
    components: [hisense_ac]
```

## Usage

Example configuration:

```yaml
climate:
  - platform: hisense_ac
    name: "Air Conditioner"
    temperature_unit: CELSIUS  # or FAHRENHEIT
    uart:
      tx_pin: GPIO1
      rx_pin: GPIO3
      baud_rate: 9600
```

## Configuration Variables

- **name** (*Required*, string): The name of the climate device
- **temperature_unit** (*Optional*, string): The temperature unit to use. Can be CELSIUS or FAHRENHEIT. Defaults to CELSIUS.
- **uart** (*Required*): UART bus configuration
  - **tx_pin** (*Required*, pin): TX pin
  - **rx_pin** (*Required*, pin): RX pin
  - **baud_rate** (*Required*, int): Baud rate (should be 9600)

## Features

- Modes: Heat, Cool, Fan Only, Dry
- Fan speeds: Auto, Low, Medium, High, Quiet
- Swing modes: Off, Vertical, Horizontal, Both
- Temperature control
- Presets: None, Boost (Turbo), Eco (Energy Save)
- Multiple temperature sensors for monitoring system status
