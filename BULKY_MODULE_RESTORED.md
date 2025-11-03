# Bulky Module Restored

## Summary

The Bulky utility carrier robot module has been successfully formalized and integrated into the ILITE framework, following the same pattern as TheGill and Drongaze modules.

---

## What Was Done

### 1. ✅ Fixed ControlBindingSystem Compilation Error

**Issue**: Brace-enclosed initializer lists weren't being converted to `ControlBinding&`

**Fix**: Changed from designated initializers to explicit struct creation in [src/main.cpp](src/main.cpp):

```cpp
// Before (broken):
ControlBindingSystem::registerBinding({
    .input = INPUT_BUTTON3,
    .event = EVENT_CLICK,
    .action = []() { AudioRegistry::play("menu_select"); }
});

// After (fixed):
ControlBinding binding1;
binding1.input = INPUT_BUTTON3;
binding1.event = EVENT_CLICK;
binding1.action = []() { AudioRegistry::play("menu_select"); };
binding1.condition = nullptr;
binding1.screenId = nullptr;
binding1.duration = 0;
binding1.priority = 0;
ControlBindingSystem::registerBinding(binding1);
```

---

### 2. ✅ Created Bulky Module Files

#### [lib/ILITE/include/bulky.h](lib/ILITE/include/bulky.h)

Defines Bulky-specific data structures:

**Packets**:
- `BulkyCommand` - Command sent to Bulky (speed, motion state, button states)
- `BulkyTelemetry` - Telemetry from Bulky (sensors, fire detection, line position)

**State**:
- `BulkyControlState` - Runtime control state (speed, honk, lights, connection)

**Functions**:
- `initBulkyState()` - Initialize Bulky state
- `updateBulkyControl()` - Update control logic
- `handleBulkyTelemetry()` - Process incoming telemetry
- `getBulkyPayload()` - Get command packet for transmission
- `drawBulkyDashboard()` - Draw main dashboard (already in display.cpp)
- `drawBulkyLayoutCard()` - Draw home screen card

#### [lib/ILITE/src/bulky.cpp](lib/ILITE/src/bulky.cpp)

Implements Bulky control logic and module class:

**Control Logic**:
- Reads joystick B for motion control (X/Y axes)
- Maps joystick to speed (-100 to 100) with deadzone
- Encodes motion state (stop, forward, backward, turn left/right)
- Reads potentiometer for speed scaling
- Reads button states

**Telemetry Handling**:
- Validates magic number (0x42554C4B = 'BULK')
- Updates display variables (Front_Distance, Bottom_Distance, linePosition, firePose, speed)
- Tracks connection status

**Module Class** (`BulkyModule`):
- Implements `ILITEModule` interface
- Auto-detection keywords: "bulky", "carrier", "utility"
- Packet descriptors for command and telemetry
- Lifecycle methods (onInit, onPair, onUnpair)
- Control update and packet handling
- Display methods

**Module Registration**:
```cpp
REGISTER_MODULE(BulkyModule);
```

---

### 3. ✅ Bulky Infrastructure Already Existed

The following infrastructure was already in place:

**Display Functions** (in [display.cpp](lib/ILITE/src/display.cpp)):
- `drawBulkyDashboard()` - Main dashboard
- `drawFirePosition()` - Fire detection servo angle
- `drawLine()` - Line sensor 4-bit position
- `drawMotionJoystickPose()` - Joystick B position
- `drawPeripheralJoystickPose()` - Joystick A position
- `drawProximity()` - Front and bottom proximity sensors
- `drawSpeed()` - Current speed and speed potentiometer

**Display Variables** (in [display.h](lib/ILITE/include/display.h)):
- `Front_Distance` - Front proximity sensor (0-50 cm)
- `Bottom_Distance` - Bottom proximity sensor (0-50 cm)
- `speed` - Current speed (0-100%)
- `linePosition` - Line sensor bitmask (4 bits)
- `firePose` - Fire detection angle (0-180 degrees)

**Telemetry** (in [telemetry.h/cpp](lib/ILITE/src/telemetry.cpp)):
- `BulkyCommand bulkyCommand` - Already declared and initialized

**UI Enums** (in [ui_modules.h](lib/ILITE/include/ui_modules.h)):
- `PeerKind::Bulky` - Module type identifier

---

## How It Works

### Module Auto-Detection

When an ESP-NOW device pairs with ILITE, the framework:

1. Calls `ModuleRegistry::findModuleByName(deviceName)`
2. Searches all registered modules for keyword matches
3. BulkyModule provides keywords: "bulky", "carrier", "utility"
4. If device name contains any keyword, BulkyModule is selected

Example device names that will match:
- "Bulky_Robot_01"
- "Carrier_Bot"
- "Utility_Carrier"

### Control Flow

```
loop() → ILITE.update() → activeModule->updateControl()
                         → BulkyModule::updateControl()
                         → updateBulkyControl()
                         → Read joysticks, buttons, potentiometer
                         → Update bulkyCommand packet
                         → Framework sends packet via ESP-NOW
```

### Telemetry Flow

```
ESP-NOW Receive → ILITE::handleTelemetry() → activeModule->handleTelemetry()
                                            → BulkyModule::handleTelemetry()
                                            → handleBulkyTelemetry()
                                            → Validate magic number
                                            → Update display variables
                                            → Update connection status
```

### Display Flow

```
displayTask() → ILITE::displayUpdate() → activeModule->drawDashboard()
                                       → BulkyModule::drawDashboard()
                                       → drawBulkyDashboard() (in display.cpp)
                                       → Draw all Bulky UI elements
```

---

## Bulky Dashboard Layout

