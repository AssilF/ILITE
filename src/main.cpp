/**
 * @file main.cpp
 * @brief ILITE Universal Controller - Framework-based entry point
 *
 * ESP32-based modular controller for ASCE robotics platforms.
 * Supports wireless ESP-NOW communication, OLED display, dual joysticks,
 * and hot-swappable control modules.
 *
 * This version uses the ILITE framework library for extensibility.
 * Existing modules (TheGill, Drongaze, Generic) are auto-registered.
 *
 * @author Assil M. Ferahta
 * @date 2025
 * @version 2.0.0 (Framework-based)
 */

#include <Arduino.h>
#include <ILITE.h>
#include <AudioRegistry.h>
#include <ControlBindingSystem.h>
#include <MenuRegistry.h>
#include <UIComponents.h>
#include <audio_feedback.h>   // For audioUpdate() to stop tones


// ============================================================================
// Configuration
// ============================================================================

// WiFi credentials for OTA updates (optional)
const char* WIFI_SSID = "ILITE PORT";
const char* WIFI_PASSWORD = "ASCE321#";

// Debug macros
#define VERBOSE_EN 1
#if VERBOSE_EN == 1
    #define debug(x) Serial.print(x);
    #define debugln(x) Serial.println(x);
#else
    #define debug(x)
    #define debugln(x)
#endif

// ============================================================================
// Setup
class MyRobot : public ILITEModule {
public:
    // Required metadata
    const char* getModuleId() const override { return "com.myrobot"; }
    const char* getModuleName() const override { return "My Robot"; }
    const char* getVersion() const override { return "1.0.0"; }

    // Packet definitions
    size_t getCommandPacketTypeCount() const override { return 1; }
    PacketDescriptor getCommandPacketDescriptor(size_t index) const override {
        return {"Command", 0x12345678, sizeof(Command), sizeof(Command), true, nullptr, 0};
    }

    size_t getTelemetryPacketTypeCount() const override { return 1; }
    PacketDescriptor getTelemetryPacketDescriptor(size_t index) const override {
        return {"Telemetry", 0x87654321, sizeof(Telemetry), sizeof(Telemetry), true, nullptr, 0};
    }

    // Control loop (50Hz)
    void updateControl(const InputManager& inputs, float dt) override {
        command_.throttle = inputs.getJoystickA_Y();
        command_.steering = inputs.getJoystickB_X();
    }

    size_t prepareCommandPacket(size_t index, uint8_t* buffer, size_t size) override {
        memcpy(buffer, &command_, sizeof(command_));
        return sizeof(command_);
    }

    void handleTelemetry(size_t index, const uint8_t* data, size_t length) override {
        memcpy(&telemetry_, data, sizeof(telemetry_));
    }

    // Display (10Hz)
    void drawDashboard(DisplayCanvas& canvas) override {
        UIComponents::drawModernHeader(canvas, "My Robot", ICON_ROBOT, telemetry_.battery, 4);
        canvas.drawProgressBar(10, 20, 100, 8, telemetry_.battery / 100.0f, "Battery");
    }

private:
    struct Command {
        uint32_t magic = 0x12345678;
        float throttle;
        float steering;
    } __attribute__((packed));

    struct Telemetry {
        uint32_t magic = 0x87654321;
        uint8_t battery;
    } __attribute__((packed));

    Command command_;
    Telemetry telemetry_;
};

// Register the module
REGISTER_MODULE(MyRobot);

// ============================================================================

void setup() {
    // Initialize serial
    Serial.begin(115200);
    delay(1000);

    Serial.println("==========================================");
    Serial.println("  ILITE Universal Controller v2.0");
    Serial.println("  Framework-based Architecture");
    Serial.println("==========================================\n");



    
    // Register custom audio cues (before framework init)
    AudioRegistry::registerCue({"startup", 1000, 200});
    AudioRegistry::registerCue({"paired", 1200, 150});
    AudioRegistry::registerCue({"unpaired", 800, 100});
    AudioRegistry::registerCue({"menu_select", 600, 50});
    AudioRegistry::registerCue({"error", 400, 300});

    // Configure ILITE framework
    ILITEConfig config;
    config.controlLoopHz = 50;           // 50Hz control loop
    config.displayRefreshHz = 20;        // 10Hz display refresh
    config.enableAudio = true;           // Enable audio feedback
    config.enableOTA = true;             // Enable OTA updates
    config.wifiSSID = WIFI_SSID;         // WiFi SSID for OTA
    config.wifiPassword = WIFI_PASSWORD; // WiFi password for OTA

    // Initialize framework
    Serial.println("[Setup] Initializing ILITE framework...");
    if (!ILITE.begin(config)) {
        Serial.println("[ERROR] Framework initialization failed!");
        Serial.println("System halted. Please check hardware connections.");
        while (1) {
            delay(1000);
        }
    }

    // Register custom control bindings (after framework init)
    // Example: Play sound on button 3 press
    ControlBinding binding1;
    binding1.input = INPUT_BUTTON2;
    binding1.event = EVENT_CLICK;
    binding1.action = []() {
        AudioRegistry::play("startup");
    };
    binding1.condition = nullptr;
    binding1.screenId = nullptr;
    binding1.duration = 0;
    binding1.priority = 0;
    ControlBindingSystem::registerBinding(binding1);

    // Example: Emergency stop on long press (2+ seconds) of button 1
    ControlBinding binding2;
    binding2.input = INPUT_BUTTON2;
    binding2.event = EVENT_LONG_PRESS;
    binding2.action = []() {
        AudioRegistry::play("error");
        Serial.println("[EMERGENCY STOP] Button 1 held for 2+ seconds");
    };
    binding2.condition = nullptr;
    binding2.screenId = nullptr;
    binding2.duration = 0;
    binding2.priority = 100;  // High priority
    ControlBindingSystem::registerBinding(binding2);

    Serial.println("\n[Setup] Framework initialized successfully!");
    Serial.println("\n==========================================");
    Serial.println("  System Ready");
    Serial.println("==========================================");
    Serial.println("\nRegistered Modules:");
    Serial.printf("  - %d modules available\n", ModuleRegistry::getModuleCount());
    Serial.println("\nFeatures:");
    Serial.println("  - Auto-discovery via ESP-NOW");
    Serial.println("  - Hot-swappable control modules");
    Serial.println("  - Extensible menu system");
    Serial.println("  - Custom audio feedback");
    Serial.println("  - Modern OLED UI");
    Serial.println("  - OTA firmware updates");
    Serial.println("\nWaiting for module pairing...\n");
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    // Update framework (handles all tasks, communication, display, input)
    ILITE.update();

    // Update audio system to stop tones properly
    audioUpdate();

    // Optional: Add custom logic here if needed
    // All module-specific logic should be in the module classes
}
