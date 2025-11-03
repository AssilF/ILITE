/**
 * @file generic_module.cpp
 * @brief Universal/Generic platform implementation
 */

#include "generic_module.h"
#include "input.h"
#include "display.h"
#include "audio_feedback.h"
#include <cstring>

// ============================================================================
// Global State
// ============================================================================

GenericCommand genericCommand{};
GenericTelemetry genericTelemetry{};
GenericControlState genericState{};

// ============================================================================
// Forward Declarations
// ============================================================================

extern byte displayMode;
extern U8G2_SH1106_128X64_NONAME_F_HW_I2C oled;

// ============================================================================
// Initialization
// ============================================================================

void initGenericState() {
    memset(&genericCommand, 0, sizeof(genericCommand));
    memset(&genericTelemetry, 0, sizeof(genericTelemetry));
    genericState.lastCommandTime = 0;
    genericState.commandCount = 0;
    genericState.connectionActive = false;
}

// ============================================================================
// Control Update
// ============================================================================

void updateGenericControl() {
    // Read all analog inputs (no processing, raw values)
    genericCommand.joystickA_X = analogRead(joystickA_X);
    genericCommand.joystickA_Y = analogRead(joystickA_Y);
    genericCommand.joystickB_X = analogRead(joystickB_X);
    genericCommand.joystickB_Y = analogRead(joystickB_Y);
    genericCommand.potA = analogRead(potA);

    // Pack button states into bitfield
    uint8_t buttons = 0;
    if (digitalRead(button1) == LOW) buttons |= 0x08;
    if (digitalRead(button2) == LOW) buttons |= 0x10;
    if (digitalRead(button3) == LOW) buttons |= 0x20;
    if (digitalRead(joystickBtnA) == LOW) buttons |= 0x02;
    if (digitalRead(joystickBtnB) == LOW) buttons |= 0x04;
    if (digitalRead(encoderBtn) == LOW) buttons |= 0x01;

    genericCommand.buttons = buttons;
    genericCommand.reserved = 0;

    genericState.lastCommandTime = millis();
    genericState.commandCount++;
}

// ============================================================================
// Telemetry Processing
// ============================================================================

void handleGenericIncoming(ModuleState& state, const uint8_t* data, size_t length) {
    (void)state; // Unused

    if (length > GENERIC_MAX_PACKET_SIZE) {
        length = GENERIC_MAX_PACKET_SIZE;
    }

    memcpy(genericTelemetry.data, data, length);
    genericTelemetry.length = length;
    genericTelemetry.lastUpdate = millis();
    genericTelemetry.packetCount++;

    genericState.connectionActive = true;
}

// ============================================================================
// Payload Preparation
// ============================================================================

const uint8_t* prepareGenericPayload(size_t& size) {
    size = sizeof(genericCommand);
    return reinterpret_cast<const uint8_t*>(&genericCommand);
}

// ============================================================================
// Display Functions
// ============================================================================

void drawGenericDashboard() {
    extern void drawHeader(const char* title);

    oled.clearBuffer();
    drawHeader("Generic");

    oled.setFont(u8g2_font_5x8_tf);

    // Connection status
    const char* status = genericState.connectionActive ? "CONNECTED" : "NO DATA";
    oled.setCursor(2, 20);
    oled.print("Status: ");
    oled.print(status);

    // Packet counts
    oled.setCursor(2, 28);
    oled.printf("TX: %lu  RX: %lu", genericState.commandCount, genericTelemetry.packetCount);

    // Joystick values (bar graphs)
    oled.setCursor(2, 38);
    oled.print("Joy A:");
    int barA_X = map(genericCommand.joystickA_X, 0, 4095, 0, 30);
    int barA_Y = map(genericCommand.joystickA_Y, 0, 4095, 0, 30);
    oled.drawFrame(38, 32, 32, 6);
    oled.drawBox(38, 32, barA_X, 6);

    oled.setCursor(2, 46);
    oled.print("Joy B:");
    int barB_X = map(genericCommand.joystickB_X, 0, 4095, 0, 30);
    int barB_Y = map(genericCommand.joystickB_Y, 0, 4095, 0, 30);
    oled.drawFrame(38, 40, 32, 6);
    oled.drawBox(38, 40, barB_X, 6);

    // Button states
    oled.setCursor(2, 54);
    oled.print("Buttons: ");
    oled.printf("%02X", genericCommand.buttons);

    // Last telemetry bytes (hex dump of first 8 bytes)
    if (genericTelemetry.length > 0) {
        oled.setCursor(2, 62);
        oled.print("RX: ");
        for (size_t i = 0; i < 6 && i < genericTelemetry.length; ++i) {
            oled.printf("%02X ", genericTelemetry.data[i]);
        }
    }

    oled.sendBuffer();
}

void drawGenericLayoutCard(const ModuleState& state, int16_t x, int16_t y, bool focused) {
    extern void drawLayoutCardFrame(int16_t x, int16_t y, const char* title, const char* subtitle, bool focused);
    extern ModuleState* getActiveModule();
    extern const int screen_Width;

    const ModuleState* active = getActiveModule();
    const bool isActive = active == &state;
    const int16_t width = screen_Width - x * 2;

    drawLayoutCardFrame(x, y, "Universal", isActive ? "Active layout" : "Generic device", focused);

    // Draw icon (question mark symbol)
    oled.setFont(u8g2_font_open_iconic_all_1x_t);
    oled.setCursor(x + width / 2 - 5, y + 32);
    oled.print("\u00ef"); // Question mark icon
}

// ============================================================================
// Function Button Actions
// ============================================================================

void actionGenericViewPackets(ModuleState& state, size_t slot) {
    (void)state;
    (void)slot;
    // TODO: Switch to packet viewer display mode
    displayMode = DISPLAY_MODE_PACKET;
    audioFeedback(AudioCue::Select);
}

void actionGenericToggleInfo(ModuleState& state, size_t slot) {
    bool next = !getFunctionOutput(state, slot);
    setFunctionOutput(state, slot, next);
    audioFeedback(next ? AudioCue::ToggleOn : AudioCue::ToggleOff);
}

// ============================================================================
// Module Descriptor
// ============================================================================

static const FunctionActionOption kGenericOptions[] = {
    {"View Packets", "PKT", actionGenericViewPackets},
    {"Toggle Info", "INFO", actionGenericToggleInfo},
};

const ModuleDescriptor kGenericDescriptor{
    PeerKind::Generic,
    "Universal",
    drawGenericDashboard,
    drawGenericLayoutCard,
    handleGenericIncoming,
    updateGenericControl,
    prepareGenericPayload,
    kGenericOptions,
    sizeof(kGenericOptions) / sizeof(kGenericOptions[0])
};
