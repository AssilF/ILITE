/**
 * @file ILITE.h
 * @brief ILITE Framework - Main Entry Point
 *
 * This is the primary header users include to access the ILITE framework.
 * It provides a simple begin()/update() interface that initializes all
 * subsystems and manages the control loop.
 *
 * ## Minimal Usage:
 * ```cpp
 * #include <ILITE.h>
 * #include "MyRobot.h"  // Your custom module
 *
 * void setup() {
 *     ILITE.begin();
 * }
 *
 * void loop() {
 *     ILITE.update();
 * }
 * ```
 *
 * ## Configured Usage:
 * ```cpp
 * ILITEConfig config;
 * config.wifiSSID = "MyController";
 * config.enableOTA = true;
 * config.controlLoopHz = 100;  // 100Hz control rate
 * ILITE.begin(config);
 * ```
 *
 * @author ILITE Framework
 * @version 1.0.0
 */

#ifndef ILITE_FRAMEWORK_H
#define ILITE_FRAMEWORK_H

#include <Arduino.h>
#include <U8G2lib.h>
#include "ILITEModule.h"
#include "ModuleRegistry.h"
#include "InputManager.h"
#include "DisplayCanvas.h"
#include "PacketRouter.h"
#include "ILITEHelpers.h"
#include "AudioRegistry.h"

// Forward declarations for existing subsystems
class EspNowDiscovery;
class AudioFeedback;

/**
 * @struct ILITEConfig
 * @brief Configuration options for ILITE framework initialization
 *
 * All fields are optional and have sensible defaults.
 */
struct ILITEConfig {
    // ========================================================================
    // Network Configuration
    // ========================================================================

    /// WiFi SSID for access point mode (used for OTA updates)
    const char* wifiSSID = "ILITE-Controller";

    /// WiFi password for access point (minimum 8 characters)
    const char* wifiPassword = "ilite2024";

    /// Enable over-the-air (OTA) updates via WiFi
    bool enableOTA = true;

    // ========================================================================
    // Timing Configuration
    // ========================================================================

    /// Display refresh rate in Hz (default 10Hz = 100ms per frame)
    uint8_t displayRefreshHz = 10;

    /// Control loop frequency in Hz (default 50Hz = 20ms per iteration)
    uint8_t controlLoopHz = 50;

    /// Connection timeout in milliseconds (default 3000ms = 3 seconds)
    uint32_t connectionTimeoutMs = 3000;

    // ========================================================================
    // Input Configuration
    // ========================================================================

    /// Joystick deadzone (0.0 to 1.0, default 0.05 = 5%)
    float joystickDeadzone = 0.05f;

    /// Joystick sensitivity multiplier (default 1.0)
    float joystickSensitivity = 1.0f;

    /// Enable joystick low-pass filtering
    bool joystickFiltering = true;

    // ========================================================================
    // Feature Flags
    // ========================================================================

    /// Enable audio feedback (beeps on button press, pairing, etc.)
    bool enableAudio = true;

    /// Enable connection event logging
    bool enableConnectionLog = true;

    /// Enable automatic discovery broadcasting
    bool enableDiscovery = true;

    // ========================================================================
    // Display Configuration
    // ========================================================================

    /// I2C address for OLED display (default 0x3C for SH1106)
    uint8_t displayI2CAddress = 0x3C;

    /// Display rotation (0, 90, 180, 270 degrees)
    uint16_t displayRotation = 0;

    /// Display contrast (0-255, default 128)
    uint8_t displayContrast = 128;

    // ========================================================================
    // Debug Configuration
    // ========================================================================

    /// Enable serial logging (already initialized by user)
    bool enableSerialLogging = true;

    /// Serial baud rate (if framework should initialize Serial)
    uint32_t serialBaudRate = 115200;
};

