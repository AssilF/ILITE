@file LIBRARY_STRUCTURE.md
@brief ILITE Framework layout and upgrade notes (Doxygen-flavoured reference).

@section layout Directory Layout
```
ILITE/
├─ lib/ILITE/                       # Reusable framework library
│  ├─ include/                      # Public headers
│  │  ├─ ILITE.h, ILITEModule.h     # Core runtime + module contract
│  │  ├─ ControlBindingSystem.h     # Input binding engine
│  │  ├─ AudioRegistry.h, AudioCues.h
│  │  ├─ espnow_discovery.h         # Discovery/identity layer
│  │  └─ …                          # Icons, menus, screens, helpers
│  ├─ src/                          # Framework implementation
│  │  ├─ ILITE.cpp                  # Boot, tasks, lifecycle
│  │  ├─ FrameworkEngine.cpp        # UI shell + encoder/menu routing
│  │  ├─ ControlBindingSystem.cpp   # Binding dispatcher
│  │  ├─ AudioCues.cpp              # Built-in cue definitions
│  │  ├─ espnow_discovery.cpp       # ESP-NOW pairing backend
│  │  └─ ModuleRegistration.cpp     # Platform modules (TheGill, etc.)
│  └─ library.json                  # PlatformIO manifest
├─ src/main.cpp                     # User entry point (register sounds, start ILITE)
├─ include/                         # User headers (if any)
├─ docs/                            # Long-form guides
└─ *.md                             # Status updates & migration notes
```

@section runtime Runtime Flow
1. **setup()**
   - Register any custom audio cues before `ILITE.begin()`.
   - Configure `ILITEConfig` (loop rates, Wi-Fi, OTA, audio enable).
   - Call `ILITE.begin(config)` → hardware init, audio boot melody, FrameworkEngine bring-up.
2. **Module Discovery**
   - `ModuleRegistry` holds every `REGISTER_MODULE(X)` class.
   - `EspNowDiscovery` tracks peers and exposes metadata (`getPeerName`, `getPeerIdentity`).
3. **Event Loop**
   - `ILITE.update()` handles OTA, discovery, pairing state, connection timeout.
   - `ControlBindingSystem::update()` is invoked internally every loop.
   - `FrameworkEngine` renders dashboards, screens, and encoder strip functions.
4. **Tasks**
   - `CommTask` (Core 0) → reads inputs, runs `module->updateControl()`, sends ESP-NOW packets.
   - `DisplayTask` (Core 1) → renders module dashboards or DefaultActions screens.

@section upgrades Recent Upgrades

@subsection audio_cues Built-in Audio Cues Always Live
- Added `AudioCues.h` + `ensureDefaultAudioCuesRegistered()` (called from `ILITE.cpp`).
- Forces `AudioCues.cpp` to stay linked so every cue (`menu_select`, `toggle`, `edit_*`, etc.) is registered even if the user does not define duplicates in `main.cpp`.
- Result: any `AudioRegistry::play("name")` now maps to an actual buzzer pattern without silent fallbacks.

@subsection module_scope Module Activation Loads Controls Reliably
- `ControlBindingSystem` now tracks a `moduleOwned` flag per binding.
- `FrameworkEngine::loadModule()` clears module-owned bindings, wraps `module->onActivate()` with `beginModuleCapture()/endModuleCapture()`, and only registers new bindings for the active module.
- Global bindings registered by user code (e.g., emergency stop in `setup()`) are preserved.
- Encoder strip buttons are also wiped before activation; modules re-apply theirs once.
- `ILITEFramework::setActiveModule()` no longer double-calls `onActivate()`, preventing binding duplication and ensuring encoder labels reflect the selected module immediately.

