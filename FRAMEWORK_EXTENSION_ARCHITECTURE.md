# ILITE Framework Extension Architecture

## Overview

ILITE is now a **fully extensible framework** where users can create modules without knowing internal implementation details. Everything is modular and registration-based.

## Core Principles

1. **Zero Internal Knowledge Required** - Users never touch WiFi, ESP-NOW, display drivers, or hardware
2. **Registration-Based** - Everything uses `REGISTER_X()` macros for zero boilerplate
3. **Event-Driven** - Controls, menus, and screens use callbacks
4. **Conditional** - Menu entries and features can have visibility conditions
5. **Hierarchical** - Menus, screens, and bindings support parent-child relationships
6. **Hot-Pluggable** - Extensions can be added/removed at compile time

---

## Extension Systems

### 1. Audio Extension System

Users can register custom sounds with friendly names.

#### Usage:
```cpp
// Define custom tones
REGISTER_AUDIO("motor_armed", 1000, 100);  // 1000Hz, 100ms
REGISTER_AUDIO("motor_disarmed", 500, 200);

// Play in module
void onArmMotors() {
    Audio::play("motor_armed");
}
```

#### Implementation:
- `AudioRegistry` - Stores sound definitions
- `REGISTER_AUDIO()` macro - Compile-time registration
- `Audio::play(name)` - Play by name

---

### 2. Menu Extension System

Users can add menu entries with conditions, icons, and callbacks.

#### Usage:
```cpp
// Register menu entry
REGISTER_MENU_ENTRY(
    .id = "drone.pid_tuning",
    .parent = "settings",  // Nested under "Settings"
    .icon = ICON_TUNING,
    .label = "PID Tuning",
    .condition = []() { return activeModuleIs("drongaze"); },
    .onSelect = []() { ScreenRegistry::show("pid_tuning_screen"); }
);

// Register conditional entry (only visible when armed)
REGISTER_MENU_ENTRY(
    .id = "drone.disarm",
    .parent = "quick_actions",
    .icon = ICON_LOCK,
    .label = "Disarm Motors",
    .condition = []() { return drongazeState.armed; },
    .onSelect = []() { drongazeState.armed = false; }
);
```

#### Features:
- **Hierarchical** - Unlimited nesting depth
- **Conditional** - Visibility based on runtime state
- **Icons** - Custom bitmap icons (8x8, 16x16)
- **Callbacks** - Lambda or function pointer
- **Auto-sorted** - By priority or alphabetically

---

### 3. Screen Extension System

Users can register custom screens that appear in menus or are triggered by events.

#### Usage:
```cpp
// Register screen
REGISTER_SCREEN(
    .id = "pid_tuning_screen",
    .title = "PID Tuning",
    .icon = ICON_TUNING,
    .drawFunc = drawPIDTuningScreen,
    .updateFunc = updatePIDTuningScreen,  // Called at 10Hz
    .onEncoderRotate = handlePIDEncoderRotate,
    .onEncoderPress = handlePIDEncoderPress,
    .onButton1 = handlePIDButton1
);

void drawPIDTuningScreen(DisplayCanvas& canvas) {
    canvas.clear();
    canvas.drawIcon(0, 0, ICON_TUNING);
    canvas.drawText(16, 8, "PID Tuning");

    canvas.drawTextF(0, 24, "P: %.3f", drongazeState.pidP);
    canvas.drawProgressBar(40, 24, 80, 6, drongazeState.pidP / 10.0);

    // ... more tuning parameters
}
```

#### Features:
- **Standalone** - Accessible from menu
- **Module-Associated** - Shown only when module is active
- **Event Handlers** - Encoder, buttons, timers
- **Modal** - Can be overlays or full-screen
- **Transitions** - Slide, fade, instant

---

### 4. Control Binding System

Users can bind inputs to actions with event types (press, release, hold, double-tap).

#### Usage:
```cpp
// Register control bindings
REGISTER_CONTROL_BINDING(
    .input = INPUT_BUTTON1,
    .event = EVENT_PRESS,
    .condition = []() { return isPaired(); },
    .action = []() {
        drongazeState.armed = !drongazeState.armed;
        Audio::play(drongazeState.armed ? "motor_armed" : "motor_disarmed");
    }
);

REGISTER_CONTROL_BINDING(
    .input = INPUT_BUTTON2,
    .event = EVENT_HOLD,  // Hold for 1 second
    .duration = 1000,
    .action = []() {
        // Emergency stop
        drongazeState.armed = false;
        drongazeState.throttle = 0;
        Audio::play("emergency_stop");
    }
);

REGISTER_CONTROL_BINDING(
    .input = INPUT_ENCODER_ROTATE,
    .screen = "pid_tuning_screen",  // Only active on this screen
    .action = [](int delta) {
        drongazeState.pidP += delta * 0.001;
        drongazeState.pidP = constrain(drongazeState.pidP, 0.0, 10.0);
    }
);
```

