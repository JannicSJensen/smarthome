#include "wavin_sentio.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace wavin_sentio {

static const char *const TAG = "wavin_sentio";

// Register offsets within each channel's 100-block
static const uint8_t REG_DESIRED_TEMP = 1;      // X01 - Desired temperature
static const uint8_t REG_MODE = 2;              // X02 - HVAC Mode/State  
static const uint8_t REG_BLOCKING_SOURCE = 3;  // X03 - Blocking source
static const uint8_t REG_AIR_TEMP = 4;          // X04 - Air temperature (×100)
static const uint8_t REG_FLOOR_TEMP = 5;        // X05 - Floor temperature (×100)
static const uint8_t REG_HUMIDITY = 6;          // X06 - Relative humidity (×100)
static const uint8_t REG_SETPOINT = 19;         // X19 - Temperature setpoint (×100)

void WavinSentio::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Wavin Sentio...");
  
  // Initialize channel data structures
  for (uint8_t i = 1; i <= 16; i++) {
    this->channels_[i] = ChannelData(i);
  }
  
  // Start discovery process
  this->current_poll_channel_ = 1;
}

void WavinSentio::loop() {
  // Handle any ongoing modbus operations
}

void WavinSentio::update() {
  // Poll configured number of channels per update cycle
  for (uint8_t i = 0; i < this->poll_channels_per_cycle_; i++) {
    if (this->current_poll_channel_ > 16) {
      this->current_poll_channel_ = 1;
    }
    
    this->poll_channel(this->current_poll_channel_);
    this->current_poll_channel_++;
  }
}

void WavinSentio::dump_config() {
  ESP_LOGCONFIG(TAG, "Wavin Sentio:");
  ESP_LOGCONFIG(TAG, "  Update Interval: %u ms", this->get_update_interval());
  ESP_LOGCONFIG(TAG, "  Poll Channels Per Cycle: %u", this->poll_channels_per_cycle_);
  
  if (this->flow_control_pin_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Flow Control Pin: GPIO%u", this->flow_control_pin_.value());
  }
  
  if (this->tx_enable_pin_.has_value()) {
    ESP_LOGCONFIG(TAG, "  TX Enable Pin: GPIO%u", this->tx_enable_pin_.value());
  }
  
  // Log discovered channels
  uint8_t discovered_count = 0;
  for (const auto &pair : this->channels_) {
    if (pair.second.discovered) {
      discovered_count++;
      ESP_LOGCONFIG(TAG, "  Channel %u: %s%s", 
                    pair.first, 
                    pair.second.friendly_name.c_str(),
                    pair.second.has_floor_sensor ? " (floor sensor)" : "");
    }
  }
  ESP_LOGCONFIG(TAG, "  Discovered Channels: %u", discovered_count);
}

void WavinSentio::set_channel_friendly_name(uint8_t channel, const std::string &name) {
  if (channel >= 1 && channel <= 16) {
    this->friendly_names_[channel] = name;
    if (this->channels_.find(channel) != this->channels_.end()) {
      this->channels_[channel].friendly_name = name;
    }
  }
}

ChannelData* WavinSentio::get_channel_data(uint8_t channel) {
  if (this->channels_.find(channel) != this->channels_.end()) {
    return &this->channels_[channel];
  }
  return nullptr;
}

bool WavinSentio::is_channel_discovered(uint8_t channel) {
  auto *data = this->get_channel_data(channel);
  return data != nullptr && data->discovered;
}

uint16_t WavinSentio::get_register_address(uint8_t channel, uint8_t offset) {
  // Sentio uses channel * 100 + offset addressing
  // Channel 1: 101-119, Channel 2: 201-219, etc.
  return (channel * 100) + offset;
}

bool WavinSentio::read_register(uint8_t channel, uint8_t offset, uint16_t &value) {
  uint16_t address = this->get_register_address(channel, offset);
  
  // Try to read with retry logic
  for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
    // Use modbus read_register from ModbusDevice base class
    // For input registers (function code 0x04)
    if (offset < 10) {  // Registers 1-9 are input registers
      auto result = this->read_register(address, &value, 1, false);
      if (result == 0) {  // Success
        return true;
      }
      
      if (attempt < MAX_RETRIES - 1) {
        ESP_LOGD(TAG, "Read failed for channel %u register %u (0x%04X), attempt %u/%u", 
                 channel, offset, address, attempt + 1, MAX_RETRIES);
        delay(50);  // Small delay before retry
      }
    } else {  // Register 19+ are holding registers
      auto result = this->read_register(address, &value, 1, true);
      if (result == 0) {  // Success
        return true;
      }
      
      if (attempt < MAX_RETRIES - 1) {
        ESP_LOGD(TAG, "Read failed for channel %u register %u (0x%04X), attempt %u/%u", 
                 channel, offset, address, attempt + 1, MAX_RETRIES);
        delay(50);
      }
    }
  }
  
  ESP_LOGW(TAG, "Failed to read channel %u register %u (0x%04X) after %u attempts", 
           channel, offset, address, MAX_RETRIES);
  return false;
}