@subsection thegill_controls TheGill Control Enhancements
- Orientation mode now responds to all three axes: joystick A steers yaw/pitch while joystick B drives wrist roll, with the potentiometer scaling speed and a precision toggle on Joystick B button.
- Gripper management exposes macros (shift + BTN1 for individual fingers, safety snaps, tool presets) and reuses the new Left/Shift/Right button aliases for clarity.
- The camera menu adds Operator Left/Right and Tool perspectives to better judge depth on the OLED preview.
- Button events reach modules even when unpaired, so developers can exercise control schemes before connecting to hardware.

@subsection device_ui Device Detail + Pair/Cancel Screen
- `DefaultActions` adds a new `DEVICE_INFO` screen reached by pressing the encoder while browsing devices.
- The page shows `Identity` fields (name, type, platform, MAC, status) using `EspNowDiscovery::getPeerIdentity()`.
- Encoder rotates between **PAIR** and **CANCEL** buttons:
  - `PAIR` triggers `EspNowDiscovery::beginPairingWith(mac)` and returns to the list with feedback sounds.
  - `CANCEL` (or pressing when the device disappeared) drops back to the list with a back-tone.
- Gives operators a confirmation step plus device context before pairing.

@section modules Writing Modules
@subsection essentials Minimal Skeleton
```cpp
class MyRobot final : public ILITEModule {
public:
    const char* getModuleId() const override { return "com.example.myrobot"; }
    const char* getModuleName() const override { return "My Robot"; }
    const char* getVersion() const override { return "1.0.0"; }

    size_t getCommandPacketTypeCount() const override { return 1; }
    PacketDescriptor getCommandPacketDescriptor(size_t) const override {
        return {"Command", 0x12345678, sizeof(Command), sizeof(Command), true, nullptr, 0};
    }

    void onActivate() override {
        FrameworkEngine& fw = FrameworkEngine::getInstance();
        EncoderFunction f1{};
        f1.label = "ARM";
        f1.callback = []() { AudioRegistry::play("toggle"); };
        fw.setEncoderFunction(0, f1);

        ControlBinding binding{};
        binding.input = INPUT_BUTTON1;
        binding.event = EVENT_CLICK;
        binding.action = []() { AudioRegistry::play("menu_select"); };
        ControlBindingSystem::registerBinding(binding); // auto-marked as module-owned
    }
};
REGISTER_MODULE(MyRobot);
```

@subsection menus Module Menu Builder
Implement `buildModuleMenu(ModuleMenuBuilder& builder)` to expose per-module settings. `FrameworkEngine` clears the previous tree on each activation, so modules can rebuild confidently.

@section ui Default Actions & Screens
- **Terminal**: scrolls through `connection_log`.
- **Devices**: now flows `Device List → Device Info (PAIR/CANCEL)`.
- **Settings**: placeholder entries for display/audio/system toggles.
- Encoder press closes Terminal, opens Device Info, or navigates Settings depending on active screen.

@section pairing Pairing Workflow
1. Discovery populates peers (30s TTL).
2. User opens Devices screen, selects a device, inspects info, then presses **PAIR**.
3. `EspNowDiscovery::beginPairingWith()` sends `MSG_PAIR_CONFIRM`, waits for ACK, and `ILITEFramework::handlePairing()` activates the matching module (auto-matching by `customId` if available).

@section audio Registering Custom Cues
```cpp
AudioRegistry::registerCue({"startup", 1000, 200});
AudioRegistry::registerCue({"paired", 1200, 150});
AudioRegistry::registerCue({"menu_select", 640, 50});
// Built-ins (menu_back, toggle, edit_start, error, etc.) come for free now.
```

@section tips Quick Tips
- Call `AudioRegistry::registerCue` before `ILITE.begin()` so static tasks can use them immediately.
- Global control bindings should be registered from `setup()`; module-scoped ones belong in `onActivate()`.
- `EspNowDiscovery::getPeerIdentity()` is safe even when devices time out; always guard against `nullptr`.
- Use `FrameworkEngine::getInstance().setEncoderFunction(slot, func)` to customize the strip, and clear them in `onDeactivate()` if you perform additional teardown.
