import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import (
    climate_ir,
    sensor,
)
from esphome.const import CONF_ID
from esphome.const import (
    CONF_PRESET_ECO,
    CONF_PRESET_BOOST,
    CONF_MODEL,
    CONF_SENSOR,
)

AUTO_LOAD = ["climate_ir", "sensor"]
CONF_SWING_HORIZONTAL = "swing_horizontal"
CONF_SWING_BOTH = "swing_both"

daikin_ns = cg.esphome_ns.namespace("daikin")
DaikinClimate = daikin_ns.class_("DaikinClimate", climate_ir.ClimateIR)


Model = daikin_ns.enum("Model")
MODELS = {
    "ARC470A1": Model.MODEL_ARC470A1,
    "ARC432A14": Model.MODEL_ARC432A14,
}

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(DaikinClimate),
        cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_MODEL, default="ARC470A1"): cv.enum(MODELS, upper=True),
        cv.Optional(CONF_SWING_HORIZONTAL, default=True): cv.boolean,
        cv.Optional(CONF_SWING_BOTH, default=True): cv.boolean,
        cv.Optional(CONF_PRESET_ECO, default=True): cv.boolean,
        cv.Optional(CONF_PRESET_BOOST, default=True): cv.boolean,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate_ir.register_climate_ir(var, config)

    if CONF_SENSOR in config:
        sens = await cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))

    cg.add(var.set_swing_horizontal(config[CONF_SWING_HORIZONTAL]))
    cg.add(var.set_swing_both(config[CONF_SWING_BOTH]))
    cg.add(var.set_preset_eco(config[CONF_PRESET_ECO]))
    cg.add(var.set_preset_boost(config[CONF_PRESET_BOOST]))
    cg.add(var.set_model(config[CONF_MODEL]))


