/**
 * @file drongaze.h
 * @brief DroneGaze platform module - Quadcopter flight controller interface
 *
 * This module provides specialized control and telemetry for the DroneGaze
 * quadcopter platform. It handles:
 * - 4-axis flight control (throttle, pitch, roll, yaw)
 * - Real-time PID tuning interface
 * - Per-axis stabilization toggling
 * - Precision flight mode
 * - WiFi telemetry control
 *
 * @author ILITE Team
 * @date 2025
 */

#pragma once
#include <Arduino.h>
#include "ui_modules.h"

/// Magic number for DroneGaze packet validation
constexpr uint32_t DRONGAZE_PACKET_MAGIC = 0xA1B2C3D4;

/// Number of PID control axes (pitch, roll, yaw)
constexpr int DRONGAZE_PID_AXIS_COUNT = 3;

/// Stabilization global enable bit mask
constexpr uint8_t DRONGAZE_STABILIZATION_GLOBAL_BIT = 0x80;

/**
 * @brief Command packet sent from ILITE controller to DroneGaze
 *
 * Contains all flight control parameters and motor arming state.
 * Transmitted at 50Hz via ESP-NOW.
 */
struct DrongazeCommand {
    uint32_t magic;           ///< Must be DRONGAZE_PACKET_MAGIC (0xA1B2C3D4)
    uint16_t throttle;        ///< Base throttle: 1000 = min, 2000 = max
    int8_t pitchAngle;        ///< Desired pitch angle in degrees (-90 to +90)
    int8_t rollAngle;         ///< Desired roll angle in degrees (-90 to +90)
    int8_t yawAngle;          ///< Desired yaw heading in degrees (-180 to +180)
    bool arm_motors;          ///< true = motors armed, false = disarmed
} __attribute__((packed));

/**
 * @brief Telemetry packet sent from DroneGaze to ILITE controller
 *
 * Contains real-time flight data, PID outputs, and system status.
 * Used for dashboard display and PID tuning visualization.
 */
struct DrongazeTelemetry {
    uint32_t magic;                   ///< Should be DRONGAZE_PACKET_MAGIC
    float pitch;                      ///< Actual pitch angle in degrees
    float roll;                       ///< Actual roll angle in degrees
    float yaw;                        ///< Actual yaw heading in degrees
    float pitchCorrection;            ///< PID output for pitch axis
    float rollCorrection;             ///< PID output for roll axis
    float yawCorrection;              ///< PID output for yaw axis
    float verticalCorrection;         ///< PID output for altitude hold
    uint16_t throttle;                ///< Echo of commanded throttle
    int8_t pitchAngle;                ///< Echo of commanded pitch
    int8_t rollAngle;                 ///< Echo of commanded roll
    int8_t yawAngle;                  ///< Echo of commanded yaw
    uint8_t stabilizationMask;        ///< Bitfield: [7]=global, [2]=yaw, [1]=roll, [0]=pitch
    uint8_t reserved[3];              ///< Reserved for future expansion
    float verticalAcc;                ///< Vertical acceleration in m/s²
    uint32_t commandAge;              ///< Age of last received command in milliseconds
} __attribute__((packed));

/**
 * @brief PID gain triplet for one control axis
 */
struct DrongazePidGains {
    float kp;  ///< Proportional gain
    float ki;  ///< Integral gain
    float kd;  ///< Derivative gain
};

/**
 * @brief DroneGaze runtime control state
 *
 * Maintains flight control state including precision mode,
 * yaw command accumulation, and PID tuning data.
 */
struct DrongazeControlState {
    bool precisionMode;                                ///< Halves stick sensitivity when enabled
    int16_t yawCommand;                                ///< Accumulated yaw rate command
    int8_t currentSpeed;                               ///< Current throttle speed setting
    DrongazePidGains pidGains[DRONGAZE_PID_AXIS_COUNT]; ///< Per-axis PID gains
    bool pidGainsValid[DRONGAZE_PID_AXIS_COUNT];       ///< Whether PID gains have been received
    uint8_t stabilizationMask;                         ///< Current stabilization enable mask
    bool stabilizationGlobal;                          ///< Global stabilization enable
};

