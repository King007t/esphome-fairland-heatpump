#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"

#include "fairland_errors.h"

namespace esphome {
namespace fairland_climate {

/**
 * Climate entity for Fairland pool heat pumps.
 *
 * The component has no Modbus access of its own. It is composed of the
 * modbus_controller entities:
 *   - power_switch                (coil 0)             -> OFF / on
 *   - mode_select                 (holding 0)          -> Auto / Heating / Cooling
 *   - working_mode_select         (holding 1)          -> presets (Smart, Silence, ...)
 *   - heating_target_number       (holding 3)          -> target temperature in Heating mode
 *   - cooling_target_number       (holding 4, optional)-> target temperature in Cooling mode
 *   - auto_target_number          (holding 2, optional)-> target temperature in Auto mode
 *   - current_temperature_sensor  (input 3)            -> current temperature (water inlet)
 *   - compressor_sensor           (input 0, optional)  -> action heating / cooling / idle
 *
 * OFF clears coil 0. HEAT/COOL/AUTO set coil 0 and then write the mode register
 * (after power_on_delay when the unit was off). The target temperature is read
 * from and written to the setpoint number of the active mode; modes without a
 * configured number fall back to the heating setpoint.
 */
class FairlandClimate : public climate::Climate, public Component {
 public:
  void set_power_switch(switch_::Switch *sw) { this->power_switch_ = sw; }
  void set_mode_select(select::Select *sel) { this->mode_select_ = sel; }
  void set_working_mode_select(select::Select *sel) { this->working_mode_select_ = sel; }
  void set_heating_target_number(number::Number *num) { this->heating_target_ = num; }
  void set_cooling_target_number(number::Number *num) { this->cooling_target_ = num; }
  void set_auto_target_number(number::Number *num) { this->auto_target_ = num; }
  void set_current_temperature_sensor(sensor::Sensor *sens) { this->current_sensor_ = sens; }
  void set_compressor_sensor(sensor::Sensor *sens) { this->compressor_sensor_ = sens; }
  void set_power_on_delay(uint32_t delay_ms) { this->power_on_delay_ms_ = delay_ms; }
  void set_mode_options(const char *auto_option, const char *heat_option, const char *cool_option) {
    this->auto_option_ = auto_option;
    this->heat_option_ = heat_option;
    this->cool_option_ = cool_option;
  }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  /// Recompute the state from the linked entities and publish it.
  void refresh_();
  /// Coalesce several entity callbacks per polling cycle into one refresh.
  void schedule_refresh_();
  /// Mode currently selected on the unit (independent of the power state).
  climate::ClimateMode device_mode_() const;
  /// Setpoint number used in the given mode (heating setpoint as fallback).
  number::Number *target_number_for_mode_(climate::ClimateMode mode) const;
  climate::ClimateAction compute_action_(climate::ClimateMode mode) const;
  const char *option_for_mode_(climate::ClimateMode mode) const;
  climate::ClimateMode mode_for_option_(const char *option) const;
  void write_mode_option_(const char *option);

  switch_::Switch *power_switch_{nullptr};
  select::Select *mode_select_{nullptr};
  select::Select *working_mode_select_{nullptr};
  number::Number *heating_target_{nullptr};
  number::Number *cooling_target_{nullptr};
  number::Number *auto_target_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *compressor_sensor_{nullptr};
  uint32_t power_on_delay_ms_{5000};
  const char *auto_option_{"Auto"};
  const char *heat_option_{"Heating"};
  const char *cool_option_{"Cooling"};

  /// Requested mode, held until the device confirms it (or the grace period expires).
  optional<climate::ClimateMode> pending_mode_{};
  uint32_t pending_mode_until_{0};
};

}  // namespace fairland_climate
}  // namespace esphome
