# Temporary Encoder Debug - Testing Firmware

## Problem Found

Encoder **hardware works** (ISR fires), but **firmware doesn't process it** because:
- FrameworkEngine was created but **never integrated into ILITE.cpp**
- `FrameworkEngine::update()` is never called, so encoderCount is never read

---

## Temporary Fix (FOR TESTING ONLY)

### Changes Made

#### 1. Added Debug to Encoder ISRs
**File:** [input.cpp:43-62](C:\Users\AssilF\Documents\PlatformIO\Projects\ILITE Source\ILITE\lib\ILITE\src\input.cpp#L43)

```cpp
void IRAM_ATTR encoderBtnISR(){
  if(millis() - encoderBtnMillis >= deadtime){
    encoderBtnState = true;
    encoderBtnMillis = millis();
    Serial.println(">>> ENCODER BUTTON PRESSED <<<"); // TEMP DEBUG
  }
}

void IRAM_ATTR encoderClockISR(){
  Serial.print(">>> ENCODER ROTATED: "); // TEMP DEBUG
  if(digitalRead(encoderB) == 1){
    encoderCount++;
    Serial.println("CW"); // TEMP DEBUG
  } else {
    encoderCount--;
    Serial.println("CCW"); // TEMP DEBUG
  }
  Serial.print(">>> encoderCount = ");
  Serial.println(encoderCount); // TEMP DEBUG
}
```

#### 2. Added FrameworkEngine Call in main.cpp
**File:** [main.cpp:192-209](C:\Users\AssilF\Documents\PlatformIO\Projects\ILITE Source\ILITE\src\main.cpp#L192)

```cpp
void loop() {
    ILITE.update();

    // TEMP: Directly call FrameworkEngine to test encoder
    FrameworkEngine::getInstance().update();

    // TEMP: Check encoder state
    extern volatile int encoderCount;
    static int lastEncoderCount = 0;
    if (encoderCount != lastEncoderCount) {
        Serial.printf("[MAIN LOOP] encoderCount changed: %d -> %d\n", lastEncoderCount, encoderCount);
        lastEncoderCount = encoderCount;
    }
}
```

#### 3. Added Include
**File:** [main.cpp:23](C:\Users\AssilF\Documents\PlatformIO\Projects\ILITE Source\ILITE\src\main.cpp#L23)

```cpp
#include <FrameworkEngine.h>  // TEMP: For testing encoder
```

---

## Expected Output Now

### On Encoder Rotation:
```
>>> ENCODER ROTATED: CW
>>> encoderCount = 1
[MAIN LOOP] encoderCount changed: 0 -> 1
[Encoder] Rotated CW, selected button: 1
[AudioRegistry] Playing: menu_select (600Hz, 50ms)
>>> encoderCount = 0
```

### On Encoder Button Press:
```
>>> ENCODER BUTTON PRESSED <<<
[Button] Pin 18: PRESSED
[Encoder] Button pressed, inDashboard: 0, selected: 0
[Encoder] Opening menu (not in dashboard)
[AudioRegistry] Playing: paired (1200Hz, 150ms)
[Menu] Opened
```

---

## What This Tests

1. ✅ **ISR fires** (>>> ENCODER messages)
2. ✅ **encoderCount updates** ([MAIN LOOP] messages)
3. ✅ **FrameworkEngine reads it** ([Encoder] messages)
4. ✅ **Audio plays** ([AudioRegistry] messages)
5. ✅ **Button events work** ([Button] messages)

---

## ⚠️ REMOVE ON NEXT PROMPT

This is **temporary debug code**. Once confirmed working:

### Files to Revert:
1. **input.cpp** - Remove all `Serial.print()` in ISRs (lines 47, 52-61)
2. **input.cpp** - Remove debug in `initInput()` (lines 78-91)
3. **main.cpp** - Remove FrameworkEngine include (line 23)
4. **main.cpp** - Remove FrameworkEngine::update() call (line 197)
5. **main.cpp** - Remove encoderCount monitoring (lines 199-205)

### Proper Integration:
Instead, we need to:
1. Add FrameworkEngine to ILITEFramework class
2. Call frameworkEngine_.update() in control task (50Hz)
3. Call frameworkEngine_.render() in display task (10Hz)

---

## Testing Instructions

1. **Upload code** to ESP32
2. **Open Serial Monitor** (115200 baud)
3. **Rotate encoder** - Should see multiple layers of output:
   - ISR level: `>>> ENCODER ROTATED`
   - Main loop: `[MAIN LOOP] encoderCount changed`
   - Framework: `[Encoder] Rotated CW`
   - Audio: `[AudioRegistry] Playing: menu_select`
4. **Press encoder** - Should see:
   - ISR: `>>> ENCODER BUTTON PRESSED`
   - Button engine: `[Button] Pin 18: PRESSED`
   - Framework: `[Encoder] Button pressed`
   - Audio: `[AudioRegistry] Playing: paired`

---

**Status:** TEMPORARY - For diagnosis only
**Remove:** On next prompt as requested
**Last Updated:** 2025-01-06
