# Encoder Debug & Audio Feedback Guide

## Overview

Audio feedback and serial debug output have been added to the encoder system to help troubleshoot and verify encoder functionality.

---

## Audio Feedback

### Encoder Rotation
**Sound:** `menu_select` (600Hz, 50ms)
**Trigger:** Every encoder rotation (clockwise or counterclockwise)
**Purpose:** Confirms encoder is working and being read by the system

### Encoder Button Press
**Sound:** `paired` (1200Hz, 150ms)
**Trigger:** When encoder button is pressed
**Purpose:** Confirms button press detection

### Menu Open
**Sound:** `paired` (1200Hz, 150ms)
**Trigger:** When menu opens
**Purpose:** Confirms menu opened successfully

### Menu Close
**Sound:** `unpaired` (800Hz, 100ms)
**Trigger:** When menu closes
**Purpose:** Confirms menu closed successfully

---

## Serial Debug Output

### Button Events
```
[Button] Pin 18: PRESSED
[Button] Pin 18: RELEASED
[Button] Pin 18: LONG_PRESS
[Button] Pin 23: PRESSED
```

**Format:** `[Button] Pin <GPIO>: <EVENT>`
**Events:** PRESSED, HELD, RELEASED, LONG_PRESS
**Pins:**
- 18 = Encoder button
- 23 = Button 1
- 25 = Button 2
- 27 = Button 3
- 19 = Joystick button A

### Encoder Rotation
```
[Encoder] Rotated CW, selected button: 1
[Encoder] Rotated CCW, selected button: 0
```

**Format:** `[Encoder] Rotated <DIR>, selected button: <INDEX>`
**Direction:** CW (clockwise) or CCW (counterclockwise)
**Selected button index:**
- 0 = Menu (≡ MNU)
- 1 = Function 1 (F1)
- 2 = Function 2 (F2)

### Encoder Button Press
```
[Encoder] Button pressed, inDashboard: 0, selected: 0
[Encoder] Opening menu (not in dashboard)
```

Or when in dashboard:
```
[Encoder] Button pressed, inDashboard: 1, selected: 1
[Encoder] Activating F1: Arm Motors
```

**Format:** Multiple lines showing state and action
**inDashboard:** 1 = in dashboard (paired + module), 0 = not in dashboard
**selected:** Current strip button index (0=Menu, 1=F1, 2=F2)

### Menu System
```
[Menu] Opened
[Menu] Closed
```

**Format:** `[Menu] <STATE>`
**States:** Opened, Closed

---

## Troubleshooting

### No Encoder Rotation Sound/Output
**Problem:** Encoder rotation not detected
**Check:**
1. Encoder pins connected correctly (GPIO 16=A, 17=B, 18=Btn)
2. Encoder ISR attached: `attachInterrupt(encoderA, encoderClockISR, RISING)`
3. Serial output shows encoder count changes
4. FrameworkEngine update() is being called at 50Hz

### No Encoder Button Sound/Output
**Problem:** Encoder button press not detected
**Check:**
1. Encoder button pin connected (GPIO 18)
2. Button ISR attached: `attachInterrupt(encoderBtn, encoderBtnISR, RISING)`
3. Serial shows `[Button] Pin 18: PRESSED`
4. ButtonEventEngine::update() is being called

### Audio Plays But No Action
**Problem:** Sounds play but encoder doesn't navigate
**Check:**
1. encoderCount variable is being read and reset
2. FrameworkEngine::onEncoderRotate() is being called
3. Serial shows rotation output
4. Top strip buttons are being updated

### Button Event Debug Shows Wrong Pin
**Problem:** Serial shows events on unexpected pins
**Check:**
1. Verify pin definitions in `input.h` match hardware
2. Check for wiring issues or shorts
3. Verify INPUT_PULLUP is set correctly

---

## Expected Boot Sequence

```
[Setup] Initializing ILITE framework...
[Framework] Initializing Framework Engine...
[ButtonEventEngine] Initialized

// User rotates encoder
[Encoder] Rotated CW, selected button: 1
// Audio: beep (menu_select)

// User presses encoder
[Button] Pin 18: PRESSED
[Encoder] Button pressed, inDashboard: 0, selected: 1
[Encoder] Opening menu (not in dashboard)
[Menu] Opened
// Audio: beep (paired)

[Button] Pin 18: RELEASED
```

---

## Testing Procedure

### Test 1: Encoder Rotation
1. **Action:** Rotate encoder clockwise
2. **Expected Sound:** Short beep (menu_select)
3. **Expected Serial:** `[Encoder] Rotated CW, selected button: X`
4. **Pass Criteria:** Sound plays and serial output appears for each rotation step

### Test 2: Encoder Button Press
1. **Action:** Press encoder button briefly
2. **Expected Sound:** Medium beep (paired)
3. **Expected Serial:**
   ```
   [Button] Pin 18: PRESSED
   [Encoder] Button pressed, inDashboard: 0, selected: 0
   [Encoder] Opening menu (not in dashboard)
   [Menu] Opened
   [Button] Pin 18: RELEASED
   ```
