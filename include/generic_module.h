/**
 * @file generic_module.h
 * @brief Universal/Generic platform module for unknown devices
 *
 * This module provides a fallback interface when the connected device
 * doesn't match any known platform type. It displays raw telemetry data
 * and allows basic control without platform-specific features.
 *
 * Use cases:
 * - Testing new robots before creating dedicated modules
 * - Debugging communication with prototype hardware
 * - Universal passthrough mode
 *
 * The generic module:
 * - Accepts any incoming packet format (displays as hex)
 * - Sends dual-joystick + button data
 * - Shows connection status and packet statistics
 * - Provides no specialized controls (raw joystick forwarding)
 *
 * @author ILITE Team
 * @date 2025
 */

#pragma once
#include <Arduino.h>
#include "ui_modules.h"

/// Maximum packet size for generic telemetry/command
constexpr size_t GENERIC_MAX_PACKET_SIZE = 128;

/**
 * @brief Generic command packet - dual joystick + buttons
 *
 * Simple packet format that forwards raw joystick and button states.
 * Compatible with basic RC applications.
 */
struct GenericCommand {
    uint16_t joystickA_X;       ///< Joystick A horizontal (0-4095)
    uint16_t joystickA_Y;       ///< Joystick A vertical (0-4095)
    uint16_t joystickB_X;       ///< Joystick B horizontal (0-4095)
    uint16_t joystickB_Y;       ///< Joystick B vertical (0-4095)
    uint16_t potA;              ///< Potentiometer A (0-4095)
    uint8_t buttons;            ///< Button bitfield: [7:3]=btn3-1, [2:1]=joyB/A, [0]=encoder
    uint8_t reserved;           ///< Reserved for future use
} __attribute__((packed));

/**
 * @brief Generic telemetry packet - raw data buffer
 *
 * Stores whatever the device sends without interpretation.
 * Dashboard displays as hex dump or attempts basic decoding.
 */
struct GenericTelemetry {
    uint8_t data[GENERIC_MAX_PACKET_SIZE];  ///< Raw packet bytes
    size_t length;                           ///< Valid data length
    uint32_t lastUpdate;                     ///< Timestamp of last packet (millis)
    uint32_t packetCount;                    ///< Total packets received
};

/**
 * @brief Generic control state
 */
struct GenericControlState {
    uint32_t lastCommandTime;     ///< Last command transmission time
    uint32_t commandCount;        ///< Total commands sent
    bool connectionActive;        ///< Whether receiving telemetry
};

/// Global generic command packet
extern GenericCommand genericCommand;

/// Global generic telemetry buffer
extern GenericTelemetry genericTelemetry;

/// Global generic control state
extern GenericControlState genericState;

/**
 * @brief Initialize generic module state
 */
void initGenericState();

/**
 * @brief Update generic control from inputs
 *
 * Reads all joysticks, potentiometer, and buttons, then packs into
 * GenericCommand. No processing or scaling applied (raw values).
 */
void updateGenericControl();

/**
 * @brief Process incoming telemetry from unknown device
 *
 * Stores raw bytes for display/analysis.
 *
 * @param state Module state
 * @param data Raw packet bytes
 * @param length Packet length
 */
void handleGenericIncoming(ModuleState& state, const uint8_t* data, size_t length);

/**
 * @brief Prepare generic command packet for transmission
 *
 * @param size Output parameter: set to sizeof(genericCommand)
 * @return Pointer to genericCommand bytes
 */
const uint8_t* prepareGenericPayload(size_t& size);

/**
 * @brief Render generic dashboard
 *
 * Displays:
 * - Connection status and packet counts
 * - Raw joystick values (bar graphs)
 * - Button states (icons)
 * - Telemetry hex dump (last N bytes)
 */
void drawGenericDashboard();

/**
 * @brief Render generic layout card on home screen
 *
 * @param state Module state
 * @param x Card X position
 * @param y Card Y position
 * @param focused Whether card is selected
 */
void drawGenericLayoutCard(const ModuleState& state, int16_t x, int16_t y, bool focused);

/**
 * @brief Open raw packet viewer
 *
 * @param state Module state
 * @param slot Function button slot
 */
void actionGenericViewPackets(ModuleState& state, size_t slot);

/**
 * @brief Toggle connection info overlay
 *
 * @param state Module state
 * @param slot Function button slot
 */
void actionGenericToggleInfo(ModuleState& state, size_t slot);

/// Generic module descriptor (defined in generic_module.cpp)
extern const ModuleDescriptor kGenericDescriptor;
