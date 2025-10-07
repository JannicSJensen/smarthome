#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "wavin_sentio.h"
#include <vector>

namespace esphome {
namespace wavin_sentio {

class WavinSentioClimate : public climate::Climate, public Component {
 public:
  WavinSentioClimate() = default;
  
  void setup() override;
  void loop() override;
  void dump_config() override;
  
  float get_setup_priority() const override { return setup_priority::DATA; }
  
  // Configuration
  void set_parent(WavinSentio *parent) { this->parent_ = parent; }
  void set_channel(uint8_t channel) { 
    this->channel_ = channel; 
    this->is_group_ = false;
  }
  void set_members(const std::vector<uint8_t> &members) {
    this->members_ = members;
    this->is_group_ = true;
  }
  void set_use_floor_temperature(bool use_floor) { this->use_floor_temperature_ = use_floor; }
  
  climate::ClimateTraits traits() override;
  
 protected:
  void control(const climate::ClimateCall &call) override;
  void update_state();
  
  WavinSentio *parent_{nullptr};
  uint8_t channel_{0};
  std::vector<uint8_t> members_;
  bool is_group_{false};
  bool use_floor_temperature_{false};
  
  // For group climates - aggregate values
  float calculate_average_temperature();
  float calculate_average_target_temperature();
  bool is_any_member_heating();
};

}  // namespace wavin_sentio
}  // namespace esphome
