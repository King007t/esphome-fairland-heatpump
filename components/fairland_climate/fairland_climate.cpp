#include "fairland_climate.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fairland_climate {

static const char *const TAG = "fairland_climate";

// After a mode change the requested mode is shown until the device reports it back,
// at most power_on_delay + this grace period (covers the next polling cycle).
static const uint32_t PENDING_MODE_GRACE_MS = 15000;

void FairlandClimate::setup() {
  // The options of the working mode select become the custom presets of the climate entity.
  const auto &options = this->working_mode_select_->traits.get_options();
  std::vector<const char *> presets;
  presets.reserve(options.size());
  for (size_t i = 0; i < options.size(); i++) {
    presets.push_back(options[i]);
  }
  this->set_supported_custom_presets(presets);

  for (const char *option : {this->auto_option_, this->heat_option_, this->cool_option_}) {
    if (!this->mode_select_->has_option(option)) {
      ESP_LOGE(TAG, "Mode select '%s' has no option '%s' (check mode_options)",
               this->mode_select_->get_name().c_str(), option);
    }
  }

  this->power_switch_->add_on_state_callback([this](bool) { this->schedule_refresh_(); });
  this->mode_select_->add_on_state_callback([this](size_t) { this->schedule_refresh_(); });
  this->working_mode_select_->add_on_state_callback([this](size_t) { this->schedule_refresh_(); });
  for (number::Number *num : {this->heating_target_, this->cooling_target_, this->auto_target_}) {
    if (num != nullptr) {
      num->add_on_state_callback([this](float) { this->schedule_refresh_(); });
    }
  }
  this->current_sensor_->add_on_state_callback([this](float) { this->schedule_refresh_(); });
  if (this->compressor_sensor_ != nullptr) {
    this->compressor_sensor_->add_on_state_callback([this](float) { this->schedule_refresh_(); });
  }

  this->refresh_();
}

void FairlandClimate::dump_config() {
  LOG_CLIMATE("", "Fairland Climate", this);
  ESP_LOGCONFIG(TAG,
                "  Power switch: %s\n"
                "  Mode select: %s (auto='%s', heat='%s', cool='%s')\n"
                "  Working mode select: %s\n"
                "  Heating target number: %s\n"
                "  Cooling target number: %s\n"
                "  Auto target number: %s\n"
                "  Current temperature sensor: %s\n"
                "  Compressor sensor: %s\n"
                "  Power-on delay: %u ms",
                this->power_switch_->get_name().c_str(), this->mode_select_->get_name().c_str(), this->auto_option_,
                this->heat_option_, this->cool_option_, this->working_mode_select_->get_name().c_str(),
                this->heating_target_->get_name().c_str(),
                this->cooling_target_ != nullptr ? this->cooling_target_->get_name().c_str() : "- (heating setpoint)",
                this->auto_target_ != nullptr ? this->auto_target_->get_name().c_str() : "- (heating setpoint)",
                this->current_sensor_->get_name().c_str(),
                this->compressor_sensor_ != nullptr ? this->compressor_sensor_->get_name().c_str() : "-",
                static_cast<unsigned>(this->power_on_delay_ms_));
}

climate::ClimateTraits FairlandClimate::traits() {
  climate::ClimateTraits traits;
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  if (this->compressor_sensor_ != nullptr) {
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_ACTION);
  }
  traits.add_supported_mode(climate::CLIMATE_MODE_OFF);
  traits.add_supported_mode(climate::CLIMATE_MODE_HEAT);
  traits.add_supported_mode(climate::CLIMATE_MODE_COOL);
  traits.add_supported_mode(climate::CLIMATE_MODE_AUTO);
  // The visual range spans all configured setpoint numbers; the step comes from the heating one.
  float min_temperature = this->heating_target_->traits.get_min_value();
  float max_temperature = this->heating_target_->traits.get_max_value();
  for (number::Number *num : {this->cooling_target_, this->auto_target_}) {
    if (num != nullptr) {
      min_temperature = std::min(min_temperature, num->traits.get_min_value());
      max_temperature = std::max(max_temperature, num->traits.get_max_value());
    }
  }
  traits.set_visual_min_temperature(min_temperature);
  traits.set_visual_max_temperature(max_temperature);
  traits.set_visual_target_temperature_step(this->heating_target_->traits.get_step());
  traits.set_visual_current_temperature_step(0.5f);
  return traits;
}

