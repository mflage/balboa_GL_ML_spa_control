import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import BALBOA_GL_CHILD_SCHEMA, CONF_BALBOA_GL_ID

CONF_STATUS = "status"

CONFIG_SCHEMA = BALBOA_GL_CHILD_SCHEMA.extend(
    {cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema()}
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BALBOA_GL_ID])
    if status := config.get(CONF_STATUS):
        cg.add(hub.set_status_sensor(await text_sensor.new_text_sensor(status)))
