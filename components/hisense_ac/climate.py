import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uart, sensor
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_DEVICE_CLASS,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_FREQUENCY,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_PERCENT,
)

DEPENDENCIES = ['uart']
AUTO_LOAD = ['sensor']

hisense_ac_ns = cg.esphome_ns.namespace('hisense_ac')
HisenseAC = hisense_ac_ns.class_('HisenseAC', climate.Climate, cg.PollingComponent, uart.UARTDevice)

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

SENSOR_CONFIG_SCHEMA = sensor.sensor_schema().extend({
    cv.GenerateID(CONF_ID): cv.declare_id(sensor.Sensor),
    cv.Required(CONF_NAME): cv.string,
    cv.Optional(CONF_UNIT_OF_MEASUREMENT): cv.string,
    cv.Optional(CONF_DEVICE_CLASS): cv.string,
})

CONFIG_SCHEMA = climate.climate_schema(HisenseAC).extend({
    cv.GenerateID(): cv.declare_id(HisenseAC),
    cv.Optional(CONF_TEMP_UNIT, default='CELSIUS'): cv.enum(TEMP_UNITS, upper=True),
    cv.Optional(CONF_COMPRESSOR_FREQUENCY): SENSOR_CONFIG_SCHEMA,
    cv.Optional(CONF_COMPRESSOR_FREQUENCY_SETTING): SENSOR_CONFIG_SCHEMA,
    cv.Optional(CONF_COMPRESSOR_FREQUENCY_SEND): SENSOR_CONFIG_SCHEMA,
    cv.Optional(CONF_OUTDOOR_TEMPERATURE): SENSOR_CONFIG_SCHEMA,
    cv.Optional(CONF_OUTDOOR_CONDENSER_TEMPERATURE): SENSOR_CONFIG_SCHEMA,
    cv.Optional(CONF_COMPRESSOR_EXHAUST_TEMPERATURE): SENSOR_CONFIG_SCHEMA,
    cv.Optional(CONF_TARGET_EXHAUST_TEMPERATURE): SENSOR_CONFIG_SCHEMA,
    cv.Optional(CONF_INDOOR_PIPE_TEMPERATURE): SENSOR_CONFIG_SCHEMA,
    cv.Optional(CONF_INDOOR_HUMIDITY_SETTING): SENSOR_CONFIG_SCHEMA,
    cv.Optional(CONF_INDOOR_HUMIDITY_STATUS): SENSOR_CONFIG_SCHEMA,
}).extend(cv.polling_component_schema('5s')).extend(uart.UART_DEVICE_SCHEMA)

async def setup_sensor(config, key, var_name, unit=None, device_class=None):
    if key in config:
        conf = config[key]
        sens = await sensor.new_sensor(conf)
        if unit:
            sens.set_unit_of_measurement(unit)
        if device_class:
            sens.set_device_class(device_class)
        cg.add(getattr(var_name, f"set_{key}")(sens))

async def to_code(config):
    uart_component = await cg.get_variable(config[uart.CONF_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_component)
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    await uart.register_uart_device(var, config)

    # Initialize temperature unit
    cg.add(var.set_temperature_unit(config[CONF_TEMP_UNIT]))

    # Setup sensors
    await setup_sensor(config, CONF_COMPRESSOR_FREQUENCY, var, UNIT_HERTZ, DEVICE_CLASS_FREQUENCY)
    await setup_sensor(config, CONF_COMPRESSOR_FREQUENCY_SETTING, var, UNIT_HERTZ, DEVICE_CLASS_FREQUENCY)
    await setup_sensor(config, CONF_COMPRESSOR_FREQUENCY_SEND, var, UNIT_HERTZ, DEVICE_CLASS_FREQUENCY)
    await setup_sensor(config, CONF_OUTDOOR_TEMPERATURE, var, UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE)
    await setup_sensor(config, CONF_OUTDOOR_CONDENSER_TEMPERATURE, var, UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE)
    await setup_sensor(config, CONF_COMPRESSOR_EXHAUST_TEMPERATURE, var, UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE)
    await setup_sensor(config, CONF_TARGET_EXHAUST_TEMPERATURE, var, UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE)
    await setup_sensor(config, CONF_INDOOR_PIPE_TEMPERATURE, var, UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE)
    await setup_sensor(config, CONF_INDOOR_HUMIDITY_SETTING, var, UNIT_PERCENT, DEVICE_CLASS_HUMIDITY)
    await setup_sensor(config, CONF_INDOOR_HUMIDITY_STATUS, var, UNIT_PERCENT, DEVICE_CLASS_HUMIDITY)
