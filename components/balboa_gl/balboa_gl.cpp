#include "balboa_gl.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cmath>
#include <cstdio>

namespace esphome {
namespace balboa_gl {

static const char *const TAG = "balboa_gl";

static const size_t MAX_QUEUE = 30;
static const uint32_t COMMAND_INTERVAL_MS = 400;
static const uint32_t TX_ENABLE_DELAY_US = 20;
static const uint32_t TX_DONE_DELAY_US = 1000;
static const size_t FA_LENGTH = 23;
static const size_t AE_LENGTH = 16;
static const size_t FB_LENGTH = 9;

// Button presses as seen on the bus (header, length, payload, CRC)
static const Command CMD_UP = {0xfb, 0x06, 0x03, 0x45, 0x0e, 0x00, 0x01, 0xfe, 0x66};
static const Command CMD_DOWN = {0xfb, 0x06, 0x03, 0x45, 0x0e, 0x00, 0x02, 0xfd, 0x50};
static const Command CMD_MODE = {0xfb, 0x06, 0x03, 0x45, 0x0e, 0x00, 0x04, 0xfb, 0x3c};
static const Command CMD_AUX = {0xfb, 0x06, 0x03, 0x45, 0x0e, 0x00, 0x0a, 0xf5, 0xc0};
static const Command CMD_LIGHT = {0xfb, 0x06, 0x03, 0x45, 0x0e, 0x00, 0x09, 0xf6, 0xf6};
// Taken from another user's panel, see README
static const Command CMD_JET1 = {0xfb, 0x06, 0x03, 0x43, 0x06, 0x00, 0x06, 0xf9, 0xa2};
static const Command CMD_JET2 = {0xfb, 0x06, 0x03, 0x43, 0x06, 0x00, 0x07, 0xf8, 0xb0};

// ---------------------------------------------------------------- entities

void BalboaGLClimate::control(const climate::ClimateCall &call) {
  if (call.get_target_temperature().has_value())
    this->parent_->set_target_temperature(*call.get_target_temperature());
}

climate::ClimateTraits BalboaGLClimate::traits() {
  climate::ClimateTraits traits;
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE | climate::CLIMATE_SUPPORTS_ACTION);
  traits.set_supported_modes({climate::CLIMATE_MODE_HEAT});
  traits.set_visual_min_temperature(26);
  traits.set_visual_max_temperature(40);
  traits.set_visual_temperature_step(0.5f);
  return traits;
}

void BalboaGLSelect::control(const std::string &value) {
  auto target = this->index_of(value);
  if (!target.has_value())
    return;
  // State is published once the controller reports the change
  this->parent_->change_select(this->kind_, this->active_index().value_or(0), *target, this->size());
}

void BalboaGLSwitch::write_state(bool state) {
  if (state != this->state)
    this->parent_->toggle_light();
}

// --------------------------------------------------------------------- hub

void BalboaGL::setup() {
  this->panel_select_pin_->setup();
  this->flow_control_pin_->setup();
  this->flow_control_pin_->digital_write(false);
  // The panel-select window is only a few ms long, so we must be polled as fast as possible
  this->high_freq_.start();
}

void BalboaGL::dump_config() {
  ESP_LOGCONFIG(TAG, "Balboa GL/ML spa:");
  LOG_PIN("  Panel select pin: ", this->panel_select_pin_);
  LOG_PIN("  Flow control pin: ", this->flow_control_pin_);
}

void BalboaGL::drain_rx_() {
  uint8_t discard;
  while (this->available())
    this->read_byte(&discard);
}

void BalboaGL::loop() {
  // Pin 5 is LOW while the controller is talking to our panel
  if (this->panel_select_pin_->digital_read()) {
    this->panel_detected_ = true;
    this->selected_ = false;
    this->drain_rx_();  // traffic meant for other panels
    return;
  }

  if (!this->selected_) {  // falling edge: drop anything that arrived before our turn
    this->selected_ = true;
    this->handled_ = false;
    this->drain_rx_();
    if (!this->panel_detected_)
      this->set_status_("Panel select (pin5) not detected");
    return;
  }

  if (this->handled_) {  // one message per turn, the rest is padding/echo
    this->drain_rx_();
    return;
  }

  uint8_t first;
  // peek_byte() blocks for up to 100 ms on an empty buffer, so check first
  if (!this->available() || !this->peek_byte(&first))
    return;
  size_t len = first == 0xFA ? FA_LENGTH : first == 0xAE ? AE_LENGTH : first == 0xFB ? FB_LENGTH : 0;
  if (len == 0) {
    ESP_LOGV(TAG, "Unknown message start byte 0x%02X", first);
    this->read_byte(&first);
    return;
  }
  if (this->available() < (int) len)
    return;

  uint8_t buf[FA_LENGTH];
  this->read_array(buf, len);
  this->handled_ = true;
  this->handle_message_(buf, len);
}

void BalboaGL::handle_message_(const uint8_t *buf, size_t len) {
  ESP_LOGVV(TAG, "RX %s", format_hex_pretty(buf, len).c_str());
  if (buf[0] != 0xFA)
    return;
  this->send_command_();  // we must answer within our turn, before parsing
  if (buf[1] == 0x14)
    this->handle_status_(buf);
}

/*
 * FA 14 status message (23 bytes):
 *   2-4   temperature as ASCII "340" = 34.0
 *   5     unit 'C' / 'F' / '-' (unknown)
 *   6     pumps: bits 0-1 pump1, bits 2-3 pump2 (0 off, 1 low, 2 high)
 *   7     high nibble heater (1 on, 2 verifying), low nibble light (3 on)
 *   8     low nibble operating mode
 *   9     menu: 0x00 idle, 0x46 set temp (bytes 2-4 show the target), 0x4C set mode, 0x5A standby
 *   17-21 echo of the last command accepted by the controller
 */
void BalboaGL::handle_status_(const uint8_t *buf) {
  const uint8_t unit = buf[5];
  if (unit != 'C' && unit != 'F' && unit != '-')
    return;  // not a status message

  const uint8_t menu = buf[9];

  // controller accepted our command
  if (memcmp(this->last_cmd_echo_, buf + 17, sizeof(this->last_cmd_echo_)) != 0) {
    memcpy(this->last_cmd_echo_, buf + 17, sizeof(this->last_cmd_echo_));
    if (this->command_pending_ && !this->queue_.empty())
      this->queue_.pop_front();
    this->command_pending_ = false;
  }

  const uint8_t pumps = buf[6] & 0x0F;
  const uint8_t heater = buf[7] >> 4;
  const uint8_t light = buf[7] & 0x0F;
  const uint8_t mode = buf[8] & 0x0F;

  this->heater_ = heater == 1 || heater == 2;  // 2 = verifying, treat as on to avoid noise
  if (light == 0)
    this->light_on_ = false;
  else if (light == 3)
    this->light_on_ = true;

  // status text + operating mode index (-1 = leave unchanged)
  int mode_idx = -1;
  std::string status;
  switch (mode) {
    case 0x4: status = "Sleep"; mode_idx = 2; break;
    case 0x9: status = "Circulation"; mode_idx = 0; break;
    case 0x1: status = "Standard"; mode_idx = 0; break;
    case 0x2: status = "Economy"; mode_idx = 1; break;
    case 0xA: status = "Cleaning"; break;
    case 0xC: status = "Circulation in sleep"; mode_idx = 2; break;
    case 0xB:
    case 0x3: status = "Std in Eco"; mode_idx = 0; break;  // one hour of standard while in economy
    default: status = str_sprintf("Unknown %X", mode);
  }
  if (menu == 0x4C) {
    status = "Set Mode";
  } else if (menu == 0x46) {
    status = "Set Temperature";
  } else if (menu == 0x5A) {
    status = "Standby?";
  } else if (menu != 0x00) {
    status = str_sprintf("menu %02X", menu);
  }
  this->set_status_(status);

  if (unit != '-' && isdigit(buf[2]) && isdigit(buf[3]) && isdigit(buf[4])) {
    const float temp = ((buf[2] - '0') * 100 + (buf[3] - '0') * 10 + (buf[4] - '0')) / 10.0f;
    this->unit_ = unit;
    if (menu == 0x46)
      this->target_temperature_ = temp;
    else
      this->temperature_ = temp;
  }

  // Single speed outputs only report "on" as low or high depending on the controller
  auto speed = [this](SelectKind kind, uint8_t value) {
    auto *select = this->selects_[kind];
    return (select != nullptr && select->size() > 2) ? value : (value ? 1 : 0);
  };
  this->publish_select_(PUMP1, speed(PUMP1, pumps & 0x03));
  this->publish_select_(PUMP2, speed(PUMP2, (pumps >> 2) & 0x03));
  if (mode_idx >= 0)
    this->publish_select_(MODE, mode_idx);

  if (this->heater_sensor_ != nullptr)
    this->heater_sensor_->publish_state(this->heater_);
  if (this->light_ != nullptr && this->light_published_ != (int8_t) this->light_on_) {
    this->light_published_ = this->light_on_;
    this->light_->publish_state(this->light_on_);
  }
  this->publish_climate_();
}

void BalboaGL::publish_climate_() {
  if (this->climate_ == nullptr)
    return;
  auto *c = this->climate_;
  const auto action = this->heater_ ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_IDLE;
  // don't overwrite the target while we are still stepping it
  const float target = this->queue_.empty() ? this->target_temperature_ : c->target_temperature;
  if (c->mode == climate::CLIMATE_MODE_HEAT && c->action == action && c->current_temperature == this->temperature_ &&
      (c->target_temperature == target || std::isnan(target)))
    return;
  c->mode = climate::CLIMATE_MODE_HEAT;
  c->action = action;
  c->current_temperature = this->temperature_;
  c->target_temperature = target;
  c->publish_state();
}

void BalboaGL::publish_select_(SelectKind kind, size_t index) {
  auto *select = this->selects_[kind];
  if (select == nullptr || (select->has_state() && select->active_index() == index))
    return;
  if (auto option = select->at(index))
    select->publish_state(*option);
}

void BalboaGL::set_status_(const std::string &status) {
  if (status == this->status_)
    return;
  this->status_ = status;
  if (this->status_sensor_ != nullptr)
    this->status_sensor_->publish_state(status);
}

// ---------------------------------------------------------------- commands

void BalboaGL::enqueue_(const Command &cmd, size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (this->queue_.size() >= MAX_QUEUE) {
      ESP_LOGW(TAG, "Command queue full, dropping command");
      return;
    }
    this->queue_.push_back(cmd);
  }
}

