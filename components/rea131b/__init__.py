import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

rea131b_ns = cg.esphome_ns.namespace("rea131b")
REA131B = rea131b_ns.class_("REA131B", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(REA131B),
 }).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