#### Event Types:
- `EVENT_PRESS` - Button pressed (rising edge)
- `EVENT_RELEASE` - Button released (falling edge)
- `EVENT_CLICK` - Press + release quickly
- `EVENT_HOLD` - Held for N milliseconds
- `EVENT_DOUBLE_CLICK` - Two clicks within 300ms
- `EVENT_LONG_PRESS` - Held for 2+ seconds

---

### 5. Icon Library System

Users can define bitmap icons for menus, screens, and dashboards.

#### Usage:
```cpp
// Define 16x16 icon
REGISTER_ICON(ICON_DRONE, 16, 16, {
    0b00000001, 0b10000000,
    0b00000111, 0b11100000,
    0b00011111, 0b11111000,
    0b01111111, 0b11111110,
    0b11111111, 0b11111111,
    // ... 11 more rows
});

// Define 8x8 icon
REGISTER_ICON(ICON_BATTERY, 8, 8, {
    0b00111110,
    0b01111111,
    0b11111111,
    0b11111111,
    0b11111111,
    0b11111111,
    0b01111110,
    0b00111100
});

// Use in code
canvas.drawIcon(0, 0, ICON_DRONE);
canvas.drawIcon(110, 0, ICON_BATTERY);
```

#### Built-in Icons:
- `ICON_HOME` - Home/dashboard
- `ICON_SETTINGS` - Gear
- `ICON_INFO` - Information
- `ICON_WARNING` - Warning triangle
- `ICON_ERROR` - Error X
- `ICON_BATTERY_FULL` - Battery 100%
- `ICON_BATTERY_MED` - Battery 50%
- `ICON_BATTERY_LOW` - Battery 20%
- `ICON_SIGNAL_FULL` - WiFi/signal full
- `ICON_SIGNAL_MED` - Signal medium
- `ICON_SIGNAL_LOW` - Signal low
- `ICON_SIGNAL_NONE` - No signal
- `ICON_JOYSTICK` - Joystick symbol
- `ICON_DRONE` - Quadcopter
- `ICON_ROBOT` - Robot
- `ICON_CAR` - RC car
- `ICON_TUNING` - Wrench/tuning
- `ICON_LOCK` - Lock (disarmed)
- `ICON_UNLOCK` - Unlock (armed)
- `ICON_PLAY` - Play
- `ICON_PAUSE` - Pause
- `ICON_STOP` - Stop
- `ICON_UP` - Arrow up
- `ICON_DOWN` - Arrow down
- `ICON_LEFT` - Arrow left
- `ICON_RIGHT` - Arrow right
- `ICON_CHECK` - Checkmark
- `ICON_CROSS` - X mark

---

## Refactored UI/UX Design

### Dashboard Layout (Redesigned)

```
┌────────────────────────────────┐
│ [ICON] Module Name    [BAT][SIG]│  ← Header (16px)
├────────────────────────────────┤
│                                │
│   PRIMARY VISUALIZATION        │  ← Main area (32px)
│   (Gauges, graphs, status)     │
│                                │
├────────────────────────────────┤
│ [BTN1] [BTN2] [BTN3]    TX:123 │  ← Footer (16px)
└────────────────────────────────┘
```

**Features:**
- **Icon-based header** - Module icon + name + status icons
- **Adaptive middle** - Changes based on module needs
- **Button labels** - Show current function assignments
- **Telemetry counter** - TX/RX packet counts

### Menu System (Redesigned)

```
┌────────────────────────────────┐
│ [<] Settings               [i] │  ← Breadcrumb + info
├────────────────────────────────┤
│ ▶ [ICON] Display               │  ← Submenu indicator
│   [ICON] Audio             ✓   │  ← Checkmark for enabled
│ ▶ [ICON] Controls              │
│ ▶ [ICON] Network               │
│   [ICON] About                 │
├────────────────────────────────┤
│         3 / 12                 │  ← Scroll indicator
└────────────────────────────────┘
```

