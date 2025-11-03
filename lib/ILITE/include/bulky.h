/**
 * @file bulky.h
 * @brief Bulky utility carrier robot implementation
 *
 * Bulky is a utility carrier robot with line following, fire detection,
 * proximity sensors, and adjustable speed control.
 *
 * @author ILITE Team
 * @date 2025
 */

#pragma once

#include <Arduino.h>
#include <ILITE.h>

constexpr uint32_t BULKY_PACKET_MAGIC = 0x42554C4B; // 'BULK'

/**
 * @brief Command packet sent from ILITE to Bulky
 */
struct BulkyCommand {
    uint8_t replyIndex;
    int8_t speed;
    uint8_t motionState;
    uint8_t buttonStates[3];
} __attribute__((packed));

/**
 * @brief Telemetry packet sent from Bulky to ILITE
 */
struct BulkyTelemetry {
    uint32_t magic;              ///< Should be BULKY_PACKET_MAGIC
    uint8_t frontDistance;       ///< Front proximity sensor (0-50 cm)
    uint8_t bottomDistance;      ///< Bottom proximity sensor (0-50 cm)
    uint8_t linePosition;        ///< Line sensor position (bitmask: 4 bits)
    uint8_t firePose;            ///< Fire detection angle (0-180 degrees)
    uint8_t currentSpeed;        ///< Current speed (0-100%)
    uint8_t reserved[3];         ///< Reserved for future use
} __attribute__((packed));

/**
 * @brief Runtime state for Bulky control
 */
struct BulkyControlState {
    int8_t targetSpeed;          ///< Target speed (-100 to 100)
    uint8_t motionState;         ///< Current motion state
    bool honkActive;             ///< Honk/buzzer active
    bool lightActive;            ///< Light active
    bool slowModeActive;         ///< Slow mode active
    uint32_t lastTelemetryTime;  ///< Last telemetry received (ms)
    bool connectionActive;       ///< Connection status
};

// Global instances
extern BulkyCommand bulkyCommand;
extern BulkyTelemetry bulkyTelemetry;
extern BulkyControlState bulkyState;

/**
 * @brief Initialize Bulky state
 */
void initBulkyState();

/**
 * @brief Update Bulky control logic (read inputs, compute commands)
 */
void updateBulkyControl();

/**
 * @brief Handle incoming telemetry from Bulky
 * @param data Received packet data
 * @param length Packet length
 */
void handleBulkyTelemetry(const uint8_t* data, size_t length);

/**
 * @brief Get command packet for transmission
 * @param size Output: packet size
 * @return Pointer to command packet
 */
const uint8_t* getBulkyPayload(size_t& size);

/**
 * @brief Draw Bulky dashboard (called when Bulky is active module)
 */
void drawBulkyDashboard();

/**
 * @brief Draw Bulky layout card for home screen
 * @param x X position
 * @param y Y position
 * @param focused Whether this card is focused
 */
void drawBulkyLayoutCard(int16_t x, int16_t y, bool focused);
