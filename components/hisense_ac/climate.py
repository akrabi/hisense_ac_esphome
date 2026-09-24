import math

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.core import CORE, ID
from esphome.components import binary_sensor, climate, uart, sensor, switch, number
from esphome.const import (
    CONF_ID,
    CONF_DEVICE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_VISUAL,
    CONF_MIN_TEMPERATURE,
    CONF_MAX_TEMPERATURE,
    CONF_TEMPERATURE_STEP,
    CONF_TARGET_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_FREQUENCY,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_PERCENT,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_DURATION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    UNIT_SECOND,
    CONF_SUPPORTED_MODES,
    CONF_SUPPORTED_SWING_MODES,
    CONF_SUPPORTED_PRESETS,
)

DEPENDENCIES = ['uart']
AUTO_LOAD = ['sensor', 'switch', 'binary_sensor', 'number']

hisense_ac_ns = cg.esphome_ns.namespace('hisense_ac')
HisenseAC = hisense_ac_ns.class_('HisenseAC', climate.Climate, cg.PollingComponent, uart.UARTDevice)
HisenseACDisplaySwitch = hisense_ac_ns.class_('HisenseACDisplaySwitch', switch.Switch)
HisenseACDryOffsetNumber = hisense_ac_ns.class_('HisenseACDryOffsetNumber', number.Number)

CONF_TEMP_UNIT = 'temperature_unit'
TempUnit = hisense_ac_ns.enum('Temperature_Unit')
TEMP_UNITS = {
    'CELSIUS': TempUnit.CELSIUS,
    'FAHRENHEIT': TempUnit.FAHRENHEIT,
}

# Sensor configuration constants
CONF_COMPRESSOR_FREQUENCY = 'compressor_frequency'
CONF_COMPRESSOR_FREQUENCY_SETTING = 'compressor_frequency_setting'
CONF_COMPRESSOR_FREQUENCY_SEND = 'compressor_frequency_send'
CONF_OUTDOOR_TEMPERATURE = 'outdoor_temperature'
CONF_OUTDOOR_CONDENSER_TEMPERATURE = 'outdoor_condenser_temperature'
CONF_COMPRESSOR_EXHAUST_TEMPERATURE = 'compressor_exhaust_temperature'
CONF_TARGET_EXHAUST_TEMPERATURE = 'target_exhaust_temperature'
CONF_INDOOR_PIPE_TEMPERATURE = 'indoor_pipe_temperature'
CONF_INDOOR_HUMIDITY_SETTING = 'indoor_humidity_setting'
CONF_INDOOR_HUMIDITY_STATUS = 'indoor_humidity_status'
CONF_DISPLAY = 'display'
CONF_OPTIMISTIC = 'optimistic'
CONF_DRY_OFFSET = 'dry_offset'
CONF_COMMUNICATION_CONNECTED = 'communication_connected'
CONF_LAST_STATUS_AGE = 'last_status_age'
CONF_INVALID_FRAME_COUNT = 'invalid_frame_count'
CONF_RESPONSE_TIMEOUT_COUNT = 'response_timeout_count'
CONF_QUEUE_REJECTION_COUNT = 'queue_rejection_count'

SUPPORTED_MODES = {key: climate.CLIMATE_MODES[key] for key in ("OFF", "COOL", "HEAT", "DRY", "FAN_ONLY")}
SUPPORTED_SWING_MODES = {key: climate.CLIMATE_SWING_MODES[key] for key in ("OFF", "VERTICAL", "HORIZONTAL", "BOTH")}
SUPPORTED_PRESETS = {key: climate.CLIMATE_PRESETS[key] for key in ("NONE", "BOOST", "ECO")}


def validate_modes(value):
    if "OFF" not in value:
        raise cv.Invalid("supported_modes must include OFF")
    return value

FREQUENCY_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_HERTZ, device_class=DEVICE_CLASS_FREQUENCY,
    accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT,
)
TEMPERATURE_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS, device_class=DEVICE_CLASS_TEMPERATURE,
    accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT,
)
HUMIDITY_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_PERCENT, device_class=DEVICE_CLASS_HUMIDITY,
    accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT,
)
COUNTER_SENSOR_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=0, state_class=STATE_CLASS_TOTAL_INCREASING,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
)