**Features:**
- **Breadcrumb navigation** - "Home > Settings > Audio"
- **Icons** - Every entry has an icon
- **Indicators** - Checkmarks, arrows, values
- **Smooth scrolling** - Encoder-based navigation
- **Info button** - Context help (button 3)

### Module Selector (Redesigned)

```
┌────────────────────────────────┐
│ Select Module              [?] │
├────────────────────────────────┤
│ ┌──────────────────────────┐ │
│ │ [DRONE]  DroneGaze       │ │  ← Focused
│ │          Quadcopter       │ │
│ │          v2.1.0          │ │
│ └──────────────────────────┘ │
│                                │
│  [ROBOT]  TheGill              │  ← Other options
│            Diff Drive          │
│                                │
│  [CAR]    Bulky                │
│            4WD Platform        │
└────────────────────────────────┘
```

**Features:**
- **Large icons** - 32x32 module icons
- **Metadata** - Module type and version
- **Focused card** - Highlighted selection
- **Smooth transitions** - Slide animations

---

## Complete User Example

```cpp
#include <ILITE.h>

// ============================================================================
// Custom Module
// ============================================================================

class MyDrone : public ILITEModule {
public:
    const char* getModuleId() const override { return "com.mydrone"; }
    const char* getModuleName() const override { return "My Drone"; }
    const char* getVersion() const override { return "1.0.0"; }

    // ... packet definitions, control loop, etc. ...

private:
    bool armed_ = false;
    float pidP_ = 1.0f;
};

REGISTER_MODULE(MyDrone);

// ============================================================================
// Custom Audio
// ============================================================================

REGISTER_AUDIO("motor_armed", 1000, 100);
REGISTER_AUDIO("motor_disarmed", 500, 200);
REGISTER_AUDIO("emergency_stop", 250, 500);

// ============================================================================
// Custom Icons
// ============================================================================

REGISTER_ICON(ICON_MY_DRONE, 16, 16, {
    0b00000001, 0b10000000,
    0b00000111, 0b11100000,
    // ... icon data
});

// ============================================================================
// Custom Menu Entries
// ============================================================================

REGISTER_MENU_ENTRY(
    .id = "mydrone.arm",
    .parent = "quick_actions",
    .icon = ICON_UNLOCK,
    .label = "Arm Motors",
    .condition = []() { return !myDrone.armed && myDrone.isPaired(); },
    .onSelect = []() {
        myDrone.armed = true;
        Audio::play("motor_armed");
    }
);

REGISTER_MENU_ENTRY(
    .id = "mydrone.pid_tuning",
    .parent = "settings",
    .icon = ICON_TUNING,
    .label = "PID Tuning",
    .onSelect = []() { ScreenRegistry::show("pid_tuning"); }
);

// ============================================================================
// Custom Screen
// ============================================================================

REGISTER_SCREEN(
    .id = "pid_tuning",
    .title = "PID Tuning",
    .icon = ICON_TUNING,
    .drawFunc = [](DisplayCanvas& canvas) {
        canvas.drawIcon(0, 0, ICON_TUNING);
        canvas.drawText(20, 8, "PID Tuning");

        canvas.drawTextF(0, 24, "P: %.3f", myDrone.pidP);
        canvas.drawProgressBar(40, 24, 80, 6, myDrone.pidP / 10.0);
    },
    .onEncoderRotate = [](int delta) {
        myDrone.pidP += delta * 0.001;
        myDrone.pidP = constrain(myDrone.pidP, 0.0, 10.0);
    }
);

// ============================================================================
// Custom Control Bindings
// ============================================================================

REGISTER_CONTROL_BINDING(
    .input = INPUT_BUTTON1,
    .event = EVENT_PRESS,
    .action = []() {
        myDrone.armed = !myDrone.armed;
        Audio::play(myDrone.armed ? "motor_armed" : "motor_disarmed");
    }
);

REGISTER_CONTROL_BINDING(
    .input = INPUT_BUTTON2,
    .event = EVENT_HOLD,
    .duration = 1000,
    .action = []() {
        myDrone.armed = false;
        Audio::play("emergency_stop");
    }
);

// ============================================================================
// Main
// ============================================================================

void setup() {
    ILITE.begin();  // That's it! Framework handles everything
}

void loop() {
    ILITE.update();
}
```

**User never has to:**
- Configure WiFi or ESP-NOW
- Initialize display or hardware
- Poll inputs or handle debouncing
- Manage menus or screens
- Handle audio playback
- Route packets or manage connections

**Everything is declarative and registration-based!**
