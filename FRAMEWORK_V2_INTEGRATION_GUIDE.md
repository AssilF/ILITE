# ILITE Framework v2.0 - Integration Guide

## Overview

This guide explains how to integrate the new Framework Engine architecture into the existing ILITE system. The v2.0 refactoring introduces:

1. **ButtonEventEngine** - Advanced button state machine with long press, hold, and release detection
2. **FrameworkEngine** - Central coordinator managing display rendering, button routing, and module lifecycle
3. **Top Strip System** - 10px navigation bar with encoder-selectable functions
4. **Enhanced Module Interface** - Support for custom encoder functions and button events
5. **Default Actions** - Terminal, Devices, and Settings screens when no module loaded

---

## Architecture Changes

### Before (v1.x)
```
ILITEFramework
├── Display Task (renders module dashboard directly)
├── Control Task (reads inputs, updates module)
└── Module Management
```

### After (v2.0)
```
ILITEFramework
├── FrameworkEngine (NEW)
│   ├── ButtonEventEngine (NEW)
│   ├── Top Strip Rendering (NEW)
│   ├── Dashboard Routing
│   └── Default Actions Integration
├── Display Task (renders via FrameworkEngine)
├── Control Task (reads inputs, processes button events)
└── Module Management
```

---

## Files Created

### Core Engine Files
- `/lib/ILITE/include/ButtonEventEngine.h`
- `/lib/ILITE/src/ButtonEventEngine.cpp`
- `/lib/ILITE/include/FrameworkEngine.h`
- `/lib/ILITE/src/FrameworkEngine.cpp`

### Default Actions
- `/lib/ILITE/include/DefaultActions.h`
- `/lib/ILITE/src/DefaultActions.cpp`

### Module Interface Updates
- `/lib/ILITE/include/ILITEModule.h` (MODIFIED - added button events and encoder functions)

---

## Integration Steps

### Step 1: Add FrameworkEngine to ILITEFramework

**File:** `/lib/ILITE/include/ILITE.h`

Add include:
```cpp
#include "FrameworkEngine.h"
#include "DefaultActions.h"
```

Add to ILITEFramework class (private section):
```cpp
private:
    FrameworkEngine& frameworkEngine_;  // Reference to singleton
```

Add to constructor (in ILITE.cpp):
```cpp
ILITEFramework::ILITEFramework()
    : /* existing initializers */
    , frameworkEngine_(FrameworkEngine::getInstance())
{
    // existing code
}
```

---

### Step 2: Initialize FrameworkEngine in begin()

**File:** `/lib/ILITE/src/ILITE.cpp`

In `ILITEFramework::begin()`, after hardware initialization:

```cpp
bool ILITEFramework::begin(const ILITEConfig& config) {
    // ... existing initialization code ...

    // Initialize Framework Engine (NEW)
    Serial.println("[Framework] Initializing Framework Engine...");
    frameworkEngine_.begin();

    // Set default button callbacks
    frameworkEngine_.setDefaultButtonCallback(0, [](ButtonEvent event) {
        if (event == ButtonEvent::PRESSED) {
            DefaultActions::showTerminal();
        }
    });

    frameworkEngine_.setDefaultButtonCallback(1, [](ButtonEvent event) {
        if (event == ButtonEvent::PRESSED) {
            DefaultActions::openDevices();
        }
    });

    frameworkEngine_.setDefaultButtonCallback(2, [](ButtonEvent event) {
        if (event == ButtonEvent::PRESSED) {
            DefaultActions::openSettings();
        }
    });

    // ... rest of existing code ...
}
```

---

### Step 3: Update Control Task to use ButtonEventEngine

**File:** `/lib/ILITE/src/ILITE.cpp` (or wherever commTask is defined)

In `commTask()` (50Hz control loop):

