import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from . import BALBOA_GL_CHILD_SCHEMA, CONF_BALBOA_GL_ID, balboa_gl_ns

BalboaGLSwitch = balboa_gl_ns.class_("BalboaGLSwitch", switch.Switch)

CONF_LIGHT = "light"

CONFIG_SCHEMA = BALBOA_GL_CHILD_SCHEMA.extend(
    {cv.Optional(CONF_LIGHT): switch.switch_schema(BalboaGLSwitch, icon="mdi:lightbulb")}
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BALBOA_GL_ID])
    if light := config.get(CONF_LIGHT):
        var = await switch.new_switch(light)
        cg.add(hub.set_light(var))
