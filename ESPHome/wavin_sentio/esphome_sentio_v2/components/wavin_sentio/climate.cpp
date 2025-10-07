#include "climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wavin_sentio {

static const char *const TAG = "wavin_sentio.climate";

// Climate constants
static const float MIN_TEMPERATURE = 10.0f;
static const float MAX_TEMPERATURE = 30.0f;
static const float TEMPERATURE_STEP = 0.5f;

void WavinSentioClimate::setup() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent component not set!");
    this->mark_failed();
    return;
  }
  
  // Validate configuration
  if (!this->is_group_ && this->channel_ == 0) {
    ESP_LOGE(TAG, "Channel not set for single climate!");
    this->mark_failed();
    return;
  }
  
  if (this->is_group_ && this->members_.empty()) {
    ESP_LOGE(TAG, "Members not set for group climate!");
    this->mark_failed();
    return;
  }
  
  // Set initial mode
  this->mode = climate::CLIMATE_MODE_HEAT;
  
  ESP_LOGCONFIG(TAG, "Setting up Wavin Sentio Climate");
  if (this->is_group_) {
    ESP_LOGCONFIG(TAG, "  Type: Group (%u members)", this->members_.size());
  } else {
    ESP_LOGCONFIG(TAG, "  Type: Single Channel %u", this->channel_);
    ESP_LOGCONFIG(TAG, "  Floor Temperature Mode: %s", 
                  this->use_floor_temperature_ ? "YES" : "NO");
  }
}

void WavinSentioClimate::loop() {
  // Update state from parent component data
  this->update_state();
}

void WavinSentioClimate::dump_config() {
  LOG_CLIMATE("", "Wavin Sentio Climate", this);
  if (this->is_group_) {
    ESP_LOGCONFIG(TAG, "  Group Members: %u channels", this->members_.size());
    for (uint8_t member : this->members_) {
      ESP_LOGCONFIG(TAG, "    - Channel %u", member);
    }
  } else {
    ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
    if (this->use_floor_temperature_) {
      ESP_LOGCONFIG(TAG, "  Using Floor Temperature");
    }
  }
}

climate::ClimateTraits WavinSentioClimate::traits() {
  auto traits = climate::ClimateTraits();
  
  // Sentio supports heating mode
  traits.set_supported_modes({
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_HEAT,
  });
  
  // Temperature ranges
  traits.set_visual_min_temperature(MIN_TEMPERATURE);
  traits.set_visual_max_temperature(MAX_TEMPERATURE);
  traits.set_visual_temperature_step(TEMPERATURE_STEP);
  
  // Supported features
  traits.set_supports_current_temperature(true);
  traits.set_supports_two_point_target_temperature(false);
  traits.set_supports_action(true);
  
  return traits;
}

void WavinSentioClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    this->mode = *call.get_mode();
    
    // OFF mode - could write a special value or just not set target
    if (this->mode == climate::CLIMATE_MODE_OFF) {
      ESP_LOGD(TAG, "Climate set to OFF mode");
      // TODO: Implement OFF mode if supported by Sentio
    }
  }
  
  if (call.get_target_temperature().has_value()) {
    float target = *call.get_target_temperature();
    
    // Clamp to valid range
    target = clamp(target, MIN_TEMPERATURE, MAX_TEMPERATURE);
    
    // Round to nearest 0.5°C
    target = roundf(target * 2.0f) / 2.0f;
    
    this->target_temperature = target;
    
    // Write to device
    if (this->is_group_) {
      // Write same setpoint to all members
      bool all_success = true;
      for (uint8_t member : this->members_) {
        uint16_t raw_value = static_cast<uint16_t>(target * 100.0f);
        if (!this->parent_->write_register(member, 19, raw_value)) {
          ESP_LOGW(TAG, "Failed to write setpoint to member channel %u", member);
          all_success = false;
        } else {
          ESP_LOGI(TAG, "Set channel %u setpoint to %.1f°C", member, target);
        }
      }
      
      if (all_success) {
        ESP_LOGI(TAG, "Set group setpoint to %.1f°C", target);
      }
    } else {
      // Write to single channel
      uint16_t raw_value = static_cast<uint16_t>(target * 100.0f);
      if (this->parent_->write_register(this->channel_, 19, raw_value)) {
        ESP_LOGI(TAG, "Set channel %u setpoint to %.1f°C", this->channel_, target);
      } else {
        ESP_LOGW(TAG, "Failed to write setpoint to channel %u", this->channel_);
      }
    }
  }
  
  this->publish_state();
}

