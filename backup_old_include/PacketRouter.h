/**
 * @file PacketRouter.h
 * @brief Packet Router - Routes incoming telemetry packets to appropriate modules
 *
 * The PacketRouter examines incoming ESP-NOW packets and routes them to the correct
 * module based on magic number matching. It provides thread-safe operation for use
 * in ESP-NOW callbacks and maintains routing statistics.
 *
 * ## Usage Example:
 * ```cpp
 * // In ESP-NOW receive callback:
 * void onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
 *     PacketRouter::getInstance().routePacket(mac, data, len);
 * }
 * ```
 *
 * ## How It Works:
 * 1. Packet arrives via ESP-NOW
 * 2. Router reads magic number (first 4 bytes)
 * 3. Queries active module's telemetry descriptors
 * 4. Finds matching descriptor by magic number
 * 5. Validates packet size
 * 6. Calls module's handleTelemetry() with type index
 *
 * ## Thread Safety:
 * All methods are thread-safe. ESP-NOW callbacks run in WiFi task context,
 * so the router uses FreeRTOS mutexes to protect shared state.
 *
 * @author ILITE Framework
 * @version 1.0.0
 */

#ifndef ILITE_PACKET_ROUTER_H
#define ILITE_PACKET_ROUTER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "ILITEModule.h"

/**
 * @class PacketRouter
 * @brief Routes incoming packets to appropriate modules based on magic numbers
 *
 * Singleton class that examines incoming telemetry packets and dispatches them
 * to the correct module. Uses magic number matching and packet size validation.
 */
class PacketRouter {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the PacketRouter instance
     */
    static PacketRouter& getInstance();

    /**
     * @brief Initialize the packet router
     *
     * Must be called once during framework initialization, before any packets arrive.
     * Creates the mutex for thread safety.
     *
     * @return true if initialization succeeded, false otherwise
     */
    bool begin();

    /**
     * @brief Set the currently active module
     *
     * The router only routes packets to the active module. When modules are
     * switched, call this to update routing.
     *
     * @param module Pointer to the active module (nullptr to disable routing)
     */
    void setActiveModule(ILITEModule* module);

    /**
     * @brief Get the currently active module
     * @return Pointer to active module, or nullptr if none active
     */
    ILITEModule* getActiveModule() const;

    /**
     * @brief Route an incoming packet to the appropriate module
     *
     * Called by ESP-NOW receive callback. Examines packet magic number,
     * finds matching telemetry descriptor in active module, validates size,
     * and calls module's handleTelemetry().
     *
     * Thread-safe - can be called from ESP-NOW callback context.
     *
     * @param macAddr Source MAC address (6 bytes)
     * @param data Packet data
     * @param length Packet length in bytes
     * @return true if packet was successfully routed, false if dropped
     *
     * @note Packets are dropped if:
     *       - No active module set
     *       - Packet too small (< 4 bytes for magic)
     *       - Magic number doesn't match any telemetry descriptor
     *       - Packet size doesn't match descriptor size constraints
     */
    bool routePacket(const uint8_t* macAddr, const uint8_t* data, size_t length);

    // ========================================================================
    // Statistics
    // ========================================================================

    /**
     * @brief Get count of successfully routed packets
     * @return Number of packets routed to modules since last reset
     */
    uint32_t getRoutedCount() const;

    /**
     * @brief Get count of dropped packets
     *
     * Packets are dropped when no module can handle them (no magic match,
     * invalid size, or no active module).
     *
     * @return Number of packets dropped since last reset
     */
    uint32_t getDroppedCount() const;

    /**
     * @brief Get count of routing errors
     *
     * Errors occur when packet format is invalid (too small, corrupted).
     *
     * @return Number of errors since last reset
     */
    uint32_t getErrorCount() const;

    /**
     * @brief Reset all statistics counters to zero
     */
    void resetStats();

private:
    /**
     * @brief Private constructor (singleton pattern)
     */
    PacketRouter();

    /**
     * @brief Destructor - cleans up mutex
     */
    ~PacketRouter();

    // Prevent copying
    PacketRouter(const PacketRouter&) = delete;
    PacketRouter& operator=(const PacketRouter&) = delete;

    /**
     * @brief Try to route packet to specific module
     *
     * Checks all telemetry descriptors of the given module to find a matching
     * magic number, validates size, and calls handleTelemetry().
     *
     * @param module Module to try routing to
     * @param data Packet data
     * @param length Packet length
     * @return true if module accepted the packet, false otherwise
     */
    bool tryRouteToModule(ILITEModule* module, const uint8_t* data, size_t length);

    /**
     * @brief Extract magic number from packet
     *
     * Reads first 4 bytes as little-endian uint32_t.
     *
     * @param data Packet data (must be at least 4 bytes)
     * @return Magic number
     */
    static uint32_t extractMagicNumber(const uint8_t* data);

    // ========================================================================
    // Member Variables
    // ========================================================================

    /// Singleton instance pointer
    static PacketRouter* instance_;

    /// Currently active module (packets routed here)
    ILITEModule* activeModule_;

    /// Mutex for thread-safe access
    SemaphoreHandle_t mutex_;

    /// Statistics counters
    uint32_t routedCount_;   ///< Successfully routed packets
    uint32_t droppedCount_;  ///< Dropped packets (no match)
    uint32_t errorCount_;    ///< Errors (invalid packets)
};

#endif // ILITE_PACKET_ROUTER_H
