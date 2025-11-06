# ILITE Home Screen & Navigation System

## Overview

The ILITE framework now includes a comprehensive home screen and navigation system that provides a familiar, menu-driven interface when no device is paired, while seamlessly integrating with the modular platform system.

---

## Navigation Flow

```
┌─────────────────────────────────────────┐
│          HOME SCREEN (Unpaired)         │
│  ┌───────────────────────────────────┐  │
│  │ ILITE v2.0                        │  │
│  │ Controller Ready                  │  │
│  │ 4 modules registered              │  │
│  │ No device paired                  │  │
│  │                                   │  │
│  │ Press encoder for menu            │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
               │ Encoder Button
               ▼
┌─────────────────────────────────────────┐
│            MAIN MENU                    │
│  ┌───────────────────────────────────┐  │
│  │ ▶ Terminal                        │  │
│  │   Modules                         │  │
│  │   Settings                        │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
      │           │            │
      │           │            │ Select
      ▼           ▼            ▼
  Terminal   Module Browser  Settings
      │           │
      │           │ Select Module
      │           ▼
      │    ┌──────────────────┐
      │    │  MODULE DASHBOARD │
      │    │  (Unpaired view)  │
      │    └──────────────────┘
      │
      ▼
  ┌──────────────────┐
  │    TERMINAL      │
  │ > System Ready   │
  │ > Modules: OK    │
  │ > ESP-NOW: Active│
  └──────────────────┘
```

---

## Components

### 1. Home Screen (`HomeScreen.h/cpp`)

The home screen is displayed when no device is paired. It shows:
- **ILITE version** (v2.0)
- **System status** ("Controller Ready")
- **Module count** (e.g., "4 modules registered")
- **Connection status** ("No device paired")
- **Battery indicator** (top right)
- **Signal strength** (top right)
- **Help text** ("Press encoder for menu")

**Controls**:
- **Encoder button**: Opens main menu
- **Button 3**: (No action on home screen)

### 2. Main Menu

A vertical menu with 3 options:
1. **Terminal** - View system logs and status
2. **Modules** - Browse registered modules (opens Module Browser)
3. **Settings** - View system settings

**Controls**:
- **Encoder rotate**: Navigate up/down
- **Encoder button**: Select item
- **Button 3**: Back to home screen

### 3. Terminal View

Displays recent system log messages:
```
Terminal
──────────────────
> System Ready
> Modules: OK
> ESP-NOW: Active
> Waiting for pair
```

**Controls**:
- **Encoder button**: Back to menu
- **Button 3**: Back to menu

### 4. Module Browser (`ModuleBrowser.h/cpp`)

Card-based UI showing all registered modules:

```
┌────────────────────────────────────┐
│ Select Module                      │
├────────────────────────────────────┤
│  ┌────────┐      ┌────────┐        │
│  │ [LOGO] │      │ [LOGO] │        │
│  │        │      │        │        │
│  │ DroneG │      │ Bulky  │        │
│  └────────┘      └────────┘        │
│                               1/4  │
└────────────────────────────────────┘
```

Shows 2 cards per row, scrollable with encoder.

**Controls**:
- **Encoder rotate**: Navigate between modules
- **Encoder button**: Select module (view dashboard)
- **Button 3**: Back to main menu

### 5. Module Dashboard (Unpaired View)

When a module is selected from the browser (even though not paired), it displays the module's dashboard. This lets users:
- Preview what the dashboard looks like
- Test dashboard functionality
- Familiarize themselves with the module

**Controls**:
- **Button 3**: Back to module browser
- **Module-specific controls** (if applicable)

### 6. Settings View

Displays current system configuration:
```
Settings
──────────────────
Control Loop: 50Hz
Display: 10Hz
Audio: Enabled
```

**Controls**:
- **Encoder button**: Back to menu
- **Button 3**: Back to menu

---

## Display Modes

Managed by `DisplayMode` enum in `HomeScreen.h`:

```cpp
enum class DisplayMode : uint8_t {
    HOME,              // Home screen (unpaired state)
    MENU,              // Main menu
    TERMINAL,          // Terminal view
    MODULE_BROWSER,    // Module card browser
    MODULE_DASHBOARD,  // Active module dashboard
    SETTINGS          // Settings screen
};
```

---

## Platform Logos

### Logo Format
- **Size**: 32x32 pixels (XBM format)
- **Color**: Monochrome (1-bit)
- **Location**: `ModuleBrowser.cpp` (PlatformLogos namespace)

### Available Logos

1. **DroneGaze Logo** (`drongaze_32x32[]`)
   - Quadcopter with 4 propellers
   - Center body with rotors
   - Recognizable drone silhouette

2. **Bulky Logo** (`bulky_32x32[]`)
   - Utility vehicle/truck
   - Box-shaped body
   - Wheels at bottom
   - Carrier robot design

3. **TheGill Logo** (`thegill_32x32[]`)
   - Differential drive robot
   - Rectangular body
   - Side wheels
   - Tank-style design

4. **Generic Logo** (`generic_32x32[]`)
   - Generic robot icon
   - Humanoid head and body
   - Used for unknown modules
   - Fallback design

### Logo Selection Logic

The Module Browser automatically selects logos based on module name:

```cpp
if (strstr(moduleName, "Drone") || strstr(moduleName, "Gaze")) {
    logo = PlatformLogos::drongaze_32x32;
} else if (strstr(moduleName, "Bulky")) {
    logo = PlatformLogos::bulky_32x32;
} else if (strstr(moduleName, "Gill")) {
    logo = PlatformLogos::thegill_32x32;
} else {
    logo = PlatformLogos::generic_32x32;
}
```

