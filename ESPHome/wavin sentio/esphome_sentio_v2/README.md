# ESPHome Component: Wavin Sentio (v2)

Modern ESPHome integration for the Wavin Sentio floor heating controller via Modbus RTU (RS-485). This component is inspired by and based on the architecture of the [Wavin AHC 9000 v3 component](https://github.com/heinekmadsen/esphome_wavinahc9000v3).

## Key Features

| Feature | Status | Description |
|---------|--------|-------------|
| Up to 16 channels | ✅ | Automatically discovered through polling |
| Single channel climates | ✅ | One climate entity per channel |
| Group climates | ✅ | Combine multiple channels into one climate entity |
| Comfort climates | ✅ | Use floor temperature as current temp when floor probe detected |
| Battery sensors | ✅ | Per channel (0–100%) |
| Temperature sensors | ✅ | Per channel ambient temperature |
| Floor temperature sensors | ✅ | Automatically detected when floor probe present |
| Humidity sensors | ✅ | Per channel relative humidity |
| Friendly names per channel | ✅ | `channel_XX_friendly_name` config option |
| Modbus retry logic | ✅ | 2-attempt read/write retry |
| Flow control support | ✅ | Optional `flow_control_pin` for RS485 direction control |

## Hardware & Wiring

- **RS-485 (A/B)** from Wavin Sentio controller to a TTL↔RS-485 adapter
- **ESP32 recommended** (tested with GPIO 16/17 for stable UART)
- **Optional direction control:**
  - `flow_control_pin`: GPIO tied to DE & /RE (HIGH during TX, LOW for RX) - **Recommended**
  - `tx_enable_pin`: Legacy driver enable (kept for backward compatibility)

### Choosing flow_control_pin vs tx_enable_pin

| Option | Behavior | Pros | Cons |
|--------|----------|------|------|
| `flow_control_pin` | Pulsed HIGH only while sending | Minimizes bus contention, cleaner half-duplex | Slightly more GPIO toggling |
| `tx_enable_pin` | HIGH enables driver (often kept HIGH) | Simple if already wired | Can hold bus driver enabled longer than needed |

**Recommendation:** Use `flow_control_pin` for new builds.

## Quick Start Example

```yaml
external_components:
  - source: github://yourusername/esphome_sentio_v2
    components: [wavin_sentio]

uart:
  id: uart_sentio
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 19200
  parity: NONE
  stop_bits: 1

modbus:
  id: modbus_sentio
  uart_id: uart_sentio

modbus_controller:
  id: sentio_controller
  modbus_id: modbus_sentio
  address: 0x01

wavin_sentio:
  id: sentio
  modbus_controller_id: sentio_controller
  update_interval: 10s
  flow_control_pin: GPIO10  # Optional RS485 direction control
  poll_channels_per_cycle: 2
  channel_01_friendly_name: "Bedroom"
  channel_02_friendly_name: "Living Room"
  channel_03_friendly_name: "Kitchen"
  channel_04_friendly_name: "Bathroom"

climate:
  # Single channel climate
  - platform: wavin_sentio
    wavin_sentio_id: sentio
    name: "Bedroom"
    channel: 1

  # Group climate (combines multiple channels)
  - platform: wavin_sentio
    wavin_sentio_id: sentio
    name: "Living Room & Kitchen"
    members: [2, 3]

  # Comfort climate (uses floor temperature)
  - platform: wavin_sentio
    wavin_sentio_id: sentio
    name: "Bathroom Comfort"
    channel: 4
    use_floor_temperature: true

sensor:
  # Battery sensor
  - platform: wavin_sentio
    wavin_sentio_id: sentio
    name: "Bedroom Battery"
    channel: 1
    type: battery

  # Temperature sensor
  - platform: wavin_sentio
    wavin_sentio_id: sentio
    name: "Bedroom Temperature"
    channel: 1
    type: temperature

  # Floor temperature sensor (only works if floor probe detected)
  - platform: wavin_sentio
    wavin_sentio_id: sentio
    name: "Bathroom Floor Temperature"
    channel: 4
    type: floor_temperature

  # Humidity sensor
  - platform: wavin_sentio
    wavin_sentio_id: sentio
    name: "Bedroom Humidity"
    channel: 1
    type: humidity

  # Comfort setpoint (read-only)
  - platform: wavin_sentio
    wavin_sentio_id: sentio
    name: "Bedroom Comfort Setpoint"
    channel: 1
    type: comfort_setpoint
```

## Register Mapping (Wavin Sentio)

Based on Sentio SW Ver. 6 register map:

| Register | Description | R/W | Type |
|----------|-------------|-----|------|
| X01 | Desired Temperature | R | Input |
| X02 | HVAC State / Mode | R | Input |
| X03 | Blocking Source | R | Input |
| X04 | Air Temperature (×0.01°C) | R | Input |
| X05 | Floor Temperature (×0.01°C) | R | Input |
| X06 | Relative Humidity (×0.01%) | R | Input |
| X19 | Temperature Setpoint (×0.01°C) | R/W | Holding |

Where X = channel * 100 (e.g., Channel 1 uses 100-series, Channel 2 uses 200-series)

## Climate Entity Features

### Single Channel Climate
- Current temperature from air sensor (register X04)
- Target temperature read/write (register X19)
- Heating/idle action based on mode (register X02)
- Standard HVAC modes: OFF, HEAT

### Group Climate
- **Current temperature:** Average of all member temperatures
- **Target temperature:** Average of all member setpoints (sets all members to same value when changed)
- **Action:** HEATING if any member is heating, otherwise IDLE
- **Friendly naming:** Automatically generates names like "Living Room & Kitchen" or "Bedroom, Office & Hall"

### Comfort Climate (Floor Temperature Based)
- **Current temperature:** From floor sensor (register X05) instead of air sensor
- Only available when floor probe is detected (reading > 1°C and < 90°C)
- Can coexist with standard air-based climate for the same channel
- Useful for precise floor temperature control

## Sensor Types

### Battery Sensor (`type: battery`)
- Reports battery level 0-100%
- Per thermostat channel
- Device class: battery
- Unit: %

### Temperature Sensor (`type: temperature`)
- Ambient air temperature
- Device class: temperature
- Unit: °C
- Accuracy: 0.1°C (data from controller is ×100)

### Floor Temperature Sensor (`type: floor_temperature`)
- Only available if floor probe detected
- Device class: temperature
- Unit: °C
- Accuracy: 0.1°C

### Comfort Setpoint Sensor (`type: comfort_setpoint`)
- Read-only current setpoint value
- Useful for dashboards/historical data
- Device class: temperature
- Unit: °C

### Humidity Sensor (`type: humidity`)
- Relative humidity from thermostat
- Device class: humidity
- Unit: %
- Accuracy: 0.1%

## Configuration Options

### wavin_sentio Component

```yaml
wavin_sentio:
  id: sentio  # Required
  modbus_controller_id: sentio_controller  # Required
  update_interval: 10s  # Optional, default 10s
  poll_channels_per_cycle: 2  # Optional, default 2, range 1-16
  flow_control_pin: GPIO10  # Optional RS485 direction control
  tx_enable_pin: GPIO10  # Optional (legacy, use flow_control_pin instead)
  channel_01_friendly_name: "Bedroom"  # Optional friendly names for channels 1-16
  channel_02_friendly_name: "Living Room"
  # ... up to channel_16_friendly_name
```

### Climate Platform

```yaml
climate:
  - platform: wavin_sentio
    wavin_sentio_id: sentio  # Required
    name: "Bedroom"  # Required
    channel: 1  # Required for single channel (mutually exclusive with members)
    # OR
    members: [2, 3]  # Required for group climate (mutually exclusive with channel)
    use_floor_temperature: false  # Optional, only for single channel, default false
```

### Sensor Platform

```yaml
sensor:
  - platform: wavin_sentio
    wavin_sentio_id: sentio  # Required
    name: "Bedroom Battery"  # Required
    channel: 1  # Required, range 1-16
    type: battery  # Required: battery, temperature, floor_temperature, comfort_setpoint, humidity
```

## Troubleshooting

### No Response from Device

1. **Check wiring:** Verify A/B connections are not swapped
2. **Verify baud rate:** Sentio typically uses 19200
3. **Check device address:** Default is usually 0x01
4. **Flow control:** Try with and without `flow_control_pin`
5. **Enable DEBUG logging:**
   ```yaml
   logger:
     level: DEBUG
   ```

### Floor Temperature Not Working

Floor temperature sensors only work when:
- A floor probe is physically connected to the thermostat
- The probe reading is valid (> 1°C and < 90°C)
- Discovery has completed (wait ~30-60 seconds after boot)

### Climate Shows as Unavailable

- Wait for channel discovery to complete (automatic on first polling cycle)
- Check that the channel number matches a physically present thermostat
- Verify Modbus communication is working (check logs)

### Group Climate Not Updating

- Ensure all member channels are discovered
- Check that member channel numbers are valid (1-16)
- Verify at least one member is responding

## Migration from Old Sentio Component

If you're migrating from the old `sentio` component:

**Old structure:**
```yaml
external_components:
  - source: github://nic6911/esphome_components@Wavin_Sentio

sentio:  # This was just a marker

climate:
  - platform: sentio
    current_temp_sensor_id: temperatur_channel_1
    target_temp_sensor_id: temperatur_setpunkt_channel_1
```

**New structure (v2):**
```yaml
external_components:
  - source: github://yourusername/esphome_sentio_v2
    components: [wavin_sentio]

wavin_sentio:
  id: sentio
  modbus_controller_id: sentio_controller

climate:
  - platform: wavin_sentio
    wavin_sentio_id: sentio
    name: "Bedroom"
    channel: 1  # No need for separate sensor IDs!
```

**Key differences:**
1. All data reading is handled internally by the component
2. No need to define separate modbus sensors/numbers
3. Cleaner YAML with less repetition
4. Built-in support for groups and comfort climates

## Development & Debugging

### Enable Verbose Logging

```yaml
logger:
  level: VERBOSE
  logs:
    wavin_sentio: VERBOSE
    modbus_controller: DEBUG
```

### Testing Individual Channels

Start with one channel to verify wiring:

```yaml
wavin_sentio:
  poll_channels_per_cycle: 1  # Poll one channel at a time

climate:
  - platform: wavin_sentio
    channel: 1  # Test with your known-good channel
```

## Credits

- Based on the architecture of [Wavin AHC 9000 v3](https://github.com/heinekmadsen/esphome_wavinahc9000v3) by heinekmadsen
- Original Sentio component by [nic6911](https://github.com/nic6911/esphome_components)
- Hardware design by [Mogens Groth Nicolaisen](https://github.com/nic6911/Wavin-AHC-9000-mqtt)

## License

MIT License - see LICENSE file for details

## Contributing

Contributions welcome! Please open an issue or PR on GitHub.

## Disclaimer

Community-driven integration; use at your own risk. Not affiliated with Wavin.
