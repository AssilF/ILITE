/**
 * @file MinimalRobot.h
 * @brief Minimal ILITE Module Example
 *
 * This demonstrates the absolute minimum required to create a working
 * ILITE control module. This robot has:
 * - Simple tank drive (left/right motor speeds)
 * - Battery voltage telemetry
 * - One function button (toggle lights)
 *
 * Total implementation: ~100 lines of code!
 *
 * @author ILITE Example
 * @date 2025
 */

#pragma once
#include <ILITEModule.h>

/**
 * @brief Minimal robot control module
 *
 * Demonstrates the essential ILITEModule interface implementation.
 */
class MinimalRobotModule : public ILITEModule {
public:
    // ========================================================================
    // Metadata (REQUIRED)
    // ========================================================================

    const char* getModuleId() const override {
        return "com.example.minimal";
    }

    const char* getModuleName() const override {
        return "Minimal Robot";
    }

    const char* getVersion() const override {
        return "1.0.0";
    }

    const char* getAuthor() const override {
        return "ILITE Examples";
    }

    const char** getDetectionKeywords() const override {
        static const char* keywords[] = {"minimal", "minirobot"};
        return keywords;
    }

    size_t getDetectionKeywordCount() const override {
        return 2;  // Number of keywords in getDetectionKeywords()
    }

    // ========================================================================
    // Packet Definitions (REQUIRED)
    // ========================================================================

    size_t getCommandPacketTypeCount() const override { return 1; }

    PacketDescriptor getCommandPacketDescriptor(size_t index) const override {
        static const PacketDescriptor::Field fields[] = {
            {"magic", 0, 4, PacketDescriptor::Field::UINT32},
            {"leftSpeed", 4, 2, PacketDescriptor::Field::INT16},
            {"rightSpeed", 6, 2, PacketDescriptor::Field::INT16},
            {"flags", 8, 1, PacketDescriptor::Field::UINT8}
        };

        return {
            "Command",                  // name
            0x12345678,                // magic number
            sizeof(CommandPacket),     // minSize
            sizeof(CommandPacket),     // maxSize
            true,                      // usesMagic
            fields,                    // field definitions
            4                          // fieldCount
        };
    }

    size_t getTelemetryPacketTypeCount() const override { return 1; }

    PacketDescriptor getTelemetryPacketDescriptor(size_t index) const override {
        static const PacketDescriptor::Field fields[] = {
            {"magic", 0, 4, PacketDescriptor::Field::UINT32},
            {"batteryVolts", 4, 4, PacketDescriptor::Field::FLOAT}
        };

        return {
            "Telemetry",
            0x87654321,
            sizeof(TelemetryPacket),
            sizeof(TelemetryPacket),
            true,
            fields,
            2
        };
    }

    // ========================================================================
    // Lifecycle (OPTIONAL - using defaults)
    // ========================================================================

    void onInit() override {
        Serial.println("[MinimalRobot] Module initialized");
        command_ = {};
        telemetry_ = {};
        lightsOn_ = false;
    }

    void onPair() override {
        Serial.println("[MinimalRobot] Device paired - resetting to safe state");
        command_.leftSpeed = 0;
        command_.rightSpeed = 0;
        command_.flags = 0;
        lightsOn_ = false;
    }

    void onUnpair() override {
        Serial.println("[MinimalRobot] Device unpaired - emergency stop");
        command_.leftSpeed = 0;
        command_.rightSpeed = 0;
    }

    // ========================================================================
    // Control Loop (REQUIRED - 50Hz)
    // ========================================================================

    void updateControl(const InputManager& inputs, float dt) override {
        // Tank drive: Left stick controls left motor, right stick controls right motor
        float leftInput = inputs.getJoystickA_Y();   // -1.0 to +1.0
        float rightInput = inputs.getJoystickB_Y();

        // Scale to motor range (-1000 to +1000)
        command_.leftSpeed = static_cast<int16_t>(leftInput * 1000.0f);
        command_.rightSpeed = static_cast<int16_t>(rightInput * 1000.0f);

        // Pack flags: bit 0 = lights
        command_.flags = lightsOn_ ? 0x01 : 0x00;
    }

    size_t prepareCommandPacket(size_t packetTypeIndex, uint8_t* buffer, size_t bufferSize) override {
        if (packetTypeIndex != 0 || bufferSize < sizeof(command_)) {
            return 0;
        }

        memcpy(buffer, &command_, sizeof(command_));
        return sizeof(command_);
    }

    void handleTelemetry(size_t packetTypeIndex, const uint8_t* data, size_t length) override {
        if (packetTypeIndex == 0 && length >= sizeof(telemetry_)) {
            memcpy(&telemetry_, data, sizeof(telemetry_));
        }
    }

    // ========================================================================
    // Display (REQUIRED)
    // ========================================================================

    void drawDashboard(DisplayCanvas& canvas) override {
        canvas.clear();
        canvas.drawHeader("Minimal Robot");

        // Battery gauge
        float batteryPercent = (telemetry_.batteryVolts - 6.0f) / (8.4f - 6.0f);
        batteryPercent = constrain(batteryPercent, 0.0f, 1.0f);
        canvas.drawProgressBar(10, 20, 108, 10, batteryPercent, "Battery");

        // Connection status
        canvas.setFont(DisplayCanvas::SMALL);
        canvas.drawTextF(10, 38, "Connected: %s", isPaired() ? "Yes" : "No");

        // Motor speeds
        canvas.drawTextF(10, 46, "Left: %d", command_.leftSpeed);
        canvas.drawTextF(10, 54, "Right: %d", command_.rightSpeed);

        // Lights indicator
        if (lightsOn_) {
            canvas.drawText(90, 54, "LIGHTS ON");
        }
    }

    // ========================================================================
    // Function Buttons (OPTIONAL)
    // ========================================================================

    size_t getFunctionActionCount() const override { return 1; }

    FunctionAction getFunctionAction(size_t index) const override {
        if (index == 0) {
            return {
                "Toggle Lights",                           // name
                "LITE",                                    // shortLabel
                [this]() {                                 // callback
                    auto* self = const_cast<MinimalRobotModule*>(this);
                    self->lightsOn_ = !self->lightsOn_;
                    self->getAudio().playToggle(self->lightsOn_);
                },
                true                                       // isToggle
            };
        }
        return {};
    }

private:
    /**
     * @brief Command packet structure
     */
    struct CommandPacket {
        uint32_t magic = 0x12345678;
        int16_t leftSpeed = 0;      // -1000 to +1000
        int16_t rightSpeed = 0;     // -1000 to +1000
        uint8_t flags = 0;          // Bit 0: lights
    } __attribute__((packed));

    /**
     * @brief Telemetry packet structure
     */
    struct TelemetryPacket {
        uint32_t magic = 0x87654321;
        float batteryVolts = 0.0f;
    } __attribute__((packed));

    CommandPacket command_;        ///< Current command state
    TelemetryPacket telemetry_;    ///< Last received telemetry
    bool lightsOn_ = false;        ///< Light toggle state
};
