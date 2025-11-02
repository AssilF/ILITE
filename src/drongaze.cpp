/**
 * @file drongaze.cpp
 * @brief DroneGaze platform implementation
 *
 * Implements flight control, telemetry processing, and PID tuning
 * for the DroneGaze quadcopter platform.
 */

#include "drongaze.h"
#include "input.h"
#include "display.h"
#include "audio_feedback.h"
#include "espnow_discovery.h"
#include "connection_log.h"
#include <cstdio>
#include <cstring>

// ============================================================================
// Global State
// ============================================================================

DrongazeCommand drongazeCommand{DRONGAZE_PACKET_MAGIC, 1000, 0, 0, 0, false};
DrongazeTelemetry drongazeTelemetry{};
DrongazeControlState drongazeState{};

// ============================================================================
// Constants
// ============================================================================

/// Fine adjustment steps for PID tuning (Kp, Ki, Kd)
constexpr float PID_FINE_STEPS[3] = {0.05f, 0.01f, 0.01f};

/// Coarse adjustment steps for PID tuning (Kp, Ki, Kd)
constexpr float PID_COARSE_STEPS[3] = {0.50f, 0.10f, 0.10f};

/// Maximum allowed PID gains (Kp, Ki, Kd)
constexpr float PID_GAIN_MAX[3] = {20.0f, 5.0f, 5.0f};

/// Axis name strings for command generation
static const char* kAxisCommandNames[DRONGAZE_PID_AXIS_COUNT] = {"pitch", "roll", "yaw"};

/// Stabilization mask bits for each axis
static const uint8_t kAxisMaskBits[DRONGAZE_PID_AXIS_COUNT] = {0x02, 0x01, 0x04};

// ============================================================================
// Forward Declarations
// ============================================================================

extern bool controlSessionActive;
extern uint8_t targetAddress[6];
extern EspNowDiscovery discovery;
extern byte displayMode;
extern bool btnmode;
extern int pidTunerAxisIndex;
extern uint8_t pidFocusIndex;
extern bool pidCoarseMode;

// Forward declare sendDroneCommand for internal use
static bool sendDroneCommand(const char* fmt, ...);

// ============================================================================
// Initialization
// ============================================================================

void initDrongazeState() {
    drongazeState.precisionMode = false;
    drongazeState.yawCommand = 0;
    drongazeState.currentSpeed = 0;
    drongazeState.stabilizationMask = 0;
    drongazeState.stabilizationGlobal = false;

    for (int i = 0; i < DRONGAZE_PID_AXIS_COUNT; ++i) {
        drongazeState.pidGains[i] = {0.0f, 0.0f, 0.0f};
        drongazeState.pidGainsValid[i] = false;
    }

    drongazeCommand.magic = DRONGAZE_PACKET_MAGIC;
    drongazeCommand.throttle = 1000;
    drongazeCommand.pitchAngle = 0;
    drongazeCommand.rollAngle = 0;
    drongazeCommand.yawAngle = 0;
    drongazeCommand.arm_motors = false;

    memset(&drongazeTelemetry, 0, sizeof(drongazeTelemetry));
    drongazeTelemetry.magic = DRONGAZE_PACKET_MAGIC;
}

// ============================================================================
// Control Update
// ============================================================================

void updateDrongazeControl() {
    // Throttle: Potentiometer provides base, joystick A Y-axis provides offset
    static uint16_t potFiltered = analogRead(potA);
    potFiltered = (potFiltered * 3 + analogRead(potA)) / 4; // IIR low-pass filter
    uint16_t potOffset = map(potFiltered, 0, 4095, 0, 500);
    potOffset = constrain(potOffset, 0, 500);

    drongazeCommand.throttle = constrain(
        map(analogRead(joystickA_Y), 0, 4095, 2000, -1000) + potOffset,
        1000,
        2000);

    // Yaw: Incremental rate control (not absolute position)
    int16_t yawDelta = map(analogRead(joystickA_X), 0, 4096, -10, 10);
    if (abs(yawDelta) < 2) {
        yawDelta = 0; // Deadband to prevent drift
    }
    drongazeState.yawCommand = constrain(drongazeState.yawCommand + yawDelta, -180, 180);
    drongazeCommand.yawAngle = drongazeState.yawCommand;

    // Roll: Joystick B X-axis
    int16_t roll = map(analogRead(joystickB_X), 0, 4095, -90, 90);
    if (abs(roll) < 10) {
        roll = 0; // Deadzone around center
    }
    if (drongazeState.precisionMode) {
        roll /= 2; // Half sensitivity in precision mode
    }
    drongazeCommand.rollAngle = roll;

    // Pitch: Joystick B Y-axis
    int16_t pitch = map(analogRead(joystickB_Y), 0, 4095, -90, 90);
    if (abs(pitch) < 10) {
        pitch = 0;
    }
    if (drongazeState.precisionMode) {
        pitch /= 2;
    }
    drongazeCommand.pitchAngle = pitch;

    // Arm state from global button mode
    drongazeCommand.arm_motors = btnmode;
}