/**
 * @class ILITEFramework
 * @brief Main framework class providing initialization and runtime management
 *
 * Singleton class that orchestrates all ILITE subsystems. Users interact with
 * this class through the global `ILITE` instance.
 *
 * ## Responsibilities:
 * - Hardware initialization (ESP-NOW, Display, Input, Audio)
 * - Module discovery and lifecycle management
 * - FreeRTOS task creation (control loop, display rendering)
 * - Discovery and pairing protocols
 * - Connection timeout detection
 * - OTA update handling
 *
 * ## Architecture:
 * ```
 * Main Loop (loop())
 *     └─> ILITE.update()
 *         ├─> OTA handling
 *         ├─> Discovery updates
 *         └─> Timeout detection
 *
 * Control Task (Core 0, 50Hz)
 *     └─> commTask()
 *         ├─> Read inputs
 *         ├─> module->updateControl()
 *         ├─> module->preparePacket()
 *         └─> Transmit via ESP-NOW
 *
 * Display Task (Core 1, 10Hz)
 *     └─> displayTask()
 *         ├─> module->drawDashboard()
 *         └─> Render to OLED
 * ```
 */
class ILITEFramework {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the ILITEFramework instance
     */
    static ILITEFramework& getInstance();

    /**
     * @brief Initialize ILITE framework with optional configuration
     *
     * This must be called once in setup() before any other framework operations.
     * Initializes all hardware, discovers modules, creates tasks, and begins
     * discovery protocol.
     *
     * @param config Configuration struct (optional, uses defaults if not provided)
     * @return true if initialization succeeded, false on error
     *
     * ## Initialization Sequence:
     * 1. Initialize hardware (WiFi, ESP-NOW, I2C, GPIO, Display)
     * 2. Initialize subsystems (Input, Display, Audio, Logger)
     * 3. Initialize all registered modules (call onInit())
     * 4. Create FreeRTOS tasks (control, display)
     * 5. Start discovery protocol
     * 6. Initialize OTA if enabled
     *
     * @note If initialization fails, check Serial output for error messages
     */
    bool begin(const ILITEConfig& config = ILITEConfig());

    /**
     * @brief Update framework state (call in loop())
     *
     * Handles non-time-critical tasks that don't need precise timing:
     * - OTA update processing
     * - Discovery protocol maintenance
     * - Connection timeout detection
     * - Audio feedback updates
     *
     * @note Time-critical control and display operations run in dedicated
     *       FreeRTOS tasks, not in this function.
     */
    void update();

    // ========================================================================
    // Module Management
    // ========================================================================

    /**
     * @brief Set the currently active module
     *
     * Activates a module by calling its onActivate() hook, deactivates the
     * previous module, and updates the packet router.
     *
     * @param module Pointer to module to activate (nullptr to deactivate all)
     */
    void setActiveModule(ILITEModule* module);

    /**
     * @brief Get the currently active module
     * @return Pointer to active module, or nullptr if none active
     */
    ILITEModule* getActiveModule() const;

    /**
     * @brief Activate a module by ID
     * @param moduleId Module identifier string
     * @return true if module found and activated, false otherwise
     */
    bool activateModuleById(const char* moduleId);

    /**
     * @brief Activate a module by name
     * @param moduleName Module name string
     * @return true if module found and activated, false otherwise
     */
    bool activateModuleByName(const char* moduleName);

    // ========================================================================
    // Pairing Management
    // ========================================================================

    /**
     * @brief Check if controller is paired with a device
     * @return true if paired and receiving telemetry, false otherwise
     */
    bool isPaired() const;

    /**
     * @brief Get time since last telemetry packet received
     * @return Milliseconds since last packet (0 if never paired)
     */
    uint32_t getTimeSinceLastTelemetry() const;

    /**
     * @brief Unpair from current device
     *
     * Calls active module's onUnpair(), clears connection, and resumes discovery.
     */
    void unpair();

    // ========================================================================
    // State Queries
    // ========================================================================

