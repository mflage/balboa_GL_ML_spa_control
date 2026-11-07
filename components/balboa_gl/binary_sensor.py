import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from . import BALBOA_GL_CHILD_SCHEMA, CONF_BALBOA_GL_ID

CONF_HEATER = "heater"

CONFIG_SCHEMA = BALBOA_GL_CHILD_SCHEMA.extend(
    {
        cv.Optional(CONF_HEATER): binary_sensor.binary_sensor_schema(icon="mdi:radiator"),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BALBOA_GL_ID])
    if heater := config.get(CONF_HEATER):
        cg.add(hub.set_heater_sensor(await binary_sensor.new_binary_sensor(heater)))
