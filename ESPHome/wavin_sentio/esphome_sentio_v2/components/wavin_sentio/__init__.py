"""Wavin Sentio ESPHome Component"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import modbus
from esphome.const import CONF_ID

DEPENDENCIES = ["modbus"]
AUTO_LOAD = ["climate", "sensor", "binary_sensor", "switch"]
CODEOWNERS = ["@yourusername"]

CONF_WAVIN_SENTIO_ID = "wavin_sentio_id"
CONF_UPDATE_INTERVAL = "update_interval"
CONF_POLL_CHANNELS_PER_CYCLE = "poll_channels_per_cycle"
CONF_FLOW_CONTROL_PIN = "flow_control_pin"
CONF_TX_ENABLE_PIN = "tx_enable_pin"

# Channel friendly names (up to 16 channels)
CHANNEL_FRIENDLY_NAME_KEYS = [f"channel_{i:02d}_friendly_name" for i in range(1, 17)]

wavin_sentio_ns = cg.esphome_ns.namespace("wavin_sentio")
WavinSentio = wavin_sentio_ns.class_("WavinSentio", cg.PollingComponent, modbus.ModbusDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WavinSentio),
    cv.Optional(CONF_UPDATE_INTERVAL, default="10s"): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_POLL_CHANNELS_PER_CYCLE, default=2): cv.int_range(min=1, max=16),
    cv.Optional(CONF_FLOW_CONTROL_PIN): cv.positive_int,
    cv.Optional(CONF_TX_ENABLE_PIN): cv.positive_int,
    # Add friendly names for each channel
    **{cv.Optional(key): cv.string for key in CHANNEL_FRIENDLY_NAME_KEYS},
}).extend(cv.polling_component_schema("10s")).extend(modbus.modbus_device_schema(0x01))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_device(var, config)
    
    if CONF_FLOW_CONTROL_PIN in config:
        cg.add(var.set_flow_control_pin(config[CONF_FLOW_CONTROL_PIN]))
    
    if CONF_TX_ENABLE_PIN in config:
        cg.add(var.set_tx_enable_pin(config[CONF_TX_ENABLE_PIN]))
    
    cg.add(var.set_poll_channels_per_cycle(config[CONF_POLL_CHANNELS_PER_CYCLE]))
    
    # Set friendly names
    for i, key in enumerate(CHANNEL_FRIENDLY_NAME_KEYS, 1):
        if key in config:
            cg.add(var.set_channel_friendly_name(i, config[key]))
