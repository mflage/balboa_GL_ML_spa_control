from esphome.components import climate
import esphome.codegen as cg

from . import BALBOA_GL_CHILD_SCHEMA, CONF_BALBOA_GL_ID, balboa_gl_ns

BalboaGLClimate = balboa_gl_ns.class_("BalboaGLClimate", climate.Climate)

CONFIG_SCHEMA = climate.climate_schema(BalboaGLClimate).extend(BALBOA_GL_CHILD_SCHEMA)


async def to_code(config):
    var = await climate.new_climate(config)
    hub = await cg.get_variable(config[CONF_BALBOA_GL_ID])
    cg.add(hub.set_climate(var))