void BalboaGL::toggle_light() { this->enqueue_(CMD_LIGHT); }

void BalboaGL::change_select(SelectKind kind, size_t current, size_t target, size_t count) {
  if (current == target)
    return;
  const size_t presses = (target + count - current) % count;
  switch (kind) {
    case PUMP1: this->enqueue_(CMD_JET1, presses); break;
    case PUMP2: this->enqueue_(CMD_JET2, presses); break;
    case AUX: this->enqueue_(CMD_AUX, presses); break;
    default:  // mode: enter mode menu, step, leave
      this->enqueue_(CMD_MODE);
      this->enqueue_(CMD_DOWN, presses);
      this->enqueue_(CMD_MODE);
  }
}

void BalboaGL::set_target_temperature(float target) {
  // First press only enters "set temp" mode, which makes the controller report the current target
  this->enqueue_(CMD_UP);
  if (std::isnan(this->target_temperature_)) {
    ESP_LOGW(TAG, "Target temperature not known yet, try again");
    return;
  }
  const float step = this->unit_ == 'F' ? 1.0f : 0.5f;
  const int presses = (int) lroundf((target - this->target_temperature_) / step);
  this->enqueue_(presses > 0 ? CMD_UP : CMD_DOWN, abs(presses));
}

// Resent every COMMAND_INTERVAL_MS until the controller echoes it back (see handle_status_)
void BalboaGL::send_command_() {
  if (this->queue_.empty() || millis() - this->last_command_ms_ < COMMAND_INTERVAL_MS)
    return;
  this->last_command_ms_ = millis();
  this->command_pending_ = true;

  const Command &cmd = this->queue_.front();
  this->flow_control_pin_->digital_write(true);
  delayMicroseconds(TX_ENABLE_DELAY_US);
  this->write_array(cmd.data(), cmd.size());
  this->flush();
  delayMicroseconds(TX_DONE_DELAY_US);
  this->flow_control_pin_->digital_write(false);
  if (this->panel_select_pin_->digital_read())
    ESP_LOGW(TAG, "Panel select went high before command was sent");
}

}  // namespace balboa_gl
}  // namespace esphome
