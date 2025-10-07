"""Climate platform for Wavin Sentio"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.const import CONF_ID, CONF_NAME, CONF_CHANNEL
from . import wavin_sentio_ns, CONF_WAVIN_SENTIO_ID, WavinSentio

DEPENDENCIES = ["wavin_sentio"]

CONF_MEMBERS = "members"
CONF_USE_FLOOR_TEMPERATURE = "use_floor_temperature"

WavinSentioClimate = wavin_sentio_ns.class_("WavinSentioClimate", climate.Climate, cg.Component)

# Climate schema supports either single channel or group of channels
CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(WavinSentioClimate),
    cv.GenerateID(CONF_WAVIN_SENTIO_ID): cv.use_id(WavinSentio),
    cv.Optional(CONF_CHANNEL): cv.int_range(min=1, max=16),
    cv.Optional(CONF_MEMBERS): cv.ensure_list(cv.int_range(min=1, max=16)),
    cv.Optional(CONF_USE_FLOOR_TEMPERATURE, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

# Validate that either channel or members is specified, but not both
def validate_climate_config(config):
    has_channel = CONF_CHANNEL in config
    has_members = CONF_MEMBERS in config
    
    if not has_channel and not has_members:
        raise cv.Invalid("Must specify either 'channel' or 'members'")
    
    if has_channel and has_members:
        raise cv.Invalid("Cannot specify both 'channel' and 'members'")
    
    if has_members and config.get(CONF_USE_FLOOR_TEMPERATURE, False):
        raise cv.Invalid("use_floor_temperature can only be used with single channel, not groups")
    
    return config

FINAL_VALIDATE_SCHEMA = validate_climate_config


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    
    parent = await cg.get_variable(config[CONF_WAVIN_SENTIO_ID])
    cg.add(var.set_parent(parent))
    
    if CONF_CHANNEL in config:
        cg.add(var.set_channel(config[CONF_CHANNEL]))
    
    if CONF_MEMBERS in config:
        cg.add(var.set_members(config[CONF_MEMBERS]))
    
    if config.get(CONF_USE_FLOOR_TEMPERATURE, False):
        cg.add(var.set_use_floor_temperature(True))
