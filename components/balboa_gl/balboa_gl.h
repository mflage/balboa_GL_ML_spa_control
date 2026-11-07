#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/select/select.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"

#include <array>
#include <deque>
#include <string>

namespace esphome {
namespace balboa_gl {

class BalboaGL;

using Command = std::array<uint8_t, 9>;

enum SelectKind : uint8_t { PUMP1, PUMP2, AUX, MODE, SELECT_KIND_COUNT };

class BalboaGLClimate : public climate::Climate, public Parented<BalboaGL> {
 protected:
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;
};

class BalboaGLSelect : public select::Select, public Parented<BalboaGL> {
 public:
  void set_kind(SelectKind kind) { this->kind_ = kind; }

 protected:
  void control(const std::string &value) override;
  SelectKind kind_{PUMP1};
};

class BalboaGLSwitch : public switch_::Switch, public Parented<BalboaGL> {
 protected:
  void write_state(bool state) override;
};

/// Balboa GL/ML series controller, 8-pin topside connector (RS485 + panel select on pin 5).
class BalboaGL : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_panel_select_pin(GPIOPin *pin) { this->panel_select_pin_ = pin; }
  void set_flow_control_pin(GPIOPin *pin) { this->flow_control_pin_ = pin; }
  void set_climate(BalboaGLClimate *climate) {
    this->climate_ = climate;
    climate->set_parent(this);
  }
  void set_light(BalboaGLSwitch *light) {
    this->light_ = light;
    light->set_parent(this);
  }
  void set_select(SelectKind kind, BalboaGLSelect *select) {
    this->selects_[kind] = select;
    select->set_parent(this);
    select->set_kind(kind);
  }
  void set_heater_sensor(binary_sensor::BinarySensor *sensor) { this->heater_sensor_ = sensor; }
  void set_status_sensor(text_sensor::TextSensor *sensor) { this->status_sensor_ = sensor; }

  // Called by the entities
  void set_target_temperature(float target);
  void toggle_light();
  void change_select(SelectKind kind, size_t current, size_t target, size_t count);

 protected:
  void drain_rx_();
  void handle_message_(const uint8_t *buf, size_t len);
  void handle_status_(const uint8_t *buf);
  void send_command_();
  void enqueue_(const Command &cmd, size_t count = 1);
  void set_status_(const std::string &status);
  void publish_select_(SelectKind kind, size_t index);
  void publish_climate_();

  GPIOPin *panel_select_pin_{nullptr};
  GPIOPin *flow_control_pin_{nullptr};
  HighFrequencyLoopRequester high_freq_;

  BalboaGLClimate *climate_{nullptr};
  BalboaGLSwitch *light_{nullptr};
  BalboaGLSelect *selects_[SELECT_KIND_COUNT]{};
  binary_sensor::BinarySensor *heater_sensor_{nullptr};
  text_sensor::TextSensor *status_sensor_{nullptr};

  std::deque<Command> queue_;
  bool command_pending_{false};
  uint32_t last_command_ms_{0};
  uint8_t last_cmd_echo_[5]{};

  bool panel_detected_{false};
  bool selected_{false};
  bool handled_{false};

  bool heater_{false};
  bool light_on_{false};
  int8_t light_published_{-1};
  char unit_{'C'};
  float temperature_{NAN};
  float target_temperature_{NAN};
  std::string status_;
};

}  // namespace balboa_gl
}  // namespace esphome
