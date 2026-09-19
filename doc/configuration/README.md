# Configuration

For a full working example configuration see the configuration folder.

The checked-in examples use a local component path relative to
`doc/configuration/examples`. When copying them elsewhere, adjust that path or
use the GitHub source below.

## Installation

Add the external component to your ESPHome configuration:

```yaml
external_components:
  - source: github://akrabi/hisense_ac_esphome
    components: [hisense_ac]
```


## Basic Configuration

```yaml
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

## Full Configuration with All Sensors

```yaml
climate:
  - platform: hisense_ac
    name: "Air Conditioner"
    temperature_unit: CELSIUS
    uart_id: uart_bus
    display:
      name: "Display"
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
- **temperature_unit** (*Optional*, string): The AC protocol's temperature unit. Can be `CELSIUS` or `FAHRENHEIT`. Defaults to `CELSIUS`. This is not the Home Assistant display-unit preference.
- **display** (*Optional*, switch): Exposes the indoor unit display as a switch. Set `name` to enable it in Home Assistant.
- **optimistic** (*Optional*, boolean): Defaults to `false`. When enabled, accepted climate and display requests appear immediately, then reconcile with device status.
- **uart_id** (*Optional*, ID): Selects the top-level UART bus; required when multiple buses are configured. Each AC needs a dedicated bus.
- **update_interval** (*Optional*, time): Status polling interval. Defaults to `5s`.
- **uart** (*Required*, top-level configuration): UART bus configuration, separate from `climate`
  - **tx_pin** (*Required*, pin): TX pin
  - **rx_pin** (*Required*, pin): RX pin
  - **baud_rate** (*Required*, int): Baud rate (must be 9600)

### Optional Sensor Configuration
All sensor configurations follow the same pattern and are optional:
- **name** (*Required if sensor enabled*, string): Name of the sensor
- **unit_of_measurement** (*Optional*, string): Unit for the sensor
- **device_class** (*Optional*, string): Home Assistant device class

Frequency sensors default to Hz/frequency, temperature sensors to Celsius/
temperature, and humidity sensors to percent/humidity, all with measurement state
class and zero decimal places. Standard ESPHome sensor options, including
metadata overrides and filters, remain available. Metadata overrides alone do
not convert measurement values.

## State reporting and migration

Earlier versions published requested controls immediately. The default is now
**device-reported state**: commands are queued and the displayed state changes
when valid status arrives. Use `optimistic: true` for immediate climate/display
feedback. Measurements and compressor action always come from the AC.

Pending requests have a bounded lifetime and failures raise a component warning.
In optimistic mode, failure removes the pending state and restores the latest
known report; if no report exists, the component does not invent a replacement.
A timeout does not prove that the AC rejected a command. Controls, especially
swing toggles, are never blindly retried. Preset commands remain available, but
preset status bits are unverified: no confirmed preset is fabricated, and sending
a preset raises an unverified-operation warning.

Each AC requires its own native ESP32 UART, RX and TX, at 9600/8N1.
Do not share the bus with other devices, `uart.write`, UART debug callbacks,
manual lambdas that write to the UART, or UART logger output. Component-level
`flow_control_pin`/RS485 direction management is not supported. These constraints
allow complete short commands to fit the idle hardware FIFO without waiting for
line transmission. No blocking flush is used. Arduino and ESP-IDF ESP32
configurations use the IDF UART backend; other platforms/backends are not covered.

`temperature_unit` must match the device's protocol units. Climate values are
always Celsius internally; Home Assistant performs presentation conversion.
Fahrenheit controls use whole Fahrenheit steps (5/9 C); their default visual
range corresponds to 61-86 F. Celsius defaults remain 16-30 C. Visual overrides
must stay within encodable limits and use whole protocol-degree boundaries and
steps. The component does not change the AC's own display-unit setting.

The display status bit (`back_led`, byte 37, mask `0x80`) is verified on the
ACOND ASTI-09UW4RVEDC00 / AEH-W4B1 and the maintainer's device. This is not a
guarantee that every model supports display control.

## Optional diagnostics and capabilities

Add these under the `hisense_ac` climate entry to expose diagnostic entities:

```yaml
communication_connected:
  name: "AC Communication"
last_status_age:
  name: "AC Last Status Age"
invalid_frame_count:
  name: "AC Invalid Frames"
response_timeout_count:
  name: "AC Response Timeouts"
queue_rejection_count:
  name: "AC Queue Rejections"
```

Communication is false before the first valid report, after a failed standalone
status transaction, or when no status arrives for three polling intervals
(at least 10 seconds). It recovers on a valid status, independently of warnings
about unconfirmed controls. Status age is seconds since the last valid report
and remains unknown until one arrives. Counters reset on reboot: invalid frames
count malformed framed candidates and partial-frame timeouts, response timeouts
count unanswered status transactions, and queue rejections count operations
refused for lack of capacity (not invalid user controls). A valid but unsupported
status layout is logged and ignored, not counted as a checksum error.

Limit advertised controls for a unit with fewer capabilities:

```yaml
supported_modes: ["OFF", COOL, DRY, FAN_ONLY]
supported_swing_modes: ["OFF", VERTICAL]
supported_presets: []
```

Defaults advertise all existing component capabilities. Mode subsets must
include `"OFF"`; swing and preset lists may be empty. These are restrictions,
not automatic detection or support for new protocol variants. Disabled controls
are rejected even when invoked directly through an automation.
Named hardware faults and confirmed preset feedback remain unexposed until
their bit meanings and model applicability are verified.

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
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600
  data_bits: 8
  parity: NONE
  stop_bits: 1
```

### Debug Logging

Enable detailed logging by adding to your configuration:

```yaml
logger:
  level: DEBUG
  baud_rate: 0
```
