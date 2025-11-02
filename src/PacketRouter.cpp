/**
 * @file PacketRouter.cpp
 * @brief Packet Router Implementation
 */

#include "PacketRouter.h"
#include "ILITEHelpers.h"
#include <cstring>

// Static instance pointer
PacketRouter* PacketRouter::instance_ = nullptr;

// ============================================================================
// Singleton Management
// ============================================================================

PacketRouter::PacketRouter()
    : activeModule_(nullptr),
      mutex_(nullptr),
      routedCount_(0),
      droppedCount_(0),
      errorCount_(0)
{
    instance_ = this;
}

PacketRouter::~PacketRouter() {
    if (mutex_ != nullptr) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
}

PacketRouter& PacketRouter::getInstance() {
    static PacketRouter instance;
    return instance;
}

// ============================================================================
// Initialization
// ============================================================================

bool PacketRouter::begin() {
    // Create mutex for thread safety
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        Logger::getInstance().error("PacketRouter: Failed to create mutex");
        return false;
    }

    Logger::getInstance().log("PacketRouter: Initialized");
    return true;
}

// ============================================================================
// Module Management
// ============================================================================

void PacketRouter::setActiveModule(ILITEModule* module) {
    if (mutex_ == nullptr) {
        Logger::getInstance().error("PacketRouter: Not initialized (call begin() first)");
        return;
    }

    // Lock for thread safety
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        activeModule_ = module;

        if (module != nullptr) {
            Logger::getInstance().logf("PacketRouter: Active module set to '%s'",
                                      module->getModuleName());
        } else {
            Logger::getInstance().log("PacketRouter: Active module cleared");
        }

        xSemaphoreGive(mutex_);
    } else {
        Logger::getInstance().error("PacketRouter: Failed to acquire mutex in setActiveModule");
    }
}

ILITEModule* PacketRouter::getActiveModule() const {
    return activeModule_;
}

// ============================================================================
// Packet Routing
// ============================================================================

bool PacketRouter::routePacket(const uint8_t* macAddr, const uint8_t* data, size_t length) {
    // Basic validation
    if (data == nullptr || length < 4) {
        // Packet too small to contain magic number
        errorCount_++;
        return false;
    }

    // Lock for thread safety (ESP-NOW callbacks run in WiFi task)
    if (mutex_ == nullptr || xSemaphoreTake(mutex_, pdMS_TO_TICKS(10)) != pdTRUE) {
        // Couldn't acquire lock quickly - drop packet to avoid blocking ESP-NOW
        droppedCount_++;
        return false;
    }

    bool routed = false;

    // Check if we have an active module
    if (activeModule_ != nullptr) {
        routed = tryRouteToModule(activeModule_, data, length);
    }

    // Update statistics
    if (routed) {
        routedCount_++;
    } else {
        droppedCount_++;
    }

    xSemaphoreGive(mutex_);
    return routed;
}

bool PacketRouter::tryRouteToModule(ILITEModule* module, const uint8_t* data, size_t length) {
    // Extract magic number from packet
    uint32_t packetMagic = extractMagicNumber(data);

    // Get telemetry packet count from module
    size_t telemetryCount = module->getTelemetryPacketTypeCount();

    // Try to match against each telemetry descriptor
    for (size_t i = 0; i < telemetryCount; ++i) {
        PacketDescriptor desc = module->getTelemetryPacketDescriptor(i);

        // Check if magic number matches
        if (desc.magicNumber != packetMagic) {
            continue;  // Not this packet type
        }

        // Validate packet size
        if (length < desc.minSize || length > desc.maxSize) {
            Logger::getInstance().logf(
                "PacketRouter: Size mismatch for '%s' packet type %zu (got %u, expected %u-%u)",
                module->getModuleName(), i, length, desc.minSize, desc.maxSize
            );
            errorCount_++;
            return false;
        }

        // Valid packet - route to module
        module->handleTelemetry(i, data, length);

        // Log first packet of each type (for debugging)
        static uint32_t loggedTypes[8] = {0};  // Track first 8 types
        if (i < 8 && loggedTypes[i] == 0) {
            Logger::getInstance().logf(
                "PacketRouter: First '%s' packet (type %zu, magic 0x%08X, %u bytes)",
                desc.name, i, packetMagic, length
            );
            loggedTypes[i] = 1;
        }

        return true;
    }

    // No matching descriptor found
    Logger::getInstance().logf(
        "PacketRouter: No match for magic 0x%08X in module '%s' (%zu telemetry types)",
        packetMagic, module->getModuleName(), telemetryCount
    );

    return false;
}

uint32_t PacketRouter::extractMagicNumber(const uint8_t* data) {
    // Read first 4 bytes as little-endian uint32_t
    uint32_t magic;
    memcpy(&magic, data, sizeof(magic));
    return magic;
}

// ============================================================================
// Statistics
// ============================================================================

uint32_t PacketRouter::getRoutedCount() const {
    return routedCount_;
}

uint32_t PacketRouter::getDroppedCount() const {
    return droppedCount_;
}

uint32_t PacketRouter::getErrorCount() const {
    return errorCount_;
}

void PacketRouter::resetStats() {
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        routedCount_ = 0;
        droppedCount_ = 0;
        errorCount_ = 0;
        xSemaphoreGive(mutex_);
    }
}
