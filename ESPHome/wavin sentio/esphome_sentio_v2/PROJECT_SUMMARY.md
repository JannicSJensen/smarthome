# Wavin Sentio v2 Component - Project Summary

## What We've Created

A modern ESPHome external component for the Wavin Sentio floor heating controller, inspired by and based on the architecture of the [Wavin AHC 9000 v3 project](https://github.com/heinekmadsen/esphome_wavinahc9000v3).

## Project Structure

```
esphome_sentio_v2/
├── components/
│   └── wavin_sentio/
│       ├── __init__.py           # Main component configuration
│       ├── climate.py            # Climate platform configuration
│       ├── sensor.py             # Sensor platform configuration
│       ├── wavin_sentio.h        # C++ main component header
│       └── climate.h             # C++ climate entity header
├── example.yaml                  # Complete usage example
└── README.md                     # Comprehensive documentation

## Key Improvements Over Old Component

### 1. **Integrated Data Management**
- **Old way:** You had to manually define modbus sensors for each channel (temperature, humidity, setpoint)
- **New way:** Component handles all Modbus reads internally - just specify the channel!

### 2. **Group Climate Support**
```yaml
# Combine multiple channels into one climate entity
climate:
  - platform: wavin_sentio
    name: "Living Room & Kitchen"
    members: [2, 3]
```

### 3. **Comfort Climate (Floor Temperature)**
```yaml
# Use floor temperature as current temp instead of air temp
climate:
  - platform: wavin_sentio
    name: "Bathroom Floor"
    channel: 4
    use_floor_temperature: true
```

### 4. **Multiple Sensor Types**
- Battery level
- Air temperature
- Floor temperature (auto-detected)
- Humidity
- Comfort setpoint (read-only)

### 5. **Better Reliability**
- Built-in retry logic (2 attempts)
- Staged polling to avoid bus overload
- Optional flow control for clean RS485 communication

### 6. **Cleaner Configuration**
**Old component required:**
- 3+ entities per channel (sensor, number, climate)
- Manual register address calculations
- Complex lambda functions

**New component:**
```yaml
climate:
  - platform: wavin_sentio
    name: "Bedroom"
    channel: 1
# That's it! Temperature, setpoint, everything handled automatically
```

## Architecture (Based on AHC 9000 v3)

### Component Structure
```
WavinSentio (PollingComponent)
├── Discovers channels automatically
├── Polls channels in batches (configurable)
├── Caches channel data (temperature, humidity, battery, etc.)
└── Provides data to child entities

WavinSentioClimate (Climate entity)
├── Single channel OR group of channels
├── Gets data from parent WavinSentio component
├── Supports standard mode (air temp) or comfort mode (floor temp)
└── Writes setpoint changes back to controller

WavinSentioSensor (Sensor entity)
├── battery, temperature, floor_temperature, humidity, comfort_setpoint
├── Gets data from parent WavinSentio component
└── Publishes to Home Assistant
```

### Register Mapping (Sentio specific)

For Channel N, registers are at N×100 + offset:

| Offset | Register | Description | Type |
|--------|----------|-------------|------|
| +01 | X01 | Desired Temperature | Read (Input) |
| +02 | X02 | HVAC Mode/State | Read (Input) |
| +03 | X03 | Blocking Source | Read (Input) |
| +04 | X04 | Air Temperature (×100) | Read (Input) |
| +05 | X05 | Floor Temperature (×100) | Read (Input) |
| +06 | X06 | Humidity (×100) | Read (Input) |
| +19 | X19 | Setpoint Temperature (×100) | Read/Write (Holding) |

Example: Channel 1 uses 101-106 and 119, Channel 2 uses 201-206 and 219, etc.

## What Still Needs Implementation

The current structure includes the **configuration layer** (Python files) and **interface headers** (C++ .h files), but the **C++ implementation files** (.cpp) still need to be created:

### Required C++ Implementation Files

1. **wavin_sentio.cpp** - Main component logic
   - setup() - Initialize modbus communication
   - loop() - Handle ongoing communication
   - update() - Poll next batch of channels
   - poll_channel() - Read all registers for one channel
   - discover_channels() - Auto-discover active channels
   - Data caching and retry logic

2. **climate.cpp** - Climate entity implementation
   - setup() - Register with parent component
   - traits() - Define supported features (heat mode, temperature ranges)
   - control() - Handle setpoint/mode changes from HA
   - update_state() - Read current state from parent and publish

3. **sensor.cpp** - Sensor entity implementation
   - setup() - Register with parent component
   - update() - Read sensor value from parent and publish

### Additional Files Needed

4. **binary_sensor.py/.h/.cpp** - For optional features like:
   - yaml_ready indicator
   - Heating status per channel

5. **switch.py/.h/.cpp** - For optional features like:
   - Child lock per channel

6. **packages/** directory - For YAML generation helpers (like AHC 9000)
   - yaml_generator.on.yaml
   - yaml_generator.off.yaml
   - Jinja templates for stitching

## How to Complete the Implementation

### Step 1: Implement Core C++ Logic

Create `wavin_sentio.cpp`:
- Implement Modbus read/write helpers using ModbusDevice base class
- Implement polling logic with configurable batch size
- Cache channel data in the `channels_` map
- Handle retry logic for failed reads

### Step 2: Implement Climate Entity

Create `climate.cpp`:
- Implement single channel logic (read temp, write setpoint)
- Implement group logic (average temps, sync setpoints)
- Implement floor temperature variant
- Map Sentio modes to ESPHome climate actions

### Step 3: Implement Sensor Entity

Create `sensor.cpp`:
- Read cached data from parent component
- Apply scaling (×0.01 for temps, ×0.01 for humidity)
- Handle floor sensor availability check

### Step 4: Test Incrementally

1. Start with just one channel
2. Test basic temperature reading
3. Add setpoint writing
4. Add group support
5. Add floor temperature support

## Usage Example (Complete)

```yaml
external_components:
  - source: github://yourusername/esphome_sentio_v2
    components: [wavin_sentio]

wavin_sentio:
  id: sentio
  modbus_controller_id: sentio_controller
  channel_01_friendly_name: "Bedroom"
  channel_02_friendly_name: "Living Room"

climate:
  # Simple single channel
  - platform: wavin_sentio
    wavin_sentio_id: sentio
    name: "Bedroom"
    channel: 1

  # Group climate
  - platform: wavin_sentio
    wavin_sentio_id: sentio
    name: "Main Floor"
    members: [2, 3, 4]

sensor:
  # Battery
  - platform: wavin_sentio
    wavin_sentio_id: sentio
    channel: 1
    type: battery
    name: "Bedroom Battery"

  # Temperature
  - platform: wavin_sentio
    wavin_sentio_id: sentio
    channel: 1
    type: temperature
    name: "Bedroom Temp"
```

## Benefits of This Architecture

1. **Separation of Concerns**
   - Configuration layer (Python) validates YAML
   - Component layer (C++) handles Modbus communication
   - Entity layer (C++) exposes to Home Assistant

2. **Reusability**
   - One component instance serves multiple climate/sensor entities
   - Reduces Modbus traffic (polls once, shares data)

3. **Extensibility**
   - Easy to add new sensor types
   - Easy to add new features (child lock, mode selection, etc.)
   - Based on proven AHC 9000 v3 architecture

4. **User-Friendly**
   - Clean YAML configuration
   - Automatic discovery
   - Friendly naming support

## Next Steps

1. **Implement the C++ files** (.cpp) based on the headers we've created
2. **Test with your actual hardware** - you already have it working with the old component
3. **Add YAML generation helpers** if desired (like AHC 9000 v3)
4. **Publish to GitHub** so others can use it
5. **Submit to ESPHome external components registry**

## Questions?

Feel free to ask about:
- How to implement specific C++ functionality
- How to handle specific Modbus registers
- How to add additional features
- Testing strategies

The architecture is solid, we just need to fill in the implementation details!