void FairlandClimate::control(const climate::ClimateCall &call) {
  // Mode the setpoint applies to: the requested one, else the one the unit is in.
  climate::ClimateMode setpoint_mode = this->pending_mode_.value_or(this->device_mode_());

  if (call.get_mode().has_value()) {
    const climate::ClimateMode new_mode = *call.get_mode();
    this->cancel_timeout("mode_after_power_on");
    if (new_mode == climate::CLIMATE_MODE_OFF) {
      ESP_LOGD(TAG, "Power off (coil 0 = 0)");
      this->power_switch_->turn_off();
    } else {
      const char *option = this->option_for_mode_(new_mode);
      if (option == nullptr) {
        ESP_LOGW(TAG, "Mode %d is not supported", static_cast<int>(new_mode));
      } else if (!this->power_switch_->state) {
        ESP_LOGD(TAG, "Power on (coil 0 = 1), mode '%s' after %u ms", option,
                 static_cast<unsigned>(this->power_on_delay_ms_));
        this->power_switch_->turn_on();
        this->set_timeout("mode_after_power_on", this->power_on_delay_ms_,
                          [this, option]() { this->write_mode_option_(option); });
      } else {
        this->write_mode_option_(option);
      }
      setpoint_mode = new_mode;
    }
    this->pending_mode_ = new_mode;
    this->pending_mode_until_ = millis() + this->power_on_delay_ms_ + PENDING_MODE_GRACE_MS;
    this->mode = new_mode;
  }

  if (call.get_target_temperature().has_value()) {
    number::Number *num = this->target_number_for_mode_(setpoint_mode);
    // The climate entity shows one visual range for all modes; clamp to the range of the
    // setpoint that is actually written so the displayed value matches what the unit accepts.
    const float requested = *call.get_target_temperature();
    const float target = std::clamp(requested, num->traits.get_min_value(), num->traits.get_max_value());
    if (target != requested) {
      ESP_LOGW(TAG, "Target temperature %.1f °C is outside the range of '%s' (%.1f-%.1f °C), using %.1f °C",
               requested, num->get_name().c_str(), num->traits.get_min_value(), num->traits.get_max_value(),
               target);
    }
    ESP_LOGD(TAG, "Target temperature %.1f °C -> '%s'", target, num->get_name().c_str());
    num->make_call().set_value(target).perform();
    this->target_temperature = target;
  }

  const StringRef preset = call.get_custom_preset();
  if (!preset.empty()) {
    ESP_LOGD(TAG, "Working mode '%s'", preset.c_str());
    this->working_mode_select_->make_call().set_option(preset.c_str()).perform();
    this->set_custom_preset_(preset);
  }

  this->publish_state();
}

void FairlandClimate::write_mode_option_(const char *option) {
  ESP_LOGD(TAG, "Writing mode register: '%s'", option);
  this->mode_select_->make_call().set_option(option).perform();
}

void FairlandClimate::schedule_refresh_() {
  this->set_timeout("refresh", 100, [this]() { this->refresh_(); });
}