// ============================================================================
// Telemetry Processing
// ============================================================================

void handleDrongazeIncoming(ModuleState& state, const uint8_t* data, size_t length) {
    (void)state; // Unused, for interface compliance

    // Copy telemetry with size validation
    size_t copySize = length;
    if (copySize > sizeof(drongazeTelemetry)) {
        copySize = sizeof(drongazeTelemetry);
    }
    memcpy(&drongazeTelemetry, data, copySize);

    // Zero-fill any remaining bytes if packet was truncated
    if (copySize < sizeof(drongazeTelemetry)) {
        uint8_t* dest = reinterpret_cast<uint8_t*>(&drongazeTelemetry);
        memset(dest + copySize, 0, sizeof(drongazeTelemetry) - copySize);
    }

    // Update control state from telemetry
    drongazeState.stabilizationMask = drongazeTelemetry.stabilizationMask;
    drongazeState.stabilizationGlobal = (drongazeTelemetry.stabilizationMask & DRONGAZE_STABILIZATION_GLOBAL_BIT) != 0;
}

// ============================================================================
// Payload Preparation
// ============================================================================

const uint8_t* prepareDrongazePayload(size_t& size) {
    size = sizeof(drongazeCommand);
    return reinterpret_cast<const uint8_t*>(&drongazeCommand);
}

// ============================================================================
// PID Tuning
// ============================================================================

static int axisNameToIndex(const char* name) {
    if (!name) return -1;
    if (strcasecmp(name, "pitch") == 0) return 0;
    if (strcasecmp(name, "roll") == 0) return 1;
    if (strcasecmp(name, "yaw") == 0) return 2;
    return -1;
}

static bool sendDroneCommand(const char* fmt, ...) {
    if (!controlSessionActive) {
        connectionLogAdd("Command not sent: no active session");
        return false;
    }

    // Validate target address
    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (memcmp(targetAddress, broadcastAddress, sizeof(targetAddress)) == 0) {
        connectionLogAdd("Command not sent: unknown target");
        return false;
    }

    // Format command string
    char buffer[48];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = '\0';

    if (buffer[0] == '\0') {
        return false;
    }

    // Send via ESP-NOW command channel
    if (!discovery.sendCommand(targetAddress, buffer)) {
        connectionLogAddf("Command send failed: %s", buffer);
        audioFeedback(AudioCue::Error);
        return false;
    }

    connectionLogAddf("Command sent: %s", buffer);
    return true;
}

void sendDrongazePidGains(int axisIndex) {
    if (axisIndex < 0 || axisIndex >= DRONGAZE_PID_AXIS_COUNT) return;

    const DrongazePidGains& gains = drongazeState.pidGains[axisIndex];
    sendDroneCommand("pid set %s %.3f %.3f %.3f",
                     kAxisCommandNames[axisIndex],
                     gains.kp, gains.ki, gains.kd);
}

void adjustDrongazePidGain(int axisIndex, int paramIndex, int delta) {
    if (axisIndex < 0 || axisIndex >= DRONGAZE_PID_AXIS_COUNT) return;
    if (paramIndex < 0 || paramIndex > 2) return;

    DrongazePidGains& gains = drongazeState.pidGains[axisIndex];
    float* value = (paramIndex == 0) ? &gains.kp :
                   (paramIndex == 1) ? &gains.ki : &gains.kd;

    // Initialize to 0 if not yet valid
    if (!drongazeState.pidGainsValid[axisIndex]) {
        *value = 0.0f;
    }

    // Apply step (fine or coarse based on global mode)
    float step = (pidCoarseMode ? PID_COARSE_STEPS[paramIndex] : PID_FINE_STEPS[paramIndex]) * static_cast<float>(delta);
    *value += step;

    // Clamp to valid range
    *value = constrain(*value, 0.0f, PID_GAIN_MAX[paramIndex]);

    drongazeState.pidGainsValid[axisIndex] = true;
    sendDrongazePidGains(axisIndex);
}

