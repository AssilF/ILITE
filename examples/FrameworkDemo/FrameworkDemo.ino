/**
 * @file FrameworkDemo.ino
 * @brief Complete ILITE Framework Extension System Demo
 *
 * Demonstrates all framework extension capabilities:
 * - Custom module registration
 * - Custom audio cues
 * - Custom icons
 * - Custom menu entries with conditions
 * - Custom screens with event handlers
 * - Custom control bindings (all event types)
 * - Modern UI components
 *
 * @author ILITE Framework
 * @version 1.0.0
 */

#include <ILITE.h>

// ============================================================================
// CUSTOM MODULE DEFINITION
// ============================================================================

class DemoModule : public ILITEModule {
public:
    // ========================================================================
    // Required Metadata
    // ========================================================================

    const char* getModuleId() const override { return "com.example.demo"; }
    const char* getModuleName() const override { return "Demo Module"; }
    const char* getVersion() const override { return "1.0.0"; }

    const char** getDetectionKeywords() const override {
        static const char* keywords[] = {"demo", "example"};
        return keywords;
    }

    size_t getDetectionKeywordCount() const override { return 2; }

    // ========================================================================
    // Packet Definitions
    // ========================================================================

    size_t getCommandPacketTypeCount() const override { return 1; }

    PacketDescriptor getCommandPacketDescriptor(size_t index) const override {
        return {"DemoCommand", 0xDEADBEEF, sizeof(CommandPacket), sizeof(CommandPacket), true, nullptr, 0};
    }

    size_t getTelemetryPacketTypeCount() const override { return 1; }

    PacketDescriptor getTelemetryPacketDescriptor(size_t index) const override {
        return {"DemoTelemetry", 0xCAFEBABE, sizeof(TelemetryPacket), sizeof(TelemetryPacket), true, nullptr, 0};
    }

    // ========================================================================
    // Lifecycle Hooks
    // ========================================================================

    void onInit() override {
        Serial.println("[DemoModule] Initialized");
    }

    void onPair() override {
        Serial.println("[DemoModule] Paired!");
        AudioRegistry::play("paired");
    }

    void onUnpair() override {
        Serial.println("[DemoModule] Unpaired");
        armed_ = false;
    }

    // ========================================================================
    // Control Loop
    // ========================================================================

    void updateControl(const InputManager& inputs, float dt) override {
        // Read joysticks
        command_.leftMotor = static_cast<int16_t>(inputs.getJoystickA_Y() * 1000.0f);
        command_.rightMotor = static_cast<int16_t>(inputs.getJoystickB_Y() * 1000.0f);
        command_.armed = armed_;
    }

    size_t prepareCommandPacket(size_t typeIndex, uint8_t* buffer, size_t bufferSize) override {
        memcpy(buffer, &command_, sizeof(command_));
        return sizeof(command_);
    }

    void handleTelemetry(size_t typeIndex, const uint8_t* data, size_t length) override {
        memcpy(&telemetry_, data, sizeof(telemetry_));
    }

    // ========================================================================
    // Display
    // ========================================================================

    void drawDashboard(DisplayCanvas& canvas) override {
        // Use modern UI components
        UIComponents::drawModernHeader(canvas, "Demo Module", ICON_ROBOT,
                                      telemetry_.batteryPercent, 4);

        // Connection status
        UIComponents::drawConnectionStatus(canvas, 4, 20, isPaired());

        // Armed status with icon
        UIComponents::drawLabeledValue(canvas, 4, 32,
                                      armed_ ? ICON_UNLOCK : ICON_LOCK,
                                      "Status",
                                      armed_ ? "ARMED" : "Disarmed");

        // Battery progress bar
        canvas.drawProgressBar(4, 44, 120, 8, telemetry_.batteryPercent / 100.0f, "Battery");

        // Footer with button labels
        UIComponents::drawModernFooter(canvas, "Arm", "PID", "Menu");
    }

    // ========================================================================
    // Public State (for bindings/menus)
    // ========================================================================

    bool armed_ = false;
    float pidP_ = 1.0f;
    float pidI_ = 0.5f;
    float pidD_ = 0.1f;

private:
    struct CommandPacket {
        uint32_t magic = 0xDEADBEEF;
        int16_t leftMotor = 0;
        int16_t rightMotor = 0;
        bool armed = false;
    } __attribute__((packed));

    struct TelemetryPacket {
        uint32_t magic = 0xCAFEBABE;
        uint8_t batteryPercent = 100;
        float temperature = 25.0f;
    } __attribute__((packed));

    CommandPacket command_;
    TelemetryPacket telemetry_;
};

// Create global instance for bindings/menus to access
DemoModule demoModule;

// Register the module
REGISTER_MODULE(DemoModule);

// ============================================================================
// CUSTOM AUDIO CUES
// ============================================================================

REGISTER_AUDIO("paired", 1000, 200);
REGISTER_AUDIO("armed", 1200, 100);
REGISTER_AUDIO("disarmed", 800, 150);
REGISTER_AUDIO("pid_changed", 600, 50);

// ============================================================================
// CUSTOM ICONS
// ============================================================================

REGISTER_ICON("demo_icon", 8, 8, {
    0b00111100,
    0b01111110,
    0b11111111,
    0b11011011,
    0b11111111,
    0b01111110,
    0b00111100,
    0b00011000
});