/// Global DroneGaze command packet (prepared by updateControl)
extern DrongazeCommand drongazeCommand;

/// Global DroneGaze telemetry packet (populated by handleIncoming)
extern DrongazeTelemetry drongazeTelemetry;

/// Global DroneGaze control state
extern DrongazeControlState drongazeState;

/**
 * @brief Initialize DroneGaze control state to safe defaults
 *
 * Resets all control parameters, disarms motors, and clears PID gains.
 * Should be called when pairing with a new DroneGaze device.
 */
void initDrongazeState();

/**
 * @brief Update DroneGaze flight control from joystick inputs
 *
 * Reads joysticks and potentiometer, applies deadzones and precision mode,
 * then updates the drongazeCommand packet. Called at 50Hz by commTask.
 */
void updateDrongazeControl();

/**
 * @brief Process incoming telemetry from DroneGaze
 *
 * Parses telemetry packet and updates drongazeTelemetry and drongazeState.
 *
 * @param state Module state (unused, for interface compliance)
 * @param data Raw packet data
 * @param length Packet length in bytes
 */
void handleDrongazeIncoming(ModuleState& state, const uint8_t* data, size_t length);

/**
 * @brief Prepare DroneGaze command packet for transmission
 *
 * @param size Output parameter: set to sizeof(drongazeCommand)
 * @return Pointer to drongazeCommand packet bytes
 */
const uint8_t* prepareDrongazePayload(size_t& size);

/**
 * @brief Render DroneGaze flight dashboard
 *
 * Displays throttle, orientation, PID corrections, stabilization status,
 * and command age. Provides visual feedback for flight state.
 */
void drawDrongazeDashboard();

/**
 * @brief Render DroneGaze layout card on home screen
 *
 * @param state Module state for this DroneGaze instance
 * @param x Card X position
 * @param y Card Y position
 * @param focused Whether this card is currently selected
 */
void drawDrongazeLayoutCard(const ModuleState& state, int16_t x, int16_t y, bool focused);

/**
 * @brief Toggle motor arming state
 *
 * @param state Module state
 * @param slot Function button slot (0-2)
 */
void actionDrongazeToggleArm(ModuleState& state, size_t slot);

/**
 * @brief Toggle precision mode (half stick sensitivity)
 *
 * @param state Module state
 * @param slot Function button slot (0-2)
 */
void actionDrongazeTogglePrecision(ModuleState& state, size_t slot);

/**
 * @brief Open PID tuning screen
 *
 * @param state Module state
 * @param slot Function button slot (0-2)
 */
void actionDrongazeOpenPidSettings(ModuleState& state, size_t slot);

/**
 * @brief Send PID gains to DroneGaze for specified axis
 *
 * @param axisIndex Axis index: 0=pitch, 1=roll, 2=yaw
 */
void sendDrongazePidGains(int axisIndex);

/**
 * @brief Adjust PID gain by delta step
 *
 * @param axisIndex Axis index: 0=pitch, 1=roll, 2=yaw
 * @param paramIndex Parameter: 0=Kp, 1=Ki, 2=Kd
 * @param delta Step direction: +1 or -1
 */
void adjustDrongazePidGain(int axisIndex, int paramIndex, int delta);

/**
 * @brief Request current PID gains from DroneGaze
 *
 * Sends "pid show" command via ESP-NOW command channel.
 */
void requestDrongazePidGains();

/**
 * @brief Toggle stabilization for specified axis
 *
 * @param axisIndex Axis index: 0=pitch, 1=roll, 2=yaw
 */
void toggleDrongazeAxisStabilization(int axisIndex);

/**
 * @brief Process PID response message from DroneGaze
 *
 * Parses "PID axis: Kp=X Ki=Y Kd=Z" format messages and updates local state.
 *
 * @param message Command response string
 */
void handleDrongazeCommandMessage(const char* message);

/// DroneGaze module descriptor (defined in drongaze.cpp)
extern const ModuleDescriptor kDrongazeDescriptor;
