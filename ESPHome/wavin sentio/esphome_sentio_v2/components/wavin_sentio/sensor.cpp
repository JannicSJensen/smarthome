#include "sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wavin_sentio {

static const char *const TAG = "wavin_sentio.sensor";

void WavinSentioSensor::setup() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent component not set!");
    this->mark_failed();
    return;
  }
  
  if (this->channel_ == 0) {
    ESP_LOGE(TAG, "Channel not set!");
    this->mark_failed();
    return;
  }
  
  ESP_LOGCONFIG(TAG, "Setting up Wavin Sentio Sensor");
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  ESP_LOGCONFIG(TAG, "  Type: %u", static_cast<uint8_t>(this->sensor_type_));
}

void WavinSentioSensor::loop() {
  // Read value from parent component and publish
  this->update();
}

void WavinSentioSensor::dump_config() {
  LOG_SENSOR("", "Wavin Sentio Sensor", this);
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  
  const char *type_str = "Unknown";
  switch (this->sensor_type_) {
    case SensorType::BATTERY:
      type_str = "Battery";
      break;
    case SensorType::TEMPERATURE:
      type_str = "Temperature";
      break;
    case SensorType::FLOOR_TEMPERATURE:
      type_str = "Floor Temperature";
      break;
    case SensorType::COMFORT_SETPOINT:
      type_str = "Comfort Setpoint";
      break;
    case SensorType::HUMIDITY:
      type_str = "Humidity";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Sensor Type: %s", type_str);
}

void WavinSentioSensor::update() {
  // Get channel data from parent
  ChannelData *data = this->parent_->get_channel_data(this->channel_);
  
  if (data == nullptr) {
    ESP_LOGW(TAG, "Channel %u data not available", this->channel_);
    return;
  }
  
  if (!data->discovered) {
    // Channel not yet discovered, don't publish
    return;
  }
  
  // Read appropriate value based on sensor type
  float value = NAN;
  
  switch (this->sensor_type_) {
    case SensorType::BATTERY:
      // Battery level (0-100%)
      value = data->battery_level;
      break;
      
    case SensorType::TEMPERATURE:
      // Air temperature
      value = data->current_temperature;
      break;
      
    case SensorType::FLOOR_TEMPERATURE:
      // Floor temperature (only if sensor present)
      if (data->has_floor_sensor) {
        value = data->floor_temperature;
      } else {
        // Don't publish if floor sensor not present
        ESP_LOGV(TAG, "Channel %u has no floor sensor", this->channel_);
        return;
      }
      break;
      
    case SensorType::COMFORT_SETPOINT:
      // Target/setpoint temperature
      value = data->target_temperature;
      break;
      
    case SensorType::HUMIDITY:
      // Humidity percentage
      value = data->humidity;
      break;
      
    default:
      ESP_LOGW(TAG, "Unknown sensor type: %u", static_cast<uint8_t>(this->sensor_type_));
      return;
  }
  
  // Only publish if value is valid (not NAN)
  if (!std::isnan(value)) {
    this->publish_state(value);
    ESP_LOGV(TAG, "Channel %u sensor type %u: %.2f", 
             this->channel_, static_cast<uint8_t>(this->sensor_type_), value);
  } else {
    ESP_LOGV(TAG, "Channel %u sensor type %u: value is NAN", 
             this->channel_, static_cast<uint8_t>(this->sensor_type_));
  }
}

}  // namespace wavin_sentio
}  // namespace esphome