void handleDrongazeCommandMessage(const char* message) {
    if (!message) return;

    // Parse "PID axis: Kp=X Ki=Y Kd=Z" format
    if (strncmp(message, "PID ", 4) == 0) {
        char axisName[16] = {0};
        float kp = 0.0f, ki = 0.0f, kd = 0.0f;

        if (sscanf(message, "PID %15[^:]: Kp=%f Ki=%f Kd=%f", axisName, &kp, &ki, &kd) == 4) {
            int axisIndex = axisNameToIndex(axisName);
            if (axisIndex >= 0 && axisIndex < DRONGAZE_PID_AXIS_COUNT) {
                drongazeState.pidGains[axisIndex].kp = kp;
                drongazeState.pidGains[axisIndex].ki = ki;
                drongazeState.pidGains[axisIndex].kd = kd;
                drongazeState.pidGainsValid[axisIndex] = true;
            }
        }
    }
}

void requestDrongazePidGains() {
    sendDroneCommand("pid show");
    sendDroneCommand("stabilization status");
}

void toggleDrongazeAxisStabilization(int axisIndex) {
    if (axisIndex < 0 || axisIndex >= DRONGAZE_PID_AXIS_COUNT) return;

    bool enabled = (drongazeState.stabilizationMask & kAxisMaskBits[axisIndex]) != 0;
    bool enable = !enabled;

    if (sendDroneCommand("stabilization axis %s %s", kAxisCommandNames[axisIndex], enable ? "on" : "off")) {
        audioFeedback(enable ? AudioCue::ToggleOn : AudioCue::ToggleOff);
    }
}

// ============================================================================
// Function Button Actions
// ============================================================================

void actionDrongazeToggleArm(ModuleState& state, size_t slot) {
    bool next = !getFunctionOutput(state, slot);
    setFunctionOutput(state, slot, next);
    btnmode = next;
    audioFeedback(next ? AudioCue::ToggleOn : AudioCue::ToggleOff);
}

void actionDrongazeTogglePrecision(ModuleState& state, size_t slot) {
    drongazeState.precisionMode = !drongazeState.precisionMode;
    setFunctionOutput(state, slot, drongazeState.precisionMode);
    audioFeedback(drongazeState.precisionMode ? AudioCue::ToggleOn : AudioCue::ToggleOff);
}

void actionDrongazeOpenPidSettings(ModuleState& state, size_t slot) {
    (void)slot;
    displayMode = DISPLAY_MODE_PID;
    pidTunerAxisIndex = 0;
    pidFocusIndex = 0;
    pidCoarseMode = false;
    setFunctionOutput(state, slot, false);
    audioFeedback(AudioCue::Select);
    requestDrongazePidGains();
}

// ============================================================================
// Display Functions
// ============================================================================

// Forward declare from display.cpp
extern void drawHeader(const char* title);

void drawDrongazeDashboard() {
    // TODO: Implement DroneGaze-specific dashboard
    // This will be moved from the generic dashboard in display.cpp
    drawHeader("DroneGaze");
    // ... (implementation to follow in display.cpp refactor)
}

void drawDrongazeLayoutCard(const ModuleState& state, int16_t x, int16_t y, bool focused) {
    // Forward declare from display.cpp
    extern void drawLayoutCardFrame(int16_t x, int16_t y, const char* title, const char* subtitle, bool focused);
    extern void drawWifiStatusBadge(const ModuleState& state, int16_t right, int16_t y);
    extern ModuleState* getActiveModule();
    extern U8G2_SH1106_128X64_NONAME_F_HW_I2C oled;
    extern const int screen_Width;

    const ModuleState* active = getActiveModule();
    const bool isActive = active == &state;
    const int16_t width = screen_Width - x * 2;

    drawLayoutCardFrame(x, y, "DroneGaze", isActive ? "Active layout" : "Flight telemetry", focused);

    // Draw icon (plane symbol)
    oled.setFont(u8g2_font_open_iconic_all_1x_t);
    oled.setCursor(x + width / 2 - 5, y + 32);
    oled.print("\u00a5");
}

// ============================================================================
// Module Descriptor
// ============================================================================

static const FunctionActionOption kDrongazeOptions[] = {
    {"Toggle Arm", "ARM", actionDrongazeToggleArm},
    {"PID Screen", "PID", actionDrongazeOpenPidSettings},
    {"Precision Mode", "PREC", actionDrongazeTogglePrecision}
};

const ModuleDescriptor kDrongazeDescriptor{
    PeerKind::Generic,  // Note: We'll change this to PeerKind::Drongaze after updating enum
    "DroneGaze",
    drawDrongazeDashboard,
    drawDrongazeLayoutCard,
    handleDrongazeIncoming,
    updateDrongazeControl,
    prepareDrongazePayload,
    kDrongazeOptions,
    sizeof(kDrongazeOptions) / sizeof(kDrongazeOptions[0])
};