4. **Pass Criteria:** Two sounds (button press + menu open) and full serial output

### Test 3: Encoder Button Long Press
1. **Action:** Hold encoder button for 800ms+
2. **Expected Sound:** Medium beep after 800ms
3. **Expected Serial:**
   ```
   [Button] Pin 18: PRESSED
   [Button] Pin 18: LONG_PRESS
   [Encoder] Button pressed, inDashboard: 0, selected: 0
   ...
   [Button] Pin 18: RELEASED
   ```
4. **Pass Criteria:** Long press event fires after 800ms

### Test 4: Physical Buttons
1. **Action:** Press Button 1 (GPIO 23)
2. **Expected Serial:** `[Button] Pin 23: PRESSED`
3. **Repeat for:** Button 2 (GPIO 25), Button 3 (GPIO 27)
4. **Pass Criteria:** Each button generates serial output

### Test 5: Strip Button Navigation
1. **Action:** Rotate encoder 3 times clockwise
2. **Expected:**
   - 3 beeps
   - Selected button wraps: 0 → 1 → 2 → 0
3. **Expected Serial:**
   ```
   [Encoder] Rotated CW, selected button: 1
   [Encoder] Rotated CW, selected button: 2
   [Encoder] Rotated CW, selected button: 0
   ```
4. **Pass Criteria:** Button selection wraps around correctly

---

## Files Modified

### Audio Feedback Added
- **[FrameworkEngine.h:22](C:\Users\AssilF\Documents\PlatformIO\Projects\ILITE Source\ILITE\lib\ILITE\include\FrameworkEngine.h#L22)** - Added AudioRegistry include
- **[FrameworkEngine.cpp:408](C:\Users\AssilF\Documents\PlatformIO\Projects\ILITE Source\ILITE\lib\ILITE\src\FrameworkEngine.cpp#L408)** - Audio in onEncoderRotate()
- **[FrameworkEngine.cpp:421](C:\Users\AssilF\Documents\PlatformIO\Projects\ILITE Source\ILITE\lib\ILITE\src\FrameworkEngine.cpp#L421)** - Audio in onEncoderPress()
- **[FrameworkEngine.cpp:467](C:\Users\AssilF\Documents\PlatformIO\Projects\ILITE Source\ILITE\lib\ILITE\src\FrameworkEngine.cpp#L467)** - Audio in openMenu()
- **[FrameworkEngine.cpp:476](C:\Users\AssilF\Documents\PlatformIO\Projects\ILITE Source\ILITE\lib\ILITE\src\FrameworkEngine.cpp#L476)** - Audio in closeMenu()

### Debug Output Added
- **[ButtonEventEngine.cpp:119](C:\Users\AssilF\Documents\PlatformIO\Projects\ILITE Source\ILITE\lib\ILITE\src\ButtonEventEngine.cpp#L119)** - Button event logging
- **[FrameworkEngine.cpp:409](C:\Users\AssilF\Documents\PlatformIO\Projects\ILITE Source\ILITE\lib\ILITE\src\FrameworkEngine.cpp#L409)** - Encoder rotation logging
- **[FrameworkEngine.cpp:422](C:\Users\AssilF\Documents\PlatformIO\Projects\ILITE Source\ILITE\lib\ILITE\src\FrameworkEngine.cpp#L422)** - Encoder press logging
- **[FrameworkEngine.cpp:427-454](C:\Users\AssilF\Documents\PlatformIO\Projects\ILITE Source\ILITE\lib\ILITE\src\FrameworkEngine.cpp#L427)** - Action logging
- **[FrameworkEngine.cpp:468,477](C:\Users\AssilF\Documents\PlatformIO\Projects\ILITE Source\ILITE\lib\ILITE\src\FrameworkEngine.cpp#L468)** - Menu state logging

---

## Binding System Hierarchy

As noted, bindings follow this priority order:

1. **Module Bindings** (highest priority)
   - Active when module is loaded and paired
   - Module's onButtonEvent() handles physical buttons
   - Module's encoder functions override F1/F2

2. **Framework Bindings** (medium priority)
   - Active when no module loaded or not paired
   - Terminal/Devices/Settings for buttons 1/2/3
   - Menu always accessible via encoder (if not in dashboard)

3. **ControlBindingSystem** (configurable)
   - User-defined bindings in main.cpp
   - Can override framework defaults
   - Module bindings still take priority when active

---

## Sound Reference

All sounds registered in main.cpp:

```cpp
AudioRegistry::registerCue({"startup", 1000, 200});      // Boot sound
AudioRegistry::registerCue({"paired", 1200, 150});       // Success/Open
AudioRegistry::registerCue({"unpaired", 800, 100});      // Close/Disconnect
AudioRegistry::registerCue({"menu_select", 600, 50});    // Navigate
AudioRegistry::registerCue({"error", 400, 300});         // Error
```

**Format:** `{name, frequency_hz, duration_ms}`

---

**Last Updated:** 2025-01-06
**Version:** 2.0.0
**Author:** ILITE Team