// ============================================================================
// CUSTOM MENU ENTRIES
// ============================================================================

// Quick Actions submenu
REGISTER_MENU_ENTRY_CONDITIONAL(
    "demo.arm",
    "quick_actions",
    ICON_UNLOCK,
    "Arm Motors",
    []() { return demoModule.isPaired() && !demoModule.armed_; },
    []() {
        demoModule.armed_ = true;
        AudioRegistry::play("armed");
    }
);

REGISTER_MENU_ENTRY_CONDITIONAL(
    "demo.disarm",
    "quick_actions",
    ICON_LOCK,
    "Disarm Motors",
    []() { return demoModule.isPaired() && demoModule.armed_; },
    []() {
        demoModule.armed_ = false;
        AudioRegistry::play("disarmed");
    }
);

// Settings submenu
REGISTER_MENU_SUBMENU(
    "demo_settings",
    "settings",
    "demo_icon",
    "Demo Settings"
);

REGISTER_MENU_ENTRY_SIMPLE(
    "demo.pid_tuning",
    "demo_settings",
    ICON_TUNING,
    "PID Tuning",
    []() { ScreenRegistry::show("pid_screen"); }
);

// ============================================================================
// CUSTOM SCREENS
// ============================================================================

REGISTER_SCREEN("pid_screen", {
    .title = "PID Tuning",
    .icon = ICON_TUNING,
    .drawFunc = [](DisplayCanvas& canvas) {
        canvas.clear();

        // Modern header
        UIComponents::drawModernHeader(canvas, "PID Tuning", ICON_TUNING);

        // PID values
        canvas.setFont(DisplayCanvas::SMALL);
        canvas.drawTextF(4, 24, "P: %.3f", demoModule.pidP_);
        canvas.drawProgressBar(40, 18, 84, 6, demoModule.pidP_ / 10.0f);

        canvas.drawTextF(4, 36, "I: %.3f", demoModule.pidI_);
        canvas.drawProgressBar(40, 30, 84, 6, demoModule.pidI_ / 10.0f);

        canvas.drawTextF(4, 48, "D: %.3f", demoModule.pidD_);
        canvas.drawProgressBar(40, 42, 84, 6, demoModule.pidD_ / 10.0f);

        // Footer
        UIComponents::drawModernFooter(canvas, nullptr, nullptr, "Back");
    },
    .onEncoderRotate = [](int delta) {
        // Adjust PID P value with encoder
        demoModule.pidP_ += delta * 0.001f;
        demoModule.pidP_ = constrain(demoModule.pidP_, 0.0f, 10.0f);
        AudioRegistry::play("pid_changed");
    },
    .onButton3 = []() {
        // Back button
        ScreenRegistry::back();
    }
});

// ============================================================================
// CUSTOM CONTROL BINDINGS
// ============================================================================

// Button 1: Toggle arm/disarm (simple press)
REGISTER_CONTROL_BINDING({
    .input = INPUT_BUTTON1,
    .event = EVENT_PRESS,
    .action = []() {
        if (demoModule.isPaired()) {
            demoModule.armed_ = !demoModule.armed_;
            AudioRegistry::play(demoModule.armed_ ? "armed" : "disarmed");
        }
    },
    .condition = []() { return demoModule.isPaired(); }
});

// Button 2: Open PID tuning (click)
REGISTER_CONTROL_BINDING({
    .input = INPUT_BUTTON2,
    .event = EVENT_CLICK,
    .action = []() {
        ScreenRegistry::show("pid_screen");
    }
});

// Button 1: Emergency stop (long press 2+ seconds)
REGISTER_CONTROL_BINDING({
    .input = INPUT_BUTTON1,
    .event = EVENT_LONG_PRESS,
    .action = []() {
        demoModule.armed_ = false;
        AudioRegistry::play("disarmed");
        Serial.println("EMERGENCY STOP!");
    },
    .priority = 100  // High priority
});

// Encoder: Navigate PID screen (screen-specific binding)
REGISTER_CONTROL_BINDING({
    .input = INPUT_ENCODER_ROTATE,
    .event = EVENT_PRESS,
    .actionWithValue = [](int delta) {
        demoModule.pidP_ += delta * 0.001f;
        demoModule.pidP_ = constrain(demoModule.pidP_, 0.0f, 10.0f);
    },
    .screenId = "pid_screen"  // Only active on PID screen
});

// ============================================================================
// MAIN PROGRAM
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("===========================================");
    Serial.println("  ILITE Framework Extension Demo");
    Serial.println("===========================================");

    // Initialize framework with custom config
    ILITEConfig config;
    config.controlLoopHz = 50;
    config.displayRefreshHz = 10;
    config.enableAudio = true;

    if (!ILITE.begin(config)) {
        Serial.println("ERROR: Framework initialization failed!");
        while (1) delay(1000);
    }

    Serial.println("\nDemo ready!");
    Serial.println("Try these features:");
    Serial.println("- Button 1: Toggle arm/disarm");
    Serial.println("- Button 1 (hold 2s): Emergency stop");
    Serial.println("- Button 2: Open PID tuning");
    Serial.println("- Encoder: Adjust values on screens");
}

void loop() {
    ILITE.update();
}
