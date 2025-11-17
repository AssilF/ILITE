# TheGill / Mech'Iane Control Scheme Documentation

**Version:** 2.0
**Last Updated:** 2025
**Hardware:** ILITE Universal Controller (ESP32-based)

---

## Table of Contents

1. [Overview](#overview)
2. [Control Modes](#control-modes)
3. [Input Mapping](#input-mapping)
4. [Button Functions](#button-functions)
5. [Potentiometer Functions](#potentiometer-functions)
6. [Visual Feedback](#visual-feedback)
7. [Troubleshooting](#troubleshooting)

---

## Overview

TheGill control system supports three distinct operating modes:
- **Drive Mode:** Tank-drive or differential drive control
- **Arm XYZ Mode:** Cartesian position control with inverse kinematics
- **Arm Orientation Mode:** Tool orientation control (pitch, yaw, roll)

### Hardware Inputs

| Input | GPIO Pin | Type | Configuration |
|-------|----------|------|---------------|
| Joystick A X-axis | GPIO 35 | Analog | ADC Input |
| Joystick A Y-axis | GPIO 34 | Analog | ADC Input |
| Joystick B X-axis | GPIO 39 | Analog | ADC Input |
| Joystick B Y-axis | GPIO 36 | Analog | ADC Input |
| Joystick A Button | GPIO 19 | Digital | INPUT_PULLUP (active LOW) |
| Joystick B Button | GPIO 13 | Digital | INPUT_PULLUP (active LOW) |
| Button 1 (Left Action) | GPIO 23 | Digital | INPUT_PULLUP (active LOW) |
| Button 2 (Shift/Modifier) | GPIO 25 | Digital | INPUT_PULLUP (active LOW) |
| Button 3 (Right Action) | GPIO 27 | Digital | INPUT_PULLUP (active LOW) |
| Potentiometer | GPIO 32 | Analog | ADC Input |
| Encoder A | GPIO 16 | Digital | INPUT_PULLUP (interrupt) |
| Encoder B | GPIO 17 | Digital | INPUT_PULLUP |
| Encoder Button | GPIO 18 | Digital | INPUT_PULLUP (active LOW) |

---

## Control Modes

### 1. Drive Mode

**Purpose:** Control the robot's drivetrain (wheels/tracks)

**Active Indicators:**
- LED 1: Drive speed indicator (scales with drive magnitude)
- LED 2: Low intensity (90) - arm not synced
- LED 3: Low intensity (80) - gripper closed

**Joystick Mapping:**

| Input | Function | Range | Notes |
|-------|----------|-------|-------|
| **Joystick A Y-axis** | Left side drive power | -1.0 to +1.0 | Tank mode: Left tracks |
| **Joystick B Y-axis** | Right side drive power | -1.0 to +1.0 | Tank mode: Right tracks |
| **Joystick A X-axis** | Turn control | -1.0 to +1.0 | Differential mode only |
| **Joystick A Y-axis** | Throttle | -1.0 to +1.0 | Differential mode only |

**Button Functions:**

| Button | Function | Notes |
|--------|----------|-------|
| **Button 1 (Left)** | Toggle LED manual control | Hold Shift + Press |
| **Button 3 (Right)** | Toggle pump manual control | Hold Shift + Press |
| **Joystick A Button** | *(Available)* | Triggers beep feedback |
| **Joystick B Button** | *(Available)* | Triggers beep feedback |

**Potentiometer:**
- **Default:** Drive speed scalar (0.35x to 1.0x)
- **LED Mode:** LED 1 brightness (0-255)
- **Pump Mode:** Pump duty cycle (0-200)

**Drive Profiles:**
1. **Tank Mode** (default): Independent control of left/right sides
2. **Differential Mode**: Single joystick controls throttle and turning

**Drive Easing:**
- **None:** Direct joystick mapping
- **Slew Rate:** Gradual acceleration/deceleration
- **Exponential:** Non-linear response for fine control

---

### 2. Arm XYZ Mode

**Purpose:** Control the arm's end-effector position in 3D space

**Active Indicators:**
- LED 1: Drive LED off (not used)
- LED 2: High intensity (220) - arm synced
- LED 3: Gripper state indicator

**Joystick Mapping:**

| Input | Function | Range | Speed Factor |
|-------|----------|-------|--------------|
| **Joystick A X-axis** | Move in X (forward/back) | 50mm to 450mm | 9.0 mm/s * armSpeedScalar |
| **Joystick A Y-axis** | Move in Y (left/right) | -300mm to +300mm | 9.0 mm/s * armSpeedScalar |
| **Joystick B Y-axis** | Move in Z (up/down) | 0mm to 420mm | 9.0 mm/s * armSpeedScalar |
| **Joystick B X-axis** | *(Unused)* | - | - |

**Button Functions:**

| Button | Function | Notes |
|--------|----------|-------|
| **Button 1 (Hold)** | Close gripper | Continuous motion, 85°/sec |
| **Button 3 (Hold)** | Open gripper | Continuous motion, 85°/sec |
| **Button 2 (Press)** | Switch to Orientation Mode | Quick toggle |
| **Shift + Button 1** | *(Disabled in arm mode)* | - |
| **Shift + Button 3** | *(Disabled in arm mode)* | - |
| **Joystick A Button** | *(Available)* | Triggers beep feedback |
| **Joystick B Button** | *(Available)* | Triggers beep feedback |

**Potentiometer:**
- **Function:** Arm speed scalar (0.2x to 1.0x)

**Coordinate System:**
- **Origin:** Robot base (shoulder pivot)
- **X-axis:** Forward (positive) / Backward (negative)
- **Y-axis:** Left (negative) / Right (positive)
- **Z-axis:** Up (positive) / Down (0)

---

### 3. Arm Orientation Mode

**Purpose:** Control the tool orientation (pitch, yaw, roll) while maintaining position

**Active Indicators:**
- Same as Arm XYZ Mode

**Joystick Mapping:**

| Input | Function | Range | Speed Factor | Units |
|-------|----------|-------|--------------|-------|
| **Joystick A Y-axis** | Pitch (tilt up/down) | -80° to +80° | 0.08 rad/s * armSpeedScalar | ~4.6°/sec @ 1.0x |
| **Joystick A X-axis** | Yaw (rotate left/right) | -180° to +180° (wraps) | 0.08 rad/s * armSpeedScalar | ~4.6°/sec @ 1.0x |
| **Joystick B Y-axis** | Roll (twist) | 0° to 180° | 12.0 deg/s * armSpeedScalar | 12°/sec @ 1.0x |
| **Joystick B X-axis** | *(Unused)* | - | - | - |

**Button Functions:**

| Button | Function | Notes |
|--------|----------|-------|
| **Button 1 (Hold)** | Close gripper | Same as XYZ mode |
| **Button 3 (Hold)** | Open gripper | Same as XYZ mode |
| **Button 2 (Press)** | Switch to XYZ Mode | Quick toggle |
| **Joystick A Button** | *(Available)* | Triggers beep feedback |
| **Joystick B Button** | *(Available)* | Triggers beep feedback |

**Potentiometer:**
- **Function:** Arm speed scalar (0.2x to 1.0x)

**Orientation Angles:**
- **Pitch:** Tool tip angle relative to horizontal plane
  - 0° = horizontal
  - +90° = pointing straight up
  - -90° = pointing straight down
- **Yaw:** Tool rotation around Z-axis (like compass heading)
  - 0° = forward
  - +90° = right
  - -90° = left
- **Roll:** Tool twist around its own axis
  - 90° = neutral position
  - 0° = fully counter-clockwise
  - 180° = fully clockwise

**Note:** The controller uses inverse kinematics to convert orientation commands into joint angles. The arm will attempt to maintain the current XYZ position while changing orientation.

---

## Button Functions

### Global Functions

| Button Combination | Function | Mode Availability |
|-------------------|----------|-------------------|
| **Encoder Button (Press)** | Open/navigate menu | All modes |
| **Encoder Rotate** | Scroll menu / adjust values | All modes |
| **Shift (Hold)** | Modifier key for combos | All modes |

### Mode-Specific Functions

#### Drive Mode Only

| Button Combination | Function | Effect |
|-------------------|----------|--------|
| **Shift + Button 1 (Press)** | Toggle LED control | Switches potentiometer to LED 1 brightness |
| **Shift + Button 3 (Press)** | Toggle pump control | Switches potentiometer to pump duty cycle |
| **Shift + Button 1 (Press again)** | Disable LED control | Returns to drive speed control |
| **Shift + Button 3 (Press again)** | Disable pump control | Returns to drive speed control |

#### Arm Modes (XYZ and Orientation)

| Button Combination | Function | Effect |
|-------------------|----------|--------|
| **Button 1 (Hold)** | Close gripper | Gripper closes at 85°/sec |
| **Button 3 (Hold)** | Open gripper | Gripper opens at 85°/sec |
| **Button 1 XOR Button 3** | Gripper control | Only one button at a time |
| **Button 2 (Press)** | Toggle XYZ ↔ Orientation | Switch between arm control modes |

### Joystick Buttons

| Button | Current Function | Debug Output |
|--------|------------------|--------------|
| **Joystick A Button** | Beep feedback | `[Thegill] Joystick A button pressed!` |
| **Joystick B Button** | Beep feedback | `[Thegill] Joystick B button pressed!` |

**Note:** Joystick buttons are currently configured for testing/feedback and can be assigned custom functions in future versions.

---

## Potentiometer Functions

The potentiometer (GPIO 32) provides continuous analog control (0.0 to 1.0).

### Drive Mode

| Target | Function | Range | Default |
|--------|----------|-------|---------|
| **Drive Speed** (default) | Drive speed multiplier | 0.35x to 1.0x | Active when LED/pump off |
| **LED 1 Brightness** | Manual LED control | 0 to 255 PWM | Active when LED mode enabled |
| **Pump Duty** | Water pump speed | 0 to 200 duty | Active when pump mode enabled |

**Activation:**
- Drive Speed: Default, or press Shift+Button1/Shift+Button3 again to disable other modes
- LED Control: Press Shift + Button 1
- Pump Control: Press Shift + Button 3

**Visual Feedback:**
- Audio cue plays when switching modes
- LED 1 reflects current setting in LED mode

### Arm Modes (XYZ and Orientation)

| Target | Function | Range | Always Active |
|--------|----------|-------|---------------|
| **Arm Speed** | Movement speed multiplier | 0.2x to 1.0x | Yes |

**Effect on Controls:**
- **XYZ Mode:** Scales position change rate (mm/s)
- **Orientation Mode:** Scales rotation rate (rad/s or deg/s)

**Precision Mode:**
- When enabled (via menu or binding): All speeds multiplied by 0.45x
- Allows fine control for delicate operations

---

## Visual Feedback

### LED Status Indicators

TheGill uses three RGB LEDs for status feedback:

| LED | Normal Function | Manual Override | Notes |
|-----|----------------|-----------------|-------|
| **LED 1 (Drive)** | Drive magnitude (0-255) | Manual brightness (LED mode) | Reflects current drive power |
| **LED 2 (Arm)** | Arm sync status | *(None)* | 220=synced, 90=not synced |
| **LED 3 (Gripper)** | Gripper state | *(None)* | 230=open, 80=closed |

### User Mask (Peripheral Byte)

The `userMask` byte encodes system state:

| Bit | Meaning | Value |
|-----|---------|-------|
| 0 | Brake active | 0x01 |
| 1 | Honk active | 0x02 |
| 2 | Arm mode active | 0x04 |
| 3 | Precision mode | 0x08 |
| 4 | Arm state synced | 0x10 |

### Audio Feedback

| Event | Sound | Frequency | Duration |
|-------|-------|-----------|----------|
| **Mode change** | Menu select | 600 Hz | 50 ms |
| **Pairing success** | Paired | 1200 Hz | 150 ms |
| **Pairing lost** | Unpaired | 800 Hz | 100 ms |
| **Joystick button press** | Menu select | 600 Hz | 50 ms |
| **Error** | Error tone | 400 Hz | 300 ms |

---

## Troubleshooting

### Joystick Buttons Not Working

**Symptoms:**
- Pressing joystick buttons produces no beep
- No serial output: `[Thegill] Joystick A/B button pressed!`

**Solution:**
1. **Check wiring:** Buttons should connect GPIO to GND when pressed
2. **Verify pull-ups:** Pins 19 and 13 configured as `INPUT_PULLUP`
3. **Test with multimeter:** Measure voltage - should be ~3.3V normally, ~0V when pressed
4. **Check serial output:** Enable verbose mode and watch for button events

**Hardware Fix:**
- If buttons don't pull to ground, add external 10kΩ pull-up resistors
- Verify button switches are normally-open (not normally-closed)

### Pitch/Yaw Not Responding in Orientation Mode

**Symptoms:**
- Arm doesn't rotate when moving Joystick A in Orientation Mode
- Serial shows: `[Thegill Orientation] Pitch: XX.XX deg, Yaw: XX.XX deg...` but arm doesn't move

**Diagnostic Steps:**

1. **Verify mode:** Check that controller is in **Orientation Mode**
   - Press Button 2 (Shift) to toggle from XYZ to Orientation
   - Display should show "ORI" instead of "XYZ"

2. **Check joystick calibration:**
   - Joystick A should output values other than 0.0 when moved
   - Serial output: `(joyAX: X.XX, joyAY: X.XX)` should change

3. **Monitor orientation angles:**
   - Enable debug output in `thegill.cpp` (already added)
   - Move Joystick A and watch serial: `[Thegill Orientation] Pitch: ...`
   - Angles should change by ~4.6°/sec at 1.0x speed

4. **Check arm command transmission:**
   - Serial should show: `[Thegill ArmCmd] Pitch: XX.X°, Yaw: XX.X°...`
   - This confirms commands are being generated

5. **Verify robot reception:**
   - Check robot firmware receives `ArmControlCommand` packets
   - Magic number should be `0x54474152` ('TGAR')
   - Verify pitch/yaw fields are populated

**Common Causes:**

| Issue | Cause | Fix |
|-------|-------|-----|
| **Joystick not centered** | Calibration drift | Call `InputManager::recalibrateJoysticks()` |
| **Speed too slow** | Potentiometer at minimum | Turn pot to increase arm speed |
| **IK solver failing** | Invalid target orientation | Check serial for IK errors |
| **Robot not receiving** | ESP-NOW not paired | Re-pair controller with robot |
| **Arm outputs disabled** | Safety flag cleared | Enable arm outputs in config |

**Debug Commands:**

Add to main loop for diagnostics:
```cpp
// Print joystick values
Serial.printf("JoyA: X=%.2f Y=%.2f  JoyB: X=%.2f Y=%.2f\n",
              inputs.getJoystickA_X(), inputs.getJoystickA_Y(),
              inputs.getJoystickB_X(), inputs.getJoystickB_Y());

// Print current mode
Serial.printf("Mode: %d (0=Drive, 1=XYZ, 2=Orientation)\n", (int)mechIaneMode);

// Print arm speed
Serial.printf("ArmSpeed: %.2f  PrecisionMode: %d\n", armSpeedScalar, precisionMode);
```

### Control Modes Not Switching

**Symptoms:**
- Pressing Button 2 doesn't toggle between XYZ and Orientation modes
- Encoder button doesn't open menu

**Solution:**
1. **Check button debouncing:** 50ms debounce may need adjustment
2. **Verify button state:** Print `inputs.getButton2Pressed()` to serial
3. **Ensure not in Drive Mode:** Button 2 only toggles in arm modes
   - Use menu or encoder to switch to arm mode first
4. **Clear button latch:** Release all buttons before pressing again

---

## Advanced Configuration

### Speed Tuning

Edit `thegill.cpp` constants for custom speed profiles:

```cpp
// Drive mode (line ~14-18)
constexpr float kMinDriveScale = 0.35f;   // Minimum drive speed (pot at 0%)
constexpr float kMaxDriveScale = 1.0f;    // Maximum drive speed (pot at 100%)

// Arm mode (line ~17-18)
constexpr float kMinArmSpeedScalar = 0.2f;   // Minimum arm speed
constexpr float kMaxArmSpeedScalar = 1.0f;   // Maximum arm speed

// Gripper (line ~20)
constexpr float kGripperSpeedDegPerSec = 85.0f;  // Gripper open/close rate

// Orientation mode (line ~602-603)
const float orientSpeed = 0.08f * armControlScalar;     // Pitch/yaw rate (rad/s)
const float rollSpeedDeg = 12.0f * armControlScalar;    // Roll rate (deg/s)
```

### Deadzone Adjustment

Modify joystick deadzone in `thegill.cpp`:

```cpp
constexpr float kDriveDeadzone = 0.05f;  // 5% deadzone (line ~25)

// Or via InputManager:
InputManager::getInstance().setJoystickDeadzone(0.10f);  // 10% deadzone
```

### Custom Button Bindings

Use the `ControlBindingSystem` for custom behaviors:

```cpp
// Example: Joystick A button toggles precision mode
ControlBinding binding;
binding.input = INPUT_JOYSTICK_A_BUTTON;
binding.event = EVENT_CLICK;
binding.action = []() {
    setPrecisionMode(!isPrecisionModeEnabled());
    AudioRegistry::play("menu_select");
};
ControlBindingSystem::registerBinding(binding);

// Example: Joystick B button cycles camera view
ControlBinding binding2;
binding2.input = INPUT_JOYSTICK_B_BUTTON;
binding2.event = EVENT_CLICK;
binding2.action = []() {
    cycleArmCameraView(1);  // Cycle forward through cameras
};
ControlBindingSystem::registerBinding(binding2);
```

---

## File References

**Core Control Logic:**
- [thegill.cpp:432-667](c:\Users\DELL\Desktop\code\ILITE\lib\ILITE\src\thegill.cpp#L432) - `updateThegillControl()` main loop
- [thegill.cpp:583-614](c:\Users\DELL\Desktop\code\ILITE\lib\ILITE\src\thegill.cpp#L583) - Arm mode control (XYZ and Orientation)
- [thegill.cpp:526-558](c:\Users\DELL\Desktop\code\ILITE\lib\ILITE\src\thegill.cpp#L526) - Drive mode control

**Input Handling:**
- [InputManager.cpp:95-105](c:\Users\DELL\Desktop\code\ILITE\lib\ILITE\src\InputManager.cpp#L95) - Button state update
- [InputManager.cpp:383-398](c:\Users\DELL\Desktop\code\ILITE\lib\ILITE\src\InputManager.cpp#L383) - Button debouncing
- [input.h:6-24](c:\Users\DELL\Desktop\code\ILITE\lib\ILITE\include\input.h#L6) - GPIO pin definitions

**Arm Commands:**
- [thegill.cpp:1240-1253](c:\Users\DELL\Desktop\code\ILITE\lib\ILITE\src\thegill.cpp#L1240) - `acquireArmCommand()` packet builder
- [thegill.h:203-216](c:\Users\DELL\Desktop\code\ILITE\lib\ILITE\include\thegill.h#L203) - `ArmControlCommand` struct

---

## Changelog

### Version 2.0 (2025)
- ✅ Added joystick button support (GPIO 19, 13)
- ✅ Implemented beep feedback for joystick buttons
- ✅ Added debug output for orientation mode (pitch/yaw tracking)
- ✅ Added arm command debug logging
- ✅ Verified pitch/yaw control propagation in orientation mode
- ✅ Documented complete control scheme

### Known Issues
- [ ] Joystick B X-axis is unused in all modes (available for future features)
- [ ] Joystick buttons have no assigned function yet (beyond testing)
- [ ] Precision mode must be enabled via menu (no direct button binding)

---

**End of Documentation**