def validate_visual(config):
    visual = config.get(CONF_VISUAL, {})
    fahrenheit = config[CONF_TEMP_UNIT] == "FAHRENHEIT"
    minimum, maximum = ((61 - 32) * 5 / 9, (90 - 32) * 5 / 9) if fahrenheit else (16, 32)
    step = 5 / 9 if fahrenheit else 1
    for key in (CONF_MIN_TEMPERATURE, CONF_MAX_TEMPERATURE):
        if key not in visual:
            continue
        value = visual[key]
        device_value = value * 9 / 5 + 32 if fahrenheit else value
        if not math.isfinite(value) or not minimum - 0.001 <= value <= maximum + 0.001:
            raise cv.Invalid(f"{key} must be within the encodable Celsius range {minimum:g} to {maximum:g}")
        if abs(device_value - round(device_value)) > 0.001:
            raise cv.Invalid(f"{key} must represent a whole degree in the AC protocol unit")
    target_step = visual.get(CONF_TEMPERATURE_STEP, {}).get(CONF_TARGET_TEMPERATURE, step)
    if not math.isfinite(target_step) or target_step < step - 0.001 or abs(target_step / step - round(target_step / step)) > 0.001:
        raise cv.Invalid("visual target temperature step must be a positive whole multiple of the AC protocol step")
    if visual.get(CONF_MIN_TEMPERATURE, minimum) > visual.get(CONF_MAX_TEMPERATURE, 30):
        raise cv.Invalid("visual minimum temperature must not exceed the maximum")
    return config


def validate_dry_offset(config):
    if CONF_DRY_OFFSET in config:
        if config[CONF_TEMP_UNIT] != "CELSIUS":
            raise cv.Invalid("dry_offset requires temperature_unit: CELSIUS")
        if "DRY" not in config.get(CONF_SUPPORTED_MODES, SUPPORTED_MODES):
            raise cv.Invalid("dry_offset requires DRY in supported_modes")
    return config