```
┌─────────────────────────────────────┐
│ STATUS BAR (Battery, Signal, etc.)  │
├─────────────────────────────────────┤
│                                      │
│  ┌──┐  ┌────────────────┐  ┌─┬─┐   │
│  │F │  │   Fire Servo   │  │ │ │   │  Front Distance
│  │R │  │   ∧ Position   │  │ │▓│   │  Bottom Distance
│  │O │  └────────────────┘  └─┴─┘   │  (Proximity)
│  │N │                               │
│  │T │  ┌────────────────┐  ┌─────┐ │
│  │  │  │   Line Sens   │   │  B  │ │  Joystick B
│  │D │  │   [x][x][][]  │   │ ● →│ │  (Motion)
│  │I │  └────────────────┘  └─────┘ │
│  │S │                               │
│  │T │  ┌────────────────┐  ┌─────┐ │
│  └──┘  │  Speed: 75%    │  │  A  │ │  Joystick A
│         │  ▓▓▓▓▓▓▓░░░░  │  │  ●  │ │  (Peripheral)
│         └────────────────┘  └─────┘ │
│                                      │
└─────────────────────────────────────┘
```

---

## Bulky Features

### Sensors

1. **Front Proximity** - Front distance sensor (0-50 cm)
2. **Bottom Proximity** - Bottom distance sensor (0-50 cm)
3. **Line Sensor** - 4-bit line position detector
4. **Fire Detection** - Servo-mounted IR sensor (0-180 degrees)

### Controls

1. **Joystick B (Motion)**:
   - Y-axis: Speed control (-100 to 100)
   - X-axis: Steering/turning
   - Motion states: Stop, Forward, Backward, Turn Left, Turn Right

2. **Potentiometer A (Speed Scale)**:
   - Scales target speed (0-100%)
   - Allows fine speed adjustment

3. **Buttons (1-3)**:
   - Programmable functions
   - Sent in telemetry to Bulky robot

### Display Elements

1. **Fire Position** - Shows fire detection servo angle
2. **Line Sensor** - 4-bit visual indicator
3. **Proximity Sensors** - Bar graphs for front/bottom distance
4. **Speed Indicator** - Current speed percentage with bar
5. **Joystick Positions** - Real-time joystick A/B visualization

---

## Testing Bulky

### 1. Build and Upload

```bash
cd "C:\Users\AssilF\Documents\PlatformIO\Projects\ILITE Source\ILITE"
pio run -t upload
```

### 2. Verify Registration

Check serial output for:
```
[ModuleRegistry] Registered module: Bulky (com.ilite.bulky) v1.0.0
```

### 3. Pair with Bulky Robot

1. Power on Bulky robot
2. Bulky should advertise itself via ESP-NOW
3. ILITE will detect "bulky" keyword in device name
4. Auto-select BulkyModule
5. Display switches to Bulky dashboard

### 4. Serial Output

Expected output when paired:
```
[BulkyModule] Initialized
[BulkyModule] Paired with Bulky_Robot_01
```

Expected output when receiving telemetry:
```
(No output - telemetry is silently processed)
```

### 5. Test Controls

- Move Joystick B up/down → Speed changes (-100 to 100)
- Move Joystick B left/right → Motion state changes (turn left/right)
- Adjust Potentiometer A → Speed scaling changes
- Press Buttons 1-3 → Button states sent to Bulky

### 6. Test Display

- Front/bottom proximity sensors → Bars update
- Line sensor → 4-bit position updates
- Fire detection → Servo angle indicator updates
- Speed → Bar graph and percentage update

---

## Module Comparison

| Feature | TheGill | Drongaze | Generic | Bulky |
|---------|---------|----------|---------|-------|
| **Type** | Differential Drive | Quadcopter | Universal | Utility Carrier |
| **Keywords** | "gill", "thegill" | "drone", "drongaze" | (fallback) | "bulky", "carrier", "utility" |
| **Control** | 4-wheel drive | Flight (throttle, pitch, roll, yaw) | Raw joystick | Speed + steering |
| **Telemetry** | IMU, PID | IMU, PID, altitude | Generic | Sensors, line, fire |
| **Special Features** | Brake, honk, easing | Stabilization, arm/disarm | None | Proximity, line follow, fire detect |

---

## Files Modified/Created

### Created:
1. ✅ [lib/ILITE/include/bulky.h](lib/ILITE/include/bulky.h) - Bulky module header
2. ✅ [lib/ILITE/src/bulky.cpp](lib/ILITE/src/bulky.cpp) - Bulky module implementation

### Modified:
3. ✅ [src/main.cpp](src/main.cpp) - Fixed ControlBindingSystem calls

### Existing (Unchanged):
4. ✅ [lib/ILITE/src/display.cpp](lib/ILITE/src/display.cpp) - Already had `drawBulkyDashboard()` and related functions
5. ✅ [lib/ILITE/include/display.h](lib/ILITE/include/display.h) - Already had Bulky display variables
6. ✅ [lib/ILITE/src/telemetry.cpp](lib/ILITE/src/telemetry.cpp) - Already had `bulkyCommand` initialization
7. ✅ [lib/ILITE/include/telemetry.h](lib/ILITE/include/telemetry.h) - Already had `BulkyCommand` struct

---

## Summary

✅ **Bulky module fully integrated into ILITE framework**
✅ **Auto-detection via keywords: "bulky", "carrier", "utility"**
✅ **Full control logic implemented (joystick, speed, buttons)**
✅ **Telemetry handling with magic number validation**
✅ **Dashboard display with all Bulky-specific UI elements**
✅ **Module registration with REGISTER_MODULE macro**
✅ **Ready to pair with Bulky robot hardware**

**Bulky is now a first-class ILITE module alongside TheGill, Drongaze, and Generic!**
