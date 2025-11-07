/**
 * @file bulky.cpp
 * @brief Bulky utility carrier robot implementation
 */

#include "bulky.h"
#include "ILITEModule.h"
#include "ModuleRegistry.h"
#include "InputManager.h"
#include "DisplayCanvas.h"
#include "input.h"
#include "display.h"
#include "audio_feedback.h"
#include "telemetry.h"
#include <cstring>

// ============================================================================
// Global State
// ============================================================================

// Note: bulkyCommand is already declared in telemetry.cpp
// BulkyCommand bulkyCommand{0, 0, 0, {0, 0, 0}};

BulkyTelemetry bulkyTelemetry{};
BulkyControlState bulkyState{};

// ============================================================================
// Forward Declarations
// ============================================================================

extern byte displayMode;
extern U8G2_SH1106_128X64_NONAME_F_HW_I2C oled;

// These variables are used by Bulky display functions
extern byte Front_Distance;
extern byte Bottom_Distance;
extern byte speed;
extern byte linePosition;
extern byte firePose;

// ============================================================================
// Initialization
// ============================================================================

void initBulkyState() {
    memset(&bulkyCommand, 0, sizeof(bulkyCommand));
    memset(&bulkyTelemetry, 0, sizeof(bulkyTelemetry));

    bulkyState.targetSpeed = 0;
    bulkyState.motionState = 0;
    bulkyState.honkActive = false;
    bulkyState.lightActive = false;
    bulkyState.slowModeActive = false;
    bulkyState.lastTelemetryTime = 0;
    bulkyState.connectionActive = false;
}

// ============================================================================
// Control Update
// ============================================================================

void updateBulkyControl() {
    // Read joystick for motion control
    int16_t joyX = analogRead(joystickB_X);
    int16_t joyY = analogRead(joystickB_Y);

    // Map joystick Y to speed (-100 to 100)
    int16_t mappedY = map(joyY, 0, 4095, -100, 100);

    // Apply deadzone
    if (abs(mappedY) < 10) {
        mappedY = 0;
    }

    bulkyState.targetSpeed = static_cast<int8_t>(mappedY);

    // Map joystick X to motion state (for turning/steering)
    int16_t mappedX = map(joyX, 0, 4095, -100, 100);

    // Simple motion state encoding:
    // 0 = stop, 1 = forward, 2 = backward, 3 = turn left, 4 = turn right
    if (abs(mappedY) < 10 && abs(mappedX) < 10) {
        bulkyState.motionState = 0; // Stop
    } else if (abs(mappedX) > abs(mappedY)) {
        // Turning dominates
        bulkyState.motionState = (mappedX > 0) ? 4 : 3; // Turn right : Turn left
    } else {
        // Forward/backward dominates
        bulkyState.motionState = (mappedY > 0) ? 1 : 2; // Forward : Backward
    }

    // Read speed potentiometer for speed scaling
    uint16_t potValue = analogRead(potA);
    uint8_t speedScale = map(potValue, 0, 4095, 0, 100);

    // Apply speed scaling
    bulkyCommand.speed = (bulkyState.targetSpeed * speedScale) / 100;
    bulkyCommand.motionState = bulkyState.motionState;

    // Read button states
    bulkyCommand.buttonStates[0] = digitalRead(button1) ? 1 : 0;
    bulkyCommand.buttonStates[1] = digitalRead(button2) ? 1 : 0;
    bulkyCommand.buttonStates[2] = digitalRead(button3) ? 1 : 0;

    // Increment reply index
    bulkyCommand.replyIndex++;
}

// ============================================================================
// Telemetry Handling
// ============================================================================

void handleBulkyTelemetry(const uint8_t* data, size_t length) {
    if (length < sizeof(BulkyTelemetry)) {
        Serial.printf("[Bulky] Telemetry too short: %d bytes\n", length);
        return;
    }

    memcpy(&bulkyTelemetry, data, sizeof(BulkyTelemetry));

    // Verify magic number
    if (bulkyTelemetry.magic != BULKY_PACKET_MAGIC) {
        Serial.printf("[Bulky] Invalid magic: 0x%08X (expected 0x%08X)\n",
                      bulkyTelemetry.magic, BULKY_PACKET_MAGIC);
        return;
    }

    // Update display variables (used by drawBulkyDashboard)
    Front_Distance = bulkyTelemetry.frontDistance;
    Bottom_Distance = bulkyTelemetry.bottomDistance;
    linePosition = bulkyTelemetry.linePosition;
    firePose = bulkyTelemetry.firePose;
    speed = bulkyTelemetry.currentSpeed;

    bulkyState.lastTelemetryTime = millis();
    bulkyState.connectionActive = true;
}

// ============================================================================
// Payload Preparation
// ============================================================================

const uint8_t* getBulkyPayload(size_t& size) {
    size = sizeof(BulkyCommand);
    return reinterpret_cast<const uint8_t*>(&bulkyCommand);
}

// ============================================================================
// Display Functions
// ============================================================================

// Note: drawBulkyDashboard() is already implemented in display.cpp
// It calls: drawFirePosition(), drawLine(), drawMotionJoystickPose(),
//           drawPeripheralJoystickPose(), drawProximity(), drawSpeed()

void drawBulkyLayoutCard(int16_t x, int16_t y, bool focused) {
    // Draw card background
    if (focused) {
        oled.setDrawColor(1);
        oled.drawRBox(x, y, 60, 30, 3);
        oled.setDrawColor(0);
    } else {
        oled.setDrawColor(1);
        oled.drawRFrame(x, y, 60, 30, 3);
    }

    // Draw Bulky icon (simplified truck/carrier icon)
    int16_t iconX = x + 8;
    int16_t iconY = y + 8;

    // Draw truck body
    oled.drawBox(iconX, iconY, 20, 12);
    oled.drawBox(iconX + 20, iconY + 4, 8, 8);

    // Draw wheels
    oled.drawCircle(iconX + 5, iconY + 14, 3);
    oled.drawCircle(iconX + 15, iconY + 14, 3);
    oled.drawCircle(iconX + 23, iconY + 14, 3);

    // Draw label
    oled.setFont(u8g2_font_5x7_tr);
    int16_t textX = x + 36;
    int16_t textY = y + 12;

    if (focused) {
        oled.setDrawColor(0);
    } else {
        oled.setDrawColor(1);
    }

    oled.setCursor(textX, textY);
    oled.print("Bulky");

    // Show connection status
    if (bulkyState.connectionActive) {
        int16_t dotX = textX + 4;
        int16_t dotY = y + 22;
        oled.drawDisc(dotX, dotY, 2);
    }

    oled.setDrawColor(1);
}

// ============================================================================

