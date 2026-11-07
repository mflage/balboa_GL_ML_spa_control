# Balboa GL ML Spa Control

While there is the excellent https://github.com/ccutrer/balboa_worldwide_app project that handled the new WiFi capable BP range of controllers, their project is *NOT* compatible with the older GL/EL range such as the GL2000, GL2001 or EL2000 - This one is.

The aim of this project is to achieve similar Home Assistant integration as the app from ccutrer - we are about 95% of the way there

If you have the Balboa GS series please try - https://github.com/MagnusPer/Balboa-GS510SZ or https://github.com/kgstorm/Balboa-GS100-with-VL260-topside

[Install Guide](https://github.com/netmindz/balboa_GL_ML_spa_control/wiki/Install-Guide)

## ESPHome component
Instead of flashing the standalone firmware you can use this as an [ESPHome external component](https://esphome.io/components/external_components.html) (ESP32, ESP-IDF or Arduino). See [esphome-example/hottub.yaml](esphome-example/hottub.yaml) for a full config:

```yaml
external_components:
  - source: github://mflage/balboa_GL_ML_spa_control
    components: [balboa_gl]

uart: {id: tub_uart, rx_pin: GPIO19, tx_pin: GPIO23, baud_rate: 115200}
balboa_gl: {uart_id: tub_uart, panel_select_pin: GPIO18, flow_control_pin: GPIO22}
```
Platforms: `climate` (target/current temp), `select` (`pump1`, `pump2`, `aux`, `mode`; `dual_speed: true` for 2-speed pumps), `switch` (`light`), `binary_sensor` (`heater`), `text_sensor` (`status`).

## Current state
App for ESP32 or ESP8266 that sends current status and allows remote control using Home Assistant and also simple web status page with live updates via WebSocket

![gl2000](GL2000_pcb_2.jpg)

![screenshot](Screenshot.png)

# Source Info
Original discssion regarding support for spa controllers with 8 pin rather than 4 pin main panels
* 8-pin Molex https://github.com/ccutrer/balboa_worldwide_app/issues/14


# Connection
* Pin 1+3 - RS485
* Pin 2,4 - Unknown
* Pin 5 - Main panel port selector
* Pin 6   - 9.7V - PSU - limited current avail
* Pin 7+8 - GND

https://www.digikey.co.uk/en/products/detail/molex/2147562082/12180458

# Docs
https://netmindz.github.io/balboa_GL_ML_spa_control/