void FairlandClimate::refresh_() {
  const bool powered = this->power_switch_->state;
  const climate::ClimateMode device_mode = this->device_mode_();

  climate::ClimateMode mode = powered ? device_mode : climate::CLIMATE_MODE_OFF;
  if (this->pending_mode_.has_value()) {
    const bool confirmed = (mode == *this->pending_mode_);
    const bool expired = static_cast<int32_t>(millis() - this->pending_mode_until_) >= 0;
    if (confirmed || expired) {
      this->pending_mode_.reset();
    } else {
      mode = *this->pending_mode_;
    }
  }
  this->mode = mode;

  // Setpoint of the mode that is (or is about to be) active on the unit.
  const climate::ClimateMode setpoint_mode =
      (this->pending_mode_.has_value() && *this->pending_mode_ != climate::CLIMATE_MODE_OFF) ? *this->pending_mode_
                                                                                              : device_mode;
  number::Number *target = this->target_number_for_mode_(setpoint_mode);
  if (target->has_state() && !std::isnan(target->state)) {
    this->target_temperature = target->state;
  }
  if (this->current_sensor_->has_state() && !std::isnan(this->current_sensor_->state)) {
    this->current_temperature = this->current_sensor_->state;
  }

  auto working_index = this->working_mode_select_->active_index();
  if (working_index.has_value()) {
    this->set_custom_preset_(this->working_mode_select_->option_at(*working_index));
  }

  this->action = this->compute_action_(mode);
  this->publish_state();
}

climate::ClimateMode FairlandClimate::device_mode_() const {
  auto index = this->mode_select_->active_index();
  if (index.has_value()) {
    return this->mode_for_option_(this->mode_select_->option_at(*index));
  }
  return climate::CLIMATE_MODE_HEAT;  // nothing read yet
}

number::Number *FairlandClimate::target_number_for_mode_(climate::ClimateMode mode) const {
  switch (mode) {
    case climate::CLIMATE_MODE_COOL:
      return this->cooling_target_ != nullptr ? this->cooling_target_ : this->heating_target_;
    case climate::CLIMATE_MODE_AUTO:
      return this->auto_target_ != nullptr ? this->auto_target_ : this->heating_target_;
    default:
      return this->heating_target_;
  }
}

climate::ClimateAction FairlandClimate::compute_action_(climate::ClimateMode mode) const {
  if (mode == climate::CLIMATE_MODE_OFF) {
    return climate::CLIMATE_ACTION_OFF;
  }
  if (this->compressor_sensor_ == nullptr || !this->compressor_sensor_->has_state() ||
      std::isnan(this->compressor_sensor_->state) || this->compressor_sensor_->state <= 0.0f) {
    return climate::CLIMATE_ACTION_IDLE;
  }
  switch (mode) {
    case climate::CLIMATE_MODE_HEAT:
      return climate::CLIMATE_ACTION_HEATING;
    case climate::CLIMATE_MODE_COOL:
      return climate::CLIMATE_ACTION_COOLING;
    default:
      // Auto: the unit only cools when the target is below the current temperature.
      if (!std::isnan(this->current_temperature) && !std::isnan(this->target_temperature) &&
          this->target_temperature < this->current_temperature) {
        return climate::CLIMATE_ACTION_COOLING;
      }
      return climate::CLIMATE_ACTION_HEATING;
  }
}

const char *FairlandClimate::option_for_mode_(climate::ClimateMode mode) const {
  const char *option = nullptr;
  switch (mode) {
    case climate::CLIMATE_MODE_AUTO:
      option = this->auto_option_;
      break;
    case climate::CLIMATE_MODE_HEAT:
      option = this->heat_option_;
      break;
    case climate::CLIMATE_MODE_COOL:
      option = this->cool_option_;
      break;
    default:
      return nullptr;
  }
  return this->mode_select_->has_option(option) ? option : nullptr;
}

climate::ClimateMode FairlandClimate::mode_for_option_(const char *option) const {
  if (option == nullptr)
    return climate::CLIMATE_MODE_AUTO;
  if (strcmp(option, this->heat_option_) == 0)
    return climate::CLIMATE_MODE_HEAT;
  if (strcmp(option, this->cool_option_) == 0)
    return climate::CLIMATE_MODE_COOL;
  return climate::CLIMATE_MODE_AUTO;
}

}  // namespace fairland_climate
}  // namespace esphome
