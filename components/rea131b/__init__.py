import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_ID

rea131b_ns = cg.esphome_ns.namespace("rea131b")
rea131b = rea131b_ns.class_("REA131B", cg.Component, cg.CustomAPIDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(rea131b),
})


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