void WavinSentioClimate::update_state() {
  if (this->is_group_) {
    // Group climate - aggregate member values
    this->current_temperature = this->calculate_average_temperature();
    this->target_temperature = this->calculate_average_target_temperature();
    
    // Action: heating if any member is heating
    if (this->is_any_member_heating()) {
      this->action = climate::CLIMATE_ACTION_HEATING;
    } else {
      this->action = climate::CLIMATE_ACTION_IDLE;
    }
  } else {
    // Single channel climate
    ChannelData *data = this->parent_->get_channel_data(this->channel_);
    
    if (data == nullptr || !data->discovered) {
      // Channel not discovered yet
      return;
    }
    
    // Set current temperature
    if (this->use_floor_temperature_) {
      // Use floor temperature if available and floor mode enabled
      if (data->has_floor_sensor && !std::isnan(data->floor_temperature)) {
        this->current_temperature = data->floor_temperature;
      } else {
        // Floor sensor not available
        this->current_temperature = NAN;
      }
    } else {
      // Use air temperature (standard mode)
      this->current_temperature = data->current_temperature;
    }
    
    // Set target temperature
    this->target_temperature = data->target_temperature;
    
    // Determine action based on mode register
    // Mode register values (from Sentio documentation):
    // 1 = IDLE, 2 = HEATING, 3 = COOLING, 4 = BLOCKED_HEATING, 5 = BLOCKED_COOLING
    switch (data->mode) {
      case 2:  // HEATING
        this->action = climate::CLIMATE_ACTION_HEATING;
        break;
      case 3:  // COOLING (if supported)
        this->action = climate::CLIMATE_ACTION_COOLING;
        break;
      case 1:   // IDLE
      case 4:   // BLOCKED_HEATING
      case 5:   // BLOCKED_COOLING
      default:
        this->action = climate::CLIMATE_ACTION_IDLE;
        break;
    }
  }
  
  // Publish updated state to Home Assistant
  this->publish_state();
}

float WavinSentioClimate::calculate_average_temperature() {
  float sum = 0.0f;
  uint8_t count = 0;
  
  for (uint8_t member : this->members_) {
    ChannelData *data = this->parent_->get_channel_data(member);
    if (data != nullptr && data->discovered && !std::isnan(data->current_temperature)) {
      sum += data->current_temperature;
      count++;
    }
  }
  
  if (count == 0) {
    return NAN;
  }
  
  return sum / count;
}

float WavinSentioClimate::calculate_average_target_temperature() {
  float sum = 0.0f;
  uint8_t count = 0;
  
  for (uint8_t member : this->members_) {
    ChannelData *data = this->parent_->get_channel_data(member);
    if (data != nullptr && data->discovered && !std::isnan(data->target_temperature)) {
      sum += data->target_temperature;
      count++;
    }
  }
  
  if (count == 0) {
    return NAN;
  }
  
  return sum / count;
}

bool WavinSentioClimate::is_any_member_heating() {
  for (uint8_t member : this->members_) {
    ChannelData *data = this->parent_->get_channel_data(member);
    if (data != nullptr && data->discovered) {
      // Check if this member is in heating mode (mode = 2)
      if (data->mode == 2) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace wavin_sentio
}  // namespace esphome
