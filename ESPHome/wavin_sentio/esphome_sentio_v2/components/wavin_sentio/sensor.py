"""Sensor platform for Wavin Sentio"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_TYPE,
    CONF_CHANNEL,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
    UNIT_CELSIUS,
)
from . import wavin_sentio_ns, CONF_WAVIN_SENTIO_ID, WavinSentio

DEPENDENCIES = ["wavin_sentio"]

CONF_SENSOR_TYPE_BATTERY = "battery"
CONF_SENSOR_TYPE_TEMPERATURE = "temperature"
CONF_SENSOR_TYPE_FLOOR_TEMPERATURE = "floor_temperature"
CONF_SENSOR_TYPE_COMFORT_SETPOINT = "comfort_setpoint"

WavinSentioSensor = wavin_sentio_ns.class_("WavinSentioSensor", sensor.Sensor, cg.Component)

CONFIG_SCHEMA = sensor.sensor_schema(
    WavinSentioSensor,
    accuracy_decimals=1,
).extend({
    cv.GenerateID(CONF_WAVIN_SENTIO_ID): cv.use_id(WavinSentio),
    cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=16),
    cv.Required(CONF_TYPE): cv.enum({
        CONF_SENSOR_TYPE_BATTERY: CONF_SENSOR_TYPE_BATTERY,
        CONF_SENSOR_TYPE_TEMPERATURE: CONF_SENSOR_TYPE_TEMPERATURE,
        CONF_SENSOR_TYPE_FLOOR_TEMPERATURE: CONF_SENSOR_TYPE_FLOOR_TEMPERATURE,
        CONF_SENSOR_TYPE_COMFORT_SETPOINT: CONF_SENSOR_TYPE_COMFORT_SETPOINT,
    }),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    
    parent = await cg.get_variable(config[CONF_WAVIN_SENTIO_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    
    sensor_type = config[CONF_TYPE]
    
    if sensor_type == CONF_SENSOR_TYPE_BATTERY:
        cg.add(var.set_sensor_type(0))  # Battery
        if not config.get(sensor.CONF_UNIT_OF_MEASUREMENT):
            cg.add(var.set_unit_of_measurement(UNIT_PERCENT))
        if not config.get(sensor.CONF_DEVICE_CLASS):
            cg.add(var.set_device_class(DEVICE_CLASS_BATTERY))
    elif sensor_type == CONF_SENSOR_TYPE_TEMPERATURE:
        cg.add(var.set_sensor_type(1))  # Temperature
        if not config.get(sensor.CONF_UNIT_OF_MEASUREMENT):
            cg.add(var.set_unit_of_measurement(UNIT_CELSIUS))
        if not config.get(sensor.CONF_DEVICE_CLASS):
            cg.add(var.set_device_class(DEVICE_CLASS_TEMPERATURE))
    elif sensor_type == CONF_SENSOR_TYPE_FLOOR_TEMPERATURE:
        cg.add(var.set_sensor_type(2))  # Floor Temperature
        if not config.get(sensor.CONF_UNIT_OF_MEASUREMENT):
            cg.add(var.set_unit_of_measurement(UNIT_CELSIUS))
        if not config.get(sensor.CONF_DEVICE_CLASS):
            cg.add(var.set_device_class(DEVICE_CLASS_TEMPERATURE))
    elif sensor_type == CONF_SENSOR_TYPE_COMFORT_SETPOINT:
        cg.add(var.set_sensor_type(3))  # Comfort Setpoint
        if not config.get(sensor.CONF_UNIT_OF_MEASUREMENT):
            cg.add(var.set_unit_of_measurement(UNIT_CELSIUS))
        if not config.get(sensor.CONF_DEVICE_CLASS):
            cg.add(var.set_device_class(DEVICE_CLASS_TEMPERATURE))