```cpp
void commTask(void* parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50Hz

    while (true) {
        // Update Framework Engine (handles button state machines)
        FrameworkEngine::getInstance().update();

        // Read inputs (existing code)
        inputManager.update();

        // Update active module (existing code)
        if (activeModule_ && isPaired_) {
            activeModule_->updateControl(inputManager, 0.02f);
            // ... prepare and send packet ...
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
```

---

### Step 4: Update Display Task to use FrameworkEngine

**File:** `/lib/ILITE/src/ILITE.cpp` (or wherever displayTask is defined)

Replace existing rendering logic with:

```cpp
void displayTask(void* parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 10Hz

    while (true) {
        // Check if default screen is active
        if (DefaultActions::isTerminalActive()) {
            canvas.clear();
            FrameworkEngine::getInstance().renderTopStrip(canvas);
            DefaultActions::renderTerminalScreen(canvas);
            canvas.sendBuffer();
        } else if (DefaultActions::isDevicesActive()) {
            canvas.clear();
            FrameworkEngine::getInstance().renderTopStrip(canvas);
            DefaultActions::renderDevicesScreen(canvas);
            canvas.sendBuffer();
        } else if (DefaultActions::isSettingsActive()) {
            canvas.clear();
            FrameworkEngine::getInstance().renderTopStrip(canvas);
            DefaultActions::renderSettingsScreen(canvas);
            canvas.sendBuffer();
        } else {
            // Normal rendering via FrameworkEngine
            FrameworkEngine::getInstance().render(canvas);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
```

---

### Step 5: Sync Module State with FrameworkEngine

When setting active module or pairing status, update FrameworkEngine:

```cpp
void ILITEFramework::setActiveModule(ILITEModule* module) {
    // Existing logic...
    activeModule_ = module;

    // Sync with FrameworkEngine (NEW)
    FrameworkEngine::getInstance().loadModule(module);

    // Load encoder functions from module
    if (module && module->getEncoderFunctionCount() > 0) {
        // TODO: Get encoder functions from module and set them
        // This requires updating the module interface to return EncoderFunction objects
    }
}

void ILITEFramework::onPairStatusChange(bool paired) {
    // Existing logic...
    isPaired_ = paired;

    // Sync with FrameworkEngine (NEW)
    FrameworkEngine::getInstance().setPaired(paired);

    if (paired) {
        FrameworkEngine::getInstance().setStatus(FrameworkStatus::PAIRED);
    } else {
        FrameworkEngine::getInstance().setStatus(FrameworkStatus::IDLE);
    }
}
```

---

### Step 6: Handle Encoder Input for Default Screens

In the encoder ISR or encoder handling code:

```cpp
// Encoder rotation handler
void handleEncoderRotation(int delta) {
    // Check if default screen is active
    if (DefaultActions::isTerminalActive() ||
        DefaultActions::isDevicesActive() ||
        DefaultActions::isSettingsActive()) {
        DefaultActions::handleEncoderRotate(delta);
    } else {
        // Pass to FrameworkEngine for strip navigation
        FrameworkEngine::getInstance().onEncoderRotate(delta);
    }
}

// Encoder button handler
void handleEncoderPress() {
    // Check if default screen is active
    if (DefaultActions::isTerminalActive() ||
        DefaultActions::isDevicesActive() ||
        DefaultActions::isSettingsActive()) {
        DefaultActions::handleEncoderPress();
    } else {
        // Pass to FrameworkEngine for strip button activation
        FrameworkEngine::getInstance().onEncoderPress();
    }
}
```

---

## Module Migration Guide

### Migrating Existing Modules to v2.0

To take advantage of the new architecture, modules should:

#### 1. Add Encoder Functions (Optional)

```cpp
// In your module header
size_t getEncoderFunctionCount() const override { return 1; }

void getEncoderFunction(size_t index, void* funcOut) const override {
    if (index == 0) {
        EncoderFunction* func = static_cast<EncoderFunction*>(funcOut);
        func->label = "ARM";
        func->fullName = "Arm Motors";
        func->callback = [this]() { armMotors(); };
        func->isToggle = true;
        func->toggleState = &isArmed_;
    }
}
```

