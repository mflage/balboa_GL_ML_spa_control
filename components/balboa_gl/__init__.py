import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import uart
from esphome.const import CONF_FLOW_CONTROL_PIN, CONF_ID

CODEOWNERS = ["@mflage"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "climate", "select", "switch", "text_sensor"]

CONF_BALBOA_GL_ID = "balboa_gl_id"
CONF_PANEL_SELECT_PIN = "panel_select_pin"

balboa_gl_ns = cg.esphome_ns.namespace("balboa_gl")
BalboaGL = balboa_gl_ns.class_("BalboaGL", cg.Component, uart.UARTDevice)

# Schema fragment shared by every platform (climate, select, ...)
BALBOA_GL_CHILD_SCHEMA = cv.Schema({cv.GenerateID(CONF_BALBOA_GL_ID): cv.use_id(BalboaGL)})

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BalboaGL),
            # Pin 5 of the topside connector: LOW while the controller talks to our panel
            cv.Required(CONF_PANEL_SELECT_PIN): pins.gpio_input_pin_schema,
            # RS485 driver enable (DE/RE of the MAX485 board)
            cv.Required(CONF_FLOW_CONTROL_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "balboa_gl", baud_rate=115200, require_rx=True, require_tx=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_panel_select_pin(await cg.gpio_pin_expression(config[CONF_PANEL_SELECT_PIN])))
    cg.add(var.set_flow_control_pin(await cg.gpio_pin_expression(config[CONF_FLOW_CONTROL_PIN])))