    /**
     * @brief Get framework uptime
     * @return Milliseconds since begin() was called
     */
    uint32_t getUptimeMs() const;

    /**
     * @brief Get current configuration
     * @return Reference to active configuration struct
     */
    const ILITEConfig& getConfig() const;

    /**
     * @brief Check if framework is initialized
     * @return true if begin() completed successfully, false otherwise
     */
    bool isInitialized() const;

    // ========================================================================
    // Statistics
    // ========================================================================

    /**
     * @brief Get packet transmit count
     * @return Number of packets sent via ESP-NOW since boot
     */
    uint32_t getPacketTxCount() const;

    /**
     * @brief Get packet receive count
     * @return Number of packets received via ESP-NOW since boot
     */
    uint32_t getPacketRxCount() const;

private:
    /**
     * @brief Private constructor (singleton pattern)
     */
    ILITEFramework();

    // Prevent copying
    ILITEFramework(const ILITEFramework&) = delete;
    ILITEFramework& operator=(const ILITEFramework&) = delete;

    // ========================================================================
    // Initialization Helpers
    // ========================================================================

    /**
     * @brief Initialize hardware subsystems
     * @return true if successful, false on error
     */
    bool initHardware();

    /**
     * @brief Initialize all registered modules
     * @return true if successful, false on error
     */
    bool initModules();

    /**
     * @brief Create FreeRTOS tasks for control and display
     * @return true if successful, false on error
     */
    bool createTasks();

    /**
     * @brief Initialize OTA update system
     * @return true if successful, false on error
     */
    bool initOTA();

    // ========================================================================
    // Task Functions (Static for FreeRTOS)
    // ========================================================================

    /**
     * @brief Control loop task (Core 0, high priority)
     *
     * Runs at configured Hz (default 50Hz). Reads inputs, calls module's
     * updateControl(), prepares packets, and transmits via ESP-NOW.
     *
     * @param parameter Pointer to ILITEFramework instance
     */
    static void commTask(void* parameter);

    /**
     * @brief Display rendering task (Core 1, lower priority)
     *
     * Runs at configured Hz (default 10Hz). Calls module's drawDashboard()
     * and renders to OLED hardware.
     *
     * @param parameter Pointer to ILITEFramework instance
     */
    static void displayTask(void* parameter);

    // ========================================================================
    // Runtime Helpers
    // ========================================================================

    /**
     * @brief Handle discovery protocol updates
     */
    void handleDiscovery();

    /**
     * @brief Handle pairing state machine
     */
    void handlePairing();

    /**
     * @brief Check for connection timeout
     */
    void handleConnectionTimeout();

    /**
     * @brief Handle OTA updates
     */
    void handleOTA();

    // ========================================================================
    // Member Variables
    // ========================================================================

    /// Singleton instance pointer
    static ILITEFramework* instance_;

    /// Configuration
    ILITEConfig config_;

    /// Initialization state
    bool initialized_;

    /// Boot timestamp
    uint32_t bootTime_;

    /// Currently active module
    ILITEModule* activeModule_;

    /// Previous active module (for deactivation)
    ILITEModule* previousModule_;

    /// Pairing state
    bool paired_;
    uint32_t lastTelemetryTime_;

    /// Packet statistics
    uint32_t packetTxCount_;
    uint32_t packetRxCount_;

    /// Task handles
    TaskHandle_t commTaskHandle_;
    TaskHandle_t displayTaskHandle_;

    /// Hardware subsystem pointers
    U8G2* u8g2_;
    DisplayCanvas* displayCanvas_;
    EspNowDiscovery* discovery_;
};

/**
 * @brief Global ILITE framework instance
 *
 * Users access the framework through this global instance:
 * ```cpp
 * ILITE.begin();
 * ILITE.update();
 * ```
 */
extern ILITEFramework& ILITE;

#endif // ILITE_FRAMEWORK_H