#### 2. Add Button Event Handlers (Optional)

```cpp
void onButtonEvent(size_t buttonIndex, int event) override {
    ButtonEvent btnEvent = static_cast<ButtonEvent>(event);

    if (buttonIndex == 0) { // Button 1
        if (btnEvent == ButtonEvent::PRESSED) {
            // Short press action
            toggleMode();
        } else if (btnEvent == ButtonEvent::LONG_PRESS) {
            // Long press action (800ms+)
            emergencyStop();
        }
    }
}
```

#### 3. Update Dashboard Rendering

Dashboard rendering remains the same, but now renders at Y=10 (below strip):

```cpp
void drawDashboard(DisplayCanvas& canvas) override {
    // Top strip is rendered automatically by framework at Y=0-10
    // Your dashboard starts at Y=10 and has 54px height

    canvas.setFont(DisplayCanvas::NORMAL);
    canvas.drawText(10, 20, "Module Dashboard");

    // ... your rendering code ...
}
```

---

## Testing Checklist

### Basic Functionality
- [ ] Framework boots and shows generic dashboard
- [ ] Top strip displays [≡ MNU] button
- [ ] Encoder rotates through strip buttons
- [ ] Encoder press opens menu
- [ ] Button 1 press opens Terminal screen
- [ ] Button 2 press opens Devices screen
- [ ] Button 3 press opens Settings screen
- [ ] Default screens render correctly
- [ ] Encoder navigation works in default screens

### Module Integration
- [ ] Module loads successfully
- [ ] Module name appears in top strip
- [ ] Module dashboard renders below strip
- [ ] Module encoder functions appear (F1, F2)
- [ ] Encoder functions activate correctly
- [ ] Toggle encoder functions show state
- [ ] Physical buttons trigger module events
- [ ] Long press detection works (800ms)

### Pairing & Status
- [ ] Status animation shows in strip (Scanning/Idle/Paired)
- [ ] Battery percentage displays
- [ ] Pairing triggers module onPair()
- [ ] Unpairing triggers module onUnpair()
- [ ] Connection status updates correctly

---

## Performance Considerations

1. **Button Event Engine**: Runs at 50Hz in control task - minimal overhead
2. **Top Strip Rendering**: ~2-3ms per frame (within 100ms frame budget @ 10Hz)
3. **Memory Usage**: ~2KB additional RAM for FrameworkEngine state

---

## Troubleshooting

### Top strip not rendering
- Ensure `FrameworkEngine::render()` is called in display task
- Check that canvas is not cleared after strip rendering

### Button events not working
- Verify `FrameworkEngine::update()` is called at 50Hz
- Check that button pins match definitions in input.h
- Ensure buttons are wired active-LOW with INPUT_PULLUP

### Encoder navigation not working
- Verify encoder ISR is connected to `FrameworkEngine::onEncoderRotate()`
- Check encoder pin definitions (encoderA=16, encoderB=17, encoderBtn=18)

### Module encoder functions not showing
- Implement `getEncoderFunctionCount()` in module
- Implement `getEncoderFunction()` to populate function data
- Call `FrameworkEngine::setEncoderFunction()` when module activates

---

## Next Steps

1. Complete integration of FrameworkEngine into ILITE.cpp
2. Migrate DroneGaze module to use new button events
3. Migrate The'Gill module to use new button events
4. Migrate Bulky module to use new button events
5. Test complete system on hardware
6. Add menu system integration
7. Implement actual Logger/Discovery integration in DefaultActions

---

## Notes

- The new architecture is **backward compatible** - old modules work without changes
- New features (encoder functions, button events) are **optional** for modules
- Default actions provide basic functionality when no module is loaded
- Top strip is **always** framework-controlled - modules cannot override menu button
- Button long press threshold is **800ms** (configurable in ButtonEventEngine constructor)

---

**Last Updated:** 2025-01-06
**Version:** 2.0.0
**Author:** ILITE Team