---

## Integration with ILITE Framework

### Display Task (`ILITE.cpp:displayTask()`)

The display task routes rendering based on pairing state:

```cpp
if (ScreenRegistry::hasActiveScreen()) {
    // Extension system screen takes priority
    ScreenRegistry::drawActiveScreen(canvas);
} else if (!paired_) {
    // Not paired - use HomeScreen system
    HomeScreen::draw(canvas);
} else if (module != nullptr && paired_) {
    // Paired - use module dashboard
    module->drawDashboard(canvas);
}
```

### Input Handling (`ILITE.cpp:handleHomeScreenInput()`)

When not paired, input is routed to the HomeScreen:

```cpp
if (!paired_) {
    handleHomeScreenInput();
}
```

This polls:
- **Encoder rotation**: `HomeScreen::onEncoderRotate(delta)`
- **Encoder button**: `HomeScreen::onEncoderButton()`
- **Button 3 (back)**: `HomeScreen::onButton(3)`

---

## Adding Custom Platform Logos

### Step 1: Create Logo Bitmap

Use an image editor or online XBM generator:
1. Create 32x32 monochrome image
2. Export as XBM format
3. Get byte array data

### Step 2: Add to `ModuleBrowser.cpp`

```cpp
namespace PlatformLogos {
    const uint8_t myrobot_32x32[] = {
        0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18,
        // ... (128 bytes total for 32x32)
    };
}
```

### Step 3: Update Logo Selection

In `ModuleBrowser::drawCard()`:

```cpp
if (strstr(moduleName, "MyRobot")) {
    logo = PlatformLogos::myrobot_32x32;
}
```

---

## Behavior Summary

### When NOT Paired (Unpaired State)
- ✅ Home screen shows status and module count
- ✅ Encoder button opens main menu
- ✅ Can browse modules via Module Browser
- ✅ Can view module dashboards (preview mode)
- ✅ Can view terminal logs
- ✅ Can view settings

### When Paired
- ✅ Automatically switches to paired module dashboard
- ✅ Full module functionality enabled
- ✅ Module receives telemetry from robot
- ✅ Controls send commands to robot
- ✅ Button 3 can still access menus (if module allows)

### Pairing Transition
```
Unpaired (Home Screen) → Pair Event → Paired (Module Dashboard)
                ↑                              ↓
                └──────────  Unpair Event  ────┘
```

When transitioning:
- **On Pair**: Home screen → Module dashboard
- **On Unpair**: Module dashboard → Home screen

---

## Files Created/Modified

### Created Files
1. ✅ `lib/ILITE/include/HomeScreen.h` - Home screen interface
2. ✅ `lib/ILITE/src/HomeScreen.cpp` - Home screen implementation
3. ✅ `lib/ILITE/include/ModuleBrowser.h` - Module browser interface
4. ✅ `lib/ILITE/src/ModuleBrowser.cpp` - Module browser + platform logos

### Modified Files
5. ✅ `lib/ILITE/src/ILITE.cpp` - Integrated home screen system
   - Added HomeScreen and ModuleBrowser includes
   - Added initialization in `begin()`
   - Updated `displayTask()` to route to home screen when not paired
   - Added `handleHomeScreenInput()` method
   - Route input to home screen in `update()`

---

## Usage Example

### User Experience

1. **Power On**:
   ```
   ILITE v2.0
   Controller Ready
   4 modules registered
   No device paired

   Press encoder for menu
   ```

2. **Press Encoder**:
   ```
   Main Menu
   ──────────
   ▶ Terminal
     Modules
     Settings
   ```

3. **Select "Modules"**:
   ```
   Select Module
   ──────────────
   [DRONE]  [BULKY]
   Drongaze  Bulky

                1/4
   ```

4. **Select "Drongaze"**:
   ```
   [Drongaze Dashboard]
   (Preview mode - not paired)
   ```

5. **Power On Drone** → Auto-pairs → Dashboard becomes live

---

## Benefits

✅ **Familiar Interface** - Menu-driven, like classic embedded systems
✅ **Module Discovery** - Easy to see what modules are available
✅ **Visual Recognition** - Platform logos help identify modules
✅ **Preview Mode** - View dashboards before pairing
✅ **System Status** - Terminal shows what's happening
✅ **Backward Compatible** - Existing modules work unchanged
✅ **Extensible** - Easy to add new screens and menus

---

## Next Steps

### Suggested Enhancements

1. **Custom Logos** - Replace placeholder logos with actual platform logos
2. **Battery Monitoring** - Real battery percentage in status bar
3. **Signal Strength** - Real ESP-NOW RSSI in status bar
4. **Terminal Scrolling** - Scrollable log buffer
5. **Settings Editor** - Ability to change settings from UI
6. **Module Info Cards** - Show module version, author, etc.
7. **Recent Modules** - Remember last used module

---

## Summary

✅ **Complete home screen system implemented**
✅ **Menu navigation with Terminal, Modules, Settings**
✅ **Module browser with card-based UI**
✅ **Platform logos for DroneGaze, Bulky, TheGill, Generic**
✅ **Seamless integration with ILITE framework**
✅ **Unpaired → Paired transition handled automatically**
✅ **Backward compatible with existing modules**

**The ILITE framework now has a professional, user-friendly interface that wraps the powerful modular architecture!** 🎉
