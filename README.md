# ILITE Framework 🎮

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-blue.svg)](https://platformio.org/)
[![ESP32](https://img.shields.io/badge/ESP32-Compatible-green.svg)](https://www.espressif.com/en/products/socs/esp32)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**ILITE** (Integrated Lightweight Interactive Telemetry Environment) is a powerful, extensible ESP32-based robotics controller framework for ASCE competition robots and custom projects.

Transformed from a hardcoded controller into a modular framework, ILITE enables rapid development of wireless robotics controllers with hot-swappable modules, modern OLED UI, and ESP-NOW communication.

---

## ✨ Features

### 🎯 Core Framework
- **Modular Architecture** - Register custom modules without modifying core code
- **ESP-NOW Communication** - 2.4GHz wireless protocol with auto-discovery and pairing
- **OLED Display** - Modern UI with 128x64 SH1106 display (U8G2 library)
- **Dual Joystick Support** - Two analog joysticks for precise control
- **Rotary Encoder** - Menu navigation with push button
- **Audio Feedback** - Buzzer-based sound cues and alerts
- **OTA Updates** - Over-the-air firmware updates via WiFi

### 🔌 Extension Systems
- **Audio Registry** - Register custom sound effects and alerts
- **Icon Library** - 30+ built-in icons (8x8, 16x16, 32x32) + custom icon support
- **UI Components** - Modern widgets: headers, footers, cards, modals, menus
- **Menu Registry** - Hierarchical menus with conditional visibility
- **Screen Registry** - Custom display screens with navigation stack
- **Control Binding System** - Flexible input events (click, hold, double-click, long-press)

### 🤖 Built-in Modules
- **TheGill** - Differential drive robot with PID tuning
- **Drongaze** - Quadcopter flight controller
- **Generic Module** - Fallback for unknown devices

---

## 🚀 Quick Start

### Hardware Requirements
- **ESP32 NodeMCU-32S** (or compatible)
- **SH1106 128x64 OLED** (I2C)
- **2x Analog Joysticks** (X/Y axes + button)
- **Rotary Encoder** with push button
- **4x Push Buttons**
- **Passive Buzzer** (audio feedback)

### Software Requirements
- [PlatformIO](https://platformio.org/) (VSCode extension recommended)
- Arduino framework for ESP32
- Libraries (auto-installed by PlatformIO):
  - U8g2 (OLED driver)
  - ArduinoJson (data serialization)

### Installation

1. **Clone the repository**:
   ```bash
   git clone https://github.com/yourusername/ILITE.git
   cd ILITE
   ```

2. **Open in PlatformIO**:
   - Open the project folder in VSCode with PlatformIO extension
   - PlatformIO will auto-install dependencies

3. **Build and upload**:
   ```bash
   pio run -t upload
   ```

4. **Monitor serial output**:
   ```bash
   pio device monitor
   ```

---

## 📝 Usage

### Minimal Example

```cpp
#include <Arduino.h>
#include <ILITE.h>

void setup() {
    Serial.begin(115200);

    ILITEConfig config;
    config.controlLoopHz = 50;        // 50Hz control loop
    config.displayRefreshHz = 10;      // 10Hz display refresh
    config.enableAudio = true;         // Enable audio feedback
    config.enableOTA = true;           // Enable OTA updates

    if (!ILITE.begin(config)) {
        Serial.println("Framework initialization failed!");
        while (1) delay(1000);
    }

    Serial.println("ILITE Framework Ready!");
}

void loop() {
    ILITE.update();
}
```

### Creating a Custom Module

```cpp
#include <ILITEModule.h>

class MyRobot : public ILITEModule {
public:
    const char* getName() const override { return "MyRobot"; }
    const char* getDisplayName() const override { return "My Robot"; }

    void onPair(const ModuleState* state) override {
        Serial.println("Robot paired!");
    }

    void onUnpair() override {
        Serial.println("Robot unpaired!");
    }

    void updateControl(int16_t joyLX, int16_t joyLY,
                      int16_t joyRX, int16_t joyRY) override {
        // Implement your control logic
        packet_.data[0] = map(joyLY, -32768, 32767, 0, 255);
        packet_.data[1] = map(joyRX, -32768, 32767, 0, 255);
        sendPacket();
    }

    void drawDashboard(DisplayCanvas& canvas) override {
        canvas.setFont(u8g2_font_6x10_tr);
        canvas.drawStr(0, 10, "My Robot Dashboard");
        canvas.drawStr(0, 25, "Speed: 100%");
    }
};

REGISTER_MODULE(MyRobot);
```

### Custom Audio Cues

```cpp
void setup() {
    // Register in setup()
    AudioRegistry::registerCue({"startup", 1000, 200});
    AudioRegistry::registerCue({"error", 400, 300});

    ILITE.begin(config);

    // Play sounds
    AudioRegistry::play("startup");
}
```

### Custom Menu Entries

```cpp
void setup() {
    // Register menu entry in setup()
    MenuRegistry::registerEntry({
        .id = "my_action",
        .parentId = "quick_actions",
        .icon = ICON_PLAY,
        .displayName = "My Action",
        .onSelect = []() {
            Serial.println("Action triggered!");
        }
    });

    ILITE.begin(config);
}
```

### Custom Control Bindings

```cpp
void setup() {
    // Register control binding in setup()
    ControlBindingSystem::registerBinding({
        .input = INPUT_BUTTON3,
        .event = EVENT_CLICK,
        .action = []() {
            Serial.println("Button 3 clicked!");
        }
    });

    ILITE.begin(config);
}
```

---

## 🏗️ Architecture

### Library Structure

```
ILITE/
├── lib/ILITE/              # Framework Library
│   ├── src/                # Implementation files
│   ├── include/            # Public headers
│   └── library.json        # PlatformIO manifest
│
├── src/
│   └── main.cpp            # User application
│
├── examples/
│   └── FrameworkDemo/      # Complete demo
│
└── docs/                   # Documentation
```

### Key Components

- **ILITE Core** - Main framework class, initialization, task management
- **ILITEModule** - Base class for all robot modules
- **ModuleRegistry** - Module registration and discovery
- **InputManager** - Joystick and button abstraction
- **DisplayCanvas** - U8G2 wrapper with extended drawing functions
- **PacketRouter** - ESP-NOW packet routing and handling

### Extension Systems

- **AudioRegistry** - Custom audio cue management
- **IconLibrary** - Icon storage and rendering
- **UIComponents** - Reusable UI widgets
- **MenuRegistry** - Hierarchical menu system
- **ScreenRegistry** - Custom screen management
- **ControlBindingSystem** - Input event handling

---

## 📊 Code Metrics

| Metric | Before Framework | After Framework | Improvement |
|--------|------------------|-----------------|-------------|
| **main.cpp Lines** | 1,712 | 136 | **-92%** |
| **Includes** | 50+ | 1 | **-98%** |
| **Boilerplate** | ~1,500 lines | ~50 lines | **-97%** |
| **Extensibility** | Hardcoded | Modular | **∞%** |

---

## 🎮 Hardware Pinout

### ESP32 NodeMCU-32S Pin Configuration

| Component | Pin | Description |
|-----------|-----|-------------|
| OLED SDA | GPIO 21 | I2C Data |
| OLED SCL | GPIO 22 | I2C Clock |
| Buzzer | GPIO 26 | PWM Audio |
| Joystick L X | GPIO 34 | Analog Input |
| Joystick L Y | GPIO 35 | Analog Input |
| Joystick R X | GPIO 32 | Analog Input |
| Joystick R Y | GPIO 33 | Analog Input |
| Encoder A | GPIO 25 | Rotary Input |
| Encoder B | GPIO 27 | Rotary Input |
| Encoder SW | GPIO 13 | Push Button |
| Button 1 | GPIO 14 | Digital Input |
| Button 2 | GPIO 12 | Digital Input |
| Button 3 | GPIO 15 | Digital Input |
| Button 4 | GPIO 4 | Digital Input |

---

## 📚 Documentation

- **[FRAMEWORK_COMPLETE.md](FRAMEWORK_COMPLETE.md)** - Complete transformation guide
- **[FRAMEWORK_EXTENSION_ARCHITECTURE.md](FRAMEWORK_EXTENSION_ARCHITECTURE.md)** - Architecture details
- **[LIBRARY_STRUCTURE.md](LIBRARY_STRUCTURE.md)** - Library organization
- **[BACKWARD_COMPATIBILITY.md](BACKWARD_COMPATIBILITY.md)** - Compatibility guarantees
- **[examples/FrameworkDemo/](examples/FrameworkDemo/)** - Complete usage example

---

## 🤝 Contributing

Contributions are welcome! Please follow these guidelines:

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature/amazing-feature`
3. **Commit your changes**: `git commit -m 'Add amazing feature'`
4. **Push to the branch**: `git push origin feature/amazing-feature`
5. **Open a Pull Request**

### Development Guidelines
- Follow existing code style (C++17, Arduino framework)
- Add comments for complex logic
- Update documentation for new features
- Test on real hardware before submitting

---

## 🐛 Known Issues

- OTA updates require WiFi credentials in `main.cpp`
- Display refresh rate limited to ~10Hz due to U8G2 library
- ESP-NOW range limited to ~200m line-of-sight

---

## 🔮 Roadmap

- [ ] Web-based configuration interface
- [ ] Multiple robot support (fleet control)
- [ ] Data logging to SD card
- [ ] Bluetooth Classic/BLE support
- [ ] IMU integration for tilt compensation
- [ ] Battery voltage monitoring
- [ ] Touch screen support

---

## 📜 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

---

## 👨‍💻 Author

**Assil M. Ferahta**
ASCE Robotics Team
2025

---

## 🙏 Acknowledgments

- **U8g2 Library** - Excellent OLED display driver
- **PlatformIO** - Modern embedded development platform
- **ESP32 Community** - Amazing documentation and support
- **ASCE Team** - Testing and feedback

---

## 📞 Contact

- **GitHub Issues**: [Report bugs or request features](https://github.com/yourusername/ILITE/issues)
- **Email**: eAssilFer@gmail.com

---

## ⭐ Star This Project

If you find ILITE useful, please consider giving it a star on GitHub! It helps others discover the project.

[![GitHub stars](https://img.shields.io/github/stars/yourusername/ILITE.svg?style=social)](https://github.com/yourusername/ILITE/stargazers)

---

**Built with ❤️ for robotics enthusiasts**
