#pragma once

#include "esphome/core/component.h"
#include "esphome/components/modbus/modbus.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include <vector>
#include <map>
#include <string>

namespace esphome {
namespace wavin_sentio {

// Sentio register addresses (base addresses for each channel)
// Channel N uses base addresses: N*100 + offset
// For example, Channel 1: 100-series, Channel 2: 200-series, etc.

struct ChannelData {
  uint8_t channel_id;
  bool discovered{false};
  bool has_floor_sensor{false};
  
  // Sensor values
  float current_temperature{NAN};
  float floor_temperature{NAN};
  float humidity{NAN};
  float target_temperature{NAN};
  float battery_level{NAN};
  
  // Mode/State (from register X02)
  uint16_t mode{0};
  
  // Friendly name
  std::string friendly_name;
  
  ChannelData() = default;
  explicit ChannelData(uint8_t id) : channel_id(id) {}
};

class WavinSentio : public PollingComponent, public modbus::ModbusDevice {
 public:
  WavinSentio() = default;
  
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  
  float get_setup_priority() const override { return setup_priority::DATA; }
  
  // Configuration methods
  void set_flow_control_pin(uint8_t pin) { this->flow_control_pin_ = pin; }
  void set_tx_enable_pin(uint8_t pin) { this->tx_enable_pin_ = pin; }
  void set_poll_channels_per_cycle(uint8_t count) { this->poll_channels_per_cycle_ = count; }
  void set_channel_friendly_name(uint8_t channel, const std::string &name);
  
  // Data access methods
  ChannelData* get_channel_data(uint8_t channel);
  bool is_channel_discovered(uint8_t channel);
  
  // Modbus read/write helpers
  bool read_register(uint8_t channel, uint8_t offset, uint16_t &value);
  bool write_register(uint8_t channel, uint8_t offset, uint16_t value);
  
  // Register a climate entity
  void register_climate(climate::Climate *climate_entity, uint8_t channel);
  void register_climate_group(climate::Climate *climate_entity, const std::vector<uint8_t> &members);
  
 protected:
  void poll_channel(uint8_t channel);
  void discover_channels();
  uint16_t get_register_address(uint8_t channel, uint8_t offset);
  
  optional<uint8_t> flow_control_pin_{};
  optional<uint8_t> tx_enable_pin_{};
  uint8_t poll_channels_per_cycle_{2};
  uint8_t current_poll_channel_{1};
  
  std::map<uint8_t, ChannelData> channels_;
  std::map<uint8_t, std::string> friendly_names_;
  
  // Retry logic
  uint8_t retry_count_{0};
  static constexpr uint8_t MAX_RETRIES = 2;
};

}  // namespace wavin_sentio
}  // namespace esphome