CONFIG_SCHEMA = cv.All(climate.climate_schema(HisenseAC).extend({
    cv.GenerateID(): cv.declare_id(HisenseAC),
    cv.Optional(CONF_TEMP_UNIT, default='CELSIUS'): cv.enum(TEMP_UNITS, upper=True),
    cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
    cv.Optional(CONF_DRY_OFFSET): number.number_schema(
        HisenseACDryOffsetNumber, icon="mdi:water-percent",
    ).extend({
        cv.Optional(CONF_UNIT_OF_MEASUREMENT): cv.one_of(""),
        cv.Optional(CONF_DEVICE_CLASS): cv.one_of(""),
    }),
    cv.Optional(CONF_SUPPORTED_MODES): cv.All(cv.ensure_list(cv.enum(SUPPORTED_MODES, upper=True)), validate_modes),
    cv.Optional(CONF_SUPPORTED_SWING_MODES): cv.ensure_list(cv.enum(SUPPORTED_SWING_MODES, upper=True)),
    cv.Optional(CONF_SUPPORTED_PRESETS): cv.ensure_list(cv.enum(SUPPORTED_PRESETS, upper=True)),
    cv.Optional(CONF_COMMUNICATION_CONNECTED): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_CONNECTIVITY, entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    cv.Optional(CONF_LAST_STATUS_AGE): sensor.sensor_schema(
        unit_of_measurement=UNIT_SECOND, device_class=DEVICE_CLASS_DURATION,
        accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    cv.Optional(CONF_INVALID_FRAME_COUNT): COUNTER_SENSOR_SCHEMA,
    cv.Optional(CONF_RESPONSE_TIMEOUT_COUNT): COUNTER_SENSOR_SCHEMA,
    cv.Optional(CONF_QUEUE_REJECTION_COUNT): COUNTER_SENSOR_SCHEMA,
    cv.Optional(CONF_COMPRESSOR_FREQUENCY): FREQUENCY_SENSOR_SCHEMA,
    cv.Optional(CONF_COMPRESSOR_FREQUENCY_SETTING): FREQUENCY_SENSOR_SCHEMA,
    cv.Optional(CONF_COMPRESSOR_FREQUENCY_SEND): FREQUENCY_SENSOR_SCHEMA,
    cv.Optional(CONF_OUTDOOR_TEMPERATURE): TEMPERATURE_SENSOR_SCHEMA,
    cv.Optional(CONF_OUTDOOR_CONDENSER_TEMPERATURE): TEMPERATURE_SENSOR_SCHEMA,
    cv.Optional(CONF_COMPRESSOR_EXHAUST_TEMPERATURE): TEMPERATURE_SENSOR_SCHEMA,
    cv.Optional(CONF_TARGET_EXHAUST_TEMPERATURE): TEMPERATURE_SENSOR_SCHEMA,
    cv.Optional(CONF_INDOOR_PIPE_TEMPERATURE): TEMPERATURE_SENSOR_SCHEMA,
    cv.Optional(CONF_INDOOR_HUMIDITY_SETTING): HUMIDITY_SENSOR_SCHEMA,
    cv.Optional(CONF_INDOOR_HUMIDITY_STATUS): HUMIDITY_SENSOR_SCHEMA,
    cv.Optional(CONF_DISPLAY): switch.switch_schema(HisenseACDisplaySwitch),
}).extend(cv.polling_component_schema('5s')).extend(uart.UART_DEVICE_SCHEMA), validate_visual, validate_dry_offset)


def validate_exclusive_uart(config):
    if not CORE.is_esp32:
        raise cv.Invalid("Hisense asynchronous transport requires the ESP32 IDF UART backend")
    full = fv.full_config.get()
    uart_id = config[uart.CONF_UART_ID]
    path = full.get_path_for_id(uart_id)[:-1]
    bus = full.get_config_for_path(path)
    if not str(bus[CONF_ID].type).endswith("::IDFUARTComponent"):
        raise cv.Invalid("Hisense requires a dedicated native ESP32 hardware UART")
    if "debug" in bus or "flow_control_pin" in bus:
        raise cv.Invalid("Hisense dedicated UART cannot use debug callbacks/dummy receiver or flow control")

    def references(value):
        if isinstance(value, ID):
            return int(value.id == uart_id.id)
        if isinstance(value, dict):
            return sum(references(item) for item in value.values())
        if isinstance(value, list):
            return sum(references(item) for item in value)
        return 0

    # One declaration and this device reference. Includes uart.write actions
    # and devices that do not implement UART's own final ownership validation.
    if references(full) != 2:
        raise cv.Invalid("Hisense requires exclusive UART ownership; other devices and uart.write are unsupported")
    return config


FINAL_VALIDATE_SCHEMA = cv.All(
    validate_exclusive_uart,
    uart.final_validate_device_schema(
        "hisense_ac", baud_rate=9600, require_tx=True, require_rx=True,
        data_bits=8, parity="NONE", stop_bits=1,
    ),
)


async def setup_sensor(config, key, var_name):
    if key in config:
        conf = config[key]
        sens = await sensor.new_sensor(conf)
        cg.add(getattr(var_name, f"set_{key}")(sens))

async def to_code(config):
    uart_component = await cg.get_variable(config[uart.CONF_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_component)
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    await uart.register_uart_device(var, config)

    # Initialize temperature unit
    cg.add(var.set_temperature_unit(config[CONF_TEMP_UNIT]))
    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
    if CONF_DRY_OFFSET in config:
        conf = config[CONF_DRY_OFFSET]
        dry_offset = cg.new_Pvariable(conf[CONF_ID], var)
        await number.register_number(dry_offset, conf, min_value=-7, max_value=7, step=1)
        cg.add(var.set_dry_offset_number(dry_offset))
    for key in (CONF_SUPPORTED_MODES, CONF_SUPPORTED_SWING_MODES, CONF_SUPPORTED_PRESETS):
        if key in config:
            cg.add(getattr(var, f"set_{key}")(config[key]))
    if CONF_COMMUNICATION_CONNECTED in config:
        connected = await binary_sensor.new_binary_sensor(config[CONF_COMMUNICATION_CONNECTED])
        cg.add(var.set_communication_connected(connected))

    # Setup sensors
    for key in (
        CONF_COMPRESSOR_FREQUENCY, CONF_COMPRESSOR_FREQUENCY_SETTING,
        CONF_COMPRESSOR_FREQUENCY_SEND, CONF_OUTDOOR_TEMPERATURE,
        CONF_OUTDOOR_CONDENSER_TEMPERATURE, CONF_COMPRESSOR_EXHAUST_TEMPERATURE,
        CONF_TARGET_EXHAUST_TEMPERATURE, CONF_INDOOR_PIPE_TEMPERATURE,
        CONF_INDOOR_HUMIDITY_SETTING, CONF_INDOOR_HUMIDITY_STATUS,
        CONF_LAST_STATUS_AGE, CONF_INVALID_FRAME_COUNT, CONF_RESPONSE_TIMEOUT_COUNT, CONF_QUEUE_REJECTION_COUNT,
    ):
        await setup_sensor(config, key, var)

    if CONF_DISPLAY in config:
        display_config = config[CONF_DISPLAY]
        display_switch = cg.new_Pvariable(display_config[CONF_ID], var)
        await switch.register_switch(display_switch, display_config)
        cg.add(var.set_display_switch(display_switch))
