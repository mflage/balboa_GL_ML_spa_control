import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select

from . import BALBOA_GL_CHILD_SCHEMA, CONF_BALBOA_GL_ID, balboa_gl_ns

BalboaGLSelect = balboa_gl_ns.class_("BalboaGLSelect", select.Select)
SelectKind = balboa_gl_ns.enum("SelectKind")

CONF_PUMP1 = "pump1"
CONF_PUMP2 = "pump2"
CONF_AUX = "aux"
CONF_MODE = "mode"
CONF_DUAL_SPEED = "dual_speed"

PUMP_SCHEMA = select.select_schema(BalboaGLSelect, icon="mdi:chart-bubble").extend(
    {cv.Optional(CONF_DUAL_SPEED, default=False): cv.boolean}
)

CONFIG_SCHEMA = BALBOA_GL_CHILD_SCHEMA.extend(
    {
        cv.Optional(CONF_PUMP1): PUMP_SCHEMA,
        cv.Optional(CONF_PUMP2): PUMP_SCHEMA,
        cv.Optional(CONF_AUX): PUMP_SCHEMA,
        cv.Optional(CONF_MODE): select.select_schema(BalboaGLSelect),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BALBOA_GL_ID])
    for key, kind in ((CONF_PUMP1, "PUMP1"), (CONF_PUMP2, "PUMP2"), (CONF_AUX, "AUX")):
        if conf := config.get(key):
            options = ["Off", "Low", "High"] if conf[CONF_DUAL_SPEED] else ["Off", "On"]
            var = await select.new_select(conf, options=options)
            cg.add(hub.set_select(getattr(SelectKind, kind), var))
    if conf := config.get(CONF_MODE):
        var = await select.new_select(conf, options=["Standard", "Economy", "Sleep"])
        cg.add(hub.set_select(SelectKind.MODE, var))
