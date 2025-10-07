# Testing Guide for Wavin Sentio v2 Component

## Prerequisites Check
✅ Created test configuration: `heater01_test.yaml`
✅ Component files in: `esphome_sentio_v2/components/`

## Testing Methods

### Method 1: Home Assistant ESPHome Add-on (Recommended)

1. **Copy component to HA config directory**:
   ```
   esphome_sentio_v2/components/wavin_sentio/
   └── Copy to: /config/esphome/esphome_sentio_v2/components/wavin_sentio/
   ```

2. **Upload test config**:
   - Copy `heater01_test.yaml` to your Home Assistant ESPHome directory
   - Open ESPHome dashboard in Home Assistant
   - The new device should appear

3. **Validate configuration**:
   - Click the three dots menu on the device
   - Select "Validate"
   - Check for any errors in the output

4. **Compile** (if validation passes):
   - Click "Install"
   - Select "Manual download" for first test
   - Wait for compilation to complete
   - Check compilation log for warnings

### Method 2: ESPHome Command Line

If you have ESPHome CLI installed:

```bash
# Validate only
esphome config heater01_test.yaml

# Compile
esphome compile heater01_test.yaml

# Compile and upload (if connected via USB)
esphome run heater01_test.yaml
```

### Method 3: Manual Code Review (No Compilation)

You can check the code manually for common issues:

1. **Check all includes exist**:
   - ✅ `wavin_sentio.h`
   - ✅ `climate.h`
   - ✅ `sensor.h`
   - ✅ `wavin_sentio.cpp`
   - ✅ `climate.cpp`
   - ✅ `sensor.cpp`

2. **Python config files**:
   - ✅ `__init__.py`
   - ✅ `climate.py`
   - ✅ `sensor.py`

## What to Look For

### During Validation
- ✅ No Python syntax errors in config files
- ✅ Schema validation passes
- ✅ All imports resolve correctly

### During Compilation
- ✅ C++ files compile without errors
- ✅ No missing header warnings
- ✅ Namespace resolution correct
- ✅ Template instantiation successful

### After Upload (Runtime Testing)
1. **Check logs for discovery**:
   ```
   [INFO] [wavin_sentio] Channel 1 discovered: Bryggers Bad
   [INFO] [wavin_sentio] Channel 2 discovered: Bryggers
   [INFO] [wavin_sentio] Channel 3 discovered: Entre Bad
   [INFO] [wavin_sentio] Channel 4 discovered: Stuen
   ```

2. **Verify climate entities appear in HA**:
   - Bryggers Bad Climate
   - Bryggers Climate
   - Entre Bad Climate
   - Stuen Climate
   - All Zones Group

3. **Verify sensors appear in HA**:
   - Temperature sensors for all 4 channels
   - Humidity sensors where applicable
   - Floor temperature (if sensors present)
   - Battery levels
   - Setpoints

4. **Test climate control**:
   - Change setpoint in HA
   - Check logs for Modbus write:
     ```
     [INFO] [wavin_sentio.climate] Set channel 1 setpoint to 22.5°C
     ```
   - Verify temperature updates

## Expected Behavior

### Component Startup Sequence
```
[CONFIG] Wavin Sentio Component
[CONFIG]   Update Interval: 10s
[CONFIG]   Channels per Cycle: 2
[CONFIG]   Flow Control Pin: GPIO10
[INFO] Discovering channels...
[INFO] Channel 1 discovered: Bryggers Bad
[INFO] Channel 2 discovered: Bryggers
[INFO] Channel 3 discovered: Entre Bad
[INFO] Channel 4 discovered: Stuen
```

### Polling Pattern
With `poll_channels_per_cycle: 2`:
- Cycle 1 (0s): Poll channels 1, 2
- Cycle 2 (10s): Poll channels 3, 4
- Cycle 3 (20s): Poll channels 1, 2
- Repeats...

### Climate Control
1. User sets temperature to 22.5°C in HA
2. Component receives control request
3. Writes to register: `channel × 100 + 19` (e.g., 119 for channel 1)
4. Value written: `2250` (22.5 × 100)
5. Next poll updates current temperature
6. Action updates: IDLE or HEATING based on mode register

## Troubleshooting

### Compilation Errors

**Error: "No such file or directory"**
- Check all .h files are in components/wavin_sentio/
- Verify __init__.py is present
- Check external_components path is correct

**Error: "undefined reference to..."**
- Missing .cpp implementation
- Check all methods declared in .h have implementations in .cpp

### Runtime Errors

**"Channel X not discovered"**
- Check Modbus communication (baud rate, pins, address)
- Verify channel has responding thermostat
- Check register 104 (air temp) returns valid value

**"Failed to write setpoint"**
- Modbus address correct? (device address 1)
- Register 19 writable on your Sentio model?
- Check command_throttle not too aggressive

**Sensors show NaN**
- Channel not discovered yet (wait for polling cycle)
- Register not supported on your device
- Floor sensor: requires physical sensor connected

## Comparison with Old Component

### Old Component (heater01.yaml)
- Manual sensor definitions for every register
- Requires knowledge of register addresses
- No automatic channel discovery
- Each sensor does individual Modbus read
- Heavy Modbus traffic

### New Component (heater01_test.yaml)
- Configure once per channel/sensor type
- Automatic channel discovery
- Friendly names support
- Single Modbus read per channel (all registers)
- Efficient polling with batching
- Group climate support
- Floor temperature support

## Next Steps

1. ✅ **Validate** - Check config syntax
2. ✅ **Compile** - Ensure C++ compiles
3. ✅ **Upload** - Flash to ESP32
4. ✅ **Monitor logs** - Watch discovery and polling
5. ✅ **Test controls** - Change setpoints
6. ✅ **Verify sensors** - Check data updates

## Quick Start Command

If using Home Assistant ESPHome add-on:
1. Upload files to HA
2. Open ESPHome dashboard
3. Click device → Validate → Install

## File Locations Summary

```
Your Setup:
├── heater01.yaml (current working config)
├── heater01_test.yaml (new component test)
└── esphome_sentio_v2/
    └── components/
        └── wavin_sentio/
            ├── __init__.py
            ├── climate.py
            ├── sensor.py
            ├── wavin_sentio.h
            ├── climate.h
            ├── sensor.h
            ├── wavin_sentio.cpp
            ├── climate.cpp
            └── sensor.cpp
```

Need to upload to Home Assistant ESPHome directory for compilation.