bool WavinSentio::write_register(uint8_t channel, uint8_t offset, uint16_t value) {
  uint16_t address = this->get_register_address(channel, offset);
  
  // Try to write with retry logic
  for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
    // Use modbus write_register from ModbusDevice base class
    // Holding registers use function code 0x06 (write single register)
    auto result = this->write_register(address, value, true);
    if (result == 0) {  // Success
      ESP_LOGD(TAG, "Successfully wrote %u to channel %u register %u (0x%04X)", 
               value, channel, offset, address);
      return true;
    }
    
    if (attempt < MAX_RETRIES - 1) {
      ESP_LOGD(TAG, "Write failed for channel %u register %u (0x%04X), attempt %u/%u", 
               channel, offset, address, attempt + 1, MAX_RETRIES);
      delay(50);
    }
  }
  
  ESP_LOGW(TAG, "Failed to write to channel %u register %u (0x%04X) after %u attempts", 
           channel, offset, address, MAX_RETRIES);
  return false;
}

void WavinSentio::poll_channel(uint8_t channel) {
  if (channel < 1 || channel > 16) {
    return;
  }
  
  ChannelData *data = &this->channels_[channel];
  uint16_t raw_value;
  bool read_success = false;
  
  // Try to read air temperature first - this indicates if channel exists
  if (this->read_register(channel, REG_AIR_TEMP, raw_value)) {
    read_success = true;
    
    // Temperature is stored as value * 100, so divide by 100
    float temperature = raw_value / 100.0f;
    
    // Sanity check - temperature should be reasonable (5-40°C typically)
    if (temperature > 5.0f && temperature < 50.0f) {
      data->current_temperature = temperature;
      data->discovered = true;
      
      // Set friendly name if configured
      if (this->friendly_names_.find(channel) != this->friendly_names_.end()) {
        data->friendly_name = this->friendly_names_[channel];
      } else {
        data->friendly_name = "Zone " + to_string(channel);
      }
      
      ESP_LOGV(TAG, "Channel %u air temp: %.1f°C", channel, temperature);
    }
  }
  
  // If channel exists, read other registers
  if (data->discovered) {
    // Read floor temperature
    if (this->read_register(channel, REG_FLOOR_TEMP, raw_value)) {
      float floor_temp = raw_value / 100.0f;
      
      // Floor sensor detection: valid readings are > 1°C and < 90°C
      if (floor_temp > 1.0f && floor_temp < 90.0f) {
        data->floor_temperature = floor_temp;
        data->has_floor_sensor = true;
        ESP_LOGV(TAG, "Channel %u floor temp: %.1f°C", channel, floor_temp);
      } else {
        data->floor_temperature = NAN;
        data->has_floor_sensor = false;
      }
    }
    
    // Read humidity
    if (this->read_register(channel, REG_HUMIDITY, raw_value)) {
      float humidity = raw_value / 100.0f;
      if (humidity >= 0.0f && humidity <= 100.0f) {
        data->humidity = humidity;
        ESP_LOGV(TAG, "Channel %u humidity: %.1f%%", channel, humidity);
      }
    }
    
    // Read target temperature (setpoint)
    if (this->read_register(channel, REG_SETPOINT, raw_value)) {
      float setpoint = raw_value / 100.0f;
      if (setpoint > 5.0f && setpoint < 35.0f) {
        data->target_temperature = setpoint;
        ESP_LOGV(TAG, "Channel %u setpoint: %.1f°C", channel, setpoint);
      }
    }
    
    // Read mode/state
    if (this->read_register(channel, REG_MODE, raw_value)) {
      data->mode = raw_value;
      ESP_LOGV(TAG, "Channel %u mode: 0x%04X", channel, raw_value);
    }
    
    // TODO: Read battery level if available
    // This may require reading from a different register or calculation
    // For now, set to a default value
    data->battery_level = 100.0f;  // Placeholder
  } else if (read_success) {
    // First time discovering this channel
    ESP_LOGI(TAG, "Discovered channel %u", channel);
  }
}

void WavinSentio::discover_channels() {
  // Discovery happens automatically during polling
  // Channels that respond to temperature reads are marked as discovered
}

void WavinSentio::register_climate(climate::Climate *climate_entity, uint8_t channel) {
  ESP_LOGD(TAG, "Registering climate entity for channel %u", channel);
  // Climate entities will pull data from channel_data during their update
}

void WavinSentio::register_climate_group(climate::Climate *climate_entity, const std::vector<uint8_t> &members) {
  ESP_LOGD(TAG, "Registering group climate entity with %u members", members.size());
  // Group climate entities will aggregate data from multiple channels
}

}  // namespace wavin_sentio
}  // namespace esphome
