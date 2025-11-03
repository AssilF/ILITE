/**
 * @file ILITEModule.h
 * @brief ILITE Framework - Module Base Class
 *
 * This is the core abstraction that all custom robot control modules inherit from.
 * Provides a clean API for creating extensible robot controllers without modifying
 * the ILITE core framework.
 *
 * @author ILITE Team
 * @date 2025
 * @version 2.0.0
 */

#pragma once
#include <Arduino.h>
#include <functional>
#include <vector>

// Forward declarations
class DisplayCanvas;
class InputManager;
class PreferencesManager;
class Logger;
class Audio;

/**
 * @brief Describes a packet structure for validation and routing
 */
struct PacketDescriptor {
    const char* name;           ///< Human-readable name "Flight Command"
    uint32_t magicNumber;       ///< Packet identifier (e.g., 0xA1B2C3D4)
    size_t minSize;             ///< Minimum valid packet size in bytes
    size_t maxSize;             ///< Maximum valid packet size in bytes
    bool usesMagic;             ///< Whether to validate magic number on receive

    /**
     * @brief Field definition for packet hex viewer/debugger
     */
    struct Field {
        const char* name;       ///< Field name "throttle"
        size_t offset;          ///< Byte offset in packet
        size_t size;            ///< Field size in bytes
        enum Type {
            INT8, UINT8, INT16, UINT16, INT32, UINT32, FLOAT, BOOL, BYTE_ARRAY
        } type;                 ///< Data type for display
    };

    const Field* fields;        ///< Array of field descriptors (can be nullptr)
    size_t fieldCount;          ///< Number of fields in array
};

/**
 * @brief Defines a programmable button action
 */
struct FunctionAction {
    const char* name;                   ///< Full name "Toggle Arm Motors"
    const char* shortLabel;             ///< Short label "ARM" (max 4 chars for UI)
    std::function<void()> callback;     ///< Action callback function
    bool isToggle;                      ///< Whether this is a toggle (shows on/off state)
};

/**
 * @brief Abstract base class for all ILITE control modules
 *
 * ## Overview
 * Users inherit from this class to create custom robot controllers. The ILITE
 * framework handles all low-level communication (ESP-NOW), input polling,
 * display management, and system lifecycle.
 *
 * ## Minimal Implementation Example
 * ```cpp
 * class MyRobotModule : public ILITEModule {
 * public:
 *     const char* getModuleId() const override { return "com.example.myrobot"; }
 *     const char* getModuleName() const override { return "My Robot"; }
 *     const char* getVersion() const override { return "1.0.0"; }
 *
 *     size_t getCommandPacketTypeCount() const override { return 1; }
 *     PacketDescriptor getCommandPacketDescriptor(size_t index) const override {
 *         return {"Command", 0x12345678, sizeof(MyCommand), sizeof(MyCommand), true, nullptr, 0};
 *     }
 *
 *     size_t getTelemetryPacketTypeCount() const override { return 1; }
 *     PacketDescriptor getTelemetryPacketDescriptor(size_t index) const override {
 *         return {"Telemetry", 0x87654321, sizeof(MyTelemetry), sizeof(MyTelemetry), true, nullptr, 0};
 *     }
 *
 *     void updateControl(const InputManager& inputs, float dt) override {
 *         // Read joysticks, update command packet
 *     }
 *
 *     size_t prepareCommandPacket(size_t typeIndex, uint8_t* buffer, size_t bufferSize) override {
 *         // Copy command to buffer
 *     }
 *
 *     void handleTelemetry(size_t typeIndex, const uint8_t* data, size_t length) override {
 *         // Parse telemetry
 *     }
 *
 *     void drawDashboard(DisplayCanvas& canvas) override {
 *         // Draw UI
 *     }
 * };
 * ```
 *
 * ## Module Registration
 * Use the REGISTER_MODULE macro in your .cpp file:
 * ```cpp
 * REGISTER_MODULE(MyRobotModule);
 * ```
 *
 * The framework will automatically discover and initialize your module.
 */
class ILITEModule {
public:
    // ========================================================================
    // Module Metadata (REQUIRED)
    // ========================================================================

    /**
     * @brief Get module unique identifier
     *
     * Use reverse-DNS notation: "com.yourcompany.modulename"
     * This must be unique across all modules.
     *
     * @return String identifier (max 64 chars), must be static/const
     */
    virtual const char* getModuleId() const = 0;

    /**
     * @brief Get human-readable module name for UI
     *
     * This appears in the home screen and menus.
     *
     * @return Display name (max 32 chars), must be static/const
     */
    virtual const char* getModuleName() const = 0;

    /**
     * @brief Get module version
     *
     * Use semantic versioning: "MAJOR.MINOR.PATCH"
     *
     * @return Version string (e.g., "1.2.3")
     */
    virtual const char* getVersion() const = 0;

    /**
     * @brief Get author/organization name (optional)
     *
     * @return Author name or nullptr
     */
    virtual const char* getAuthor() const { return nullptr; }

    /**
     * @brief Device name detection keywords (case-insensitive)
     *
     * When a device pairs, the framework searches its name for these keywords
     * to auto-select the appropriate module.
     *
     * Example: {"myrobot", "my-bot", "robotv2"}
     *
     * @return Null-terminated array of keyword strings, or nullptr
     */
    virtual const char** getDetectionKeywords() const { return nullptr; }

    /**
     * @brief Number of detection keywords
     *
     * @return Count of strings returned by getDetectionKeywords()
     */
    virtual size_t getDetectionKeywordCount() const { return 0; }

    // ========================================================================
    // Packet Management (REQUIRED)
    // ========================================================================

    /**
     * @brief Get number of command packet types this module sends
     *
     * Most modules use 1 command packet. Advanced modules might have multiple
     * (e.g., high-frequency control + low-frequency configuration).
     *
     * @return Count (typically 1-3)
     */
    virtual size_t getCommandPacketTypeCount() const = 0;

    /**
     * @brief Get command packet descriptor by index
     *
     * Describes packet structure for validation and debugging.
     *
     * @param index Packet type index (0 to getCommandPacketTypeCount()-1)
     * @return Packet descriptor
     */
    virtual PacketDescriptor getCommandPacketDescriptor(size_t index) const = 0;

    /**
     * @brief Get number of telemetry packet types this module expects
     *
     * @return Count (typically 1-3)
     */
    virtual size_t getTelemetryPacketTypeCount() const = 0;

    /**
     * @brief Get telemetry packet descriptor by index
     *
     * @param index Packet type index (0 to getTelemetryPacketTypeCount()-1)
     * @return Packet descriptor
     */
    virtual PacketDescriptor getTelemetryPacketDescriptor(size_t index) const = 0;

    // ========================================================================
    // Lifecycle Callbacks (OPTIONAL)
    // ========================================================================

    /**
     * @brief Called once when module is first loaded
     *
     * Use for:
     * - Initializing state variables
     * - Allocating buffers
     * - Loading preferences from EEPROM
     * - Registering callbacks
     *
     * Do NOT access hardware directly (use onActivate instead).
     */
    virtual void onInit() {}

    /**
     * @brief Called when device is paired (connected)
     *
     * Use for:
     * - Resetting control state to safe defaults
     * - Clearing error flags
     * - Starting data logging
     *
     * Called after successful ESP-NOW handshake.
     */
    virtual void onPair() {}

    /**
     * @brief Called when device is unpaired (disconnected/timeout)
     *
     * Use for:
     * - Emergency stop/safe shutdown
     * - Saving logs
     * - Releasing resources
     *
     * Called when link timeout occurs or user unpairs.
     */
    virtual void onUnpair() {}

    /**
     * @brief Called when module becomes active (user switches to it)
     *
     * Use for:
     * - Loading module-specific preferences
     * - Enabling custom input handlers
     * - Starting timers/tasks
     *
     * Multiple modules can be loaded, but only one is active.
     */
    virtual void onActivate() {}

    /**
     * @brief Called when module becomes inactive (user switches away)
     *
     * Use for:
     * - Saving module state
     * - Disabling outputs
     * - Stopping timers/tasks
     */
    virtual void onDeactivate() {}

    // ========================================================================
    // Control Loop (REQUIRED - called at 50Hz by framework)
    // ========================================================================

    /**
     * @brief Update control logic from inputs
     *
     * This is the main control loop, called at 50Hz (every 20ms).
     *
     * Typical workflow:
     * 1. Read joysticks/buttons via InputManager API
     * 2. Compute control outputs (mixing, scaling, filtering)
     * 3. Update command packet(s)
     *
     * @param inputs Reference to InputManager (provides calibrated input values)
     * @param dt Delta time since last update in seconds (typically 0.02s)
     */
    virtual void updateControl(const InputManager& inputs, float dt) = 0;

    /**
     * @brief Prepare command packet for transmission
     *
     * Fill command packet buffer. The framework sends via ESP-NOW immediately after.
     *
     * @param packetTypeIndex Which command packet type to prepare (0 to getCommandPacketTypeCount()-1)
     * @param buffer Output buffer provided by framework
     * @param bufferSize Maximum buffer size (at least getCommandPacketDescriptor().maxSize)
     * @return Actual bytes to send (0 = skip this packet this frame)
     */
    virtual size_t prepareCommandPacket(size_t packetTypeIndex,
                                       uint8_t* buffer,
                                       size_t bufferSize) = 0;

    /**
     * @brief Process incoming telemetry packet
     *
     * Parse and store telemetry data. Update internal state for display/logging.
     *
     * Called from ESP-NOW receive callback (may be different task).
     *
     * @param packetTypeIndex Which telemetry type (determined by magic number matching)
     * @param data Raw packet bytes
     * @param length Packet length in bytes
     */
    virtual void handleTelemetry(size_t packetTypeIndex,
                                const uint8_t* data,
                                size_t length) = 0;

    // ========================================================================
    // Display & UI (REQUIRED for main dashboard)
    // ========================================================================

    /**
     * @brief Render main control dashboard
     *
     * Draw to the provided canvas. Called at ~10Hz (every 100ms).
     *
     * Typical dashboard shows:
     * - Connection status
     * - Battery level
     * - Joystick positions
     * - Sensor readings
     * - Active modes/states
     *
     * Use canvas drawing primitives and widget library:
     * ```cpp
     * canvas.clear();
     * canvas.drawHeader(getModuleName());
     * canvas.drawProgressBar(10, 20, 100, 10, batteryPercent, "Battery");
     * canvas.drawGauge(64, 40, 20, speed, 0, 100, "Speed");
     * ```
     *
     * @param canvas High-level drawing API (fonts, shapes, widgets)
     */
    virtual void drawDashboard(DisplayCanvas& canvas) = 0;

    /**
     * @brief Render home screen card (module selector)
     *
     * Draw a card representing this module on the home screen.
     * Default implementation draws a simple card with module name.
     *
     * @param canvas Drawing API
     * @param x Card origin X coordinate
     * @param y Card origin Y coordinate
     * @param focused Whether this card is currently selected (highlight border)
     */
    virtual void drawLayoutCard(DisplayCanvas& canvas,
                               int16_t x, int16_t y,
                               bool focused);

    /**
     * @brief Get number of custom display screens (beyond main dashboard)
     *
     * Custom screens appear in the global menu as:
     * "Home" → "Module Name" → "Custom Screen 1" → "Custom Screen 2"
     *
     * @return Count (0 = just dashboard, 3 = dashboard + 3 custom screens)
     */
    virtual size_t getCustomScreenCount() const { return 0; }

    /**
     * @brief Get custom screen name for menu
     *
     * @param index Screen index (0 to getCustomScreenCount()-1)
     * @return Screen name (e.g., "PID Tuning", "Sensor Config")
     */
    virtual const char* getCustomScreenName(size_t index) const { return ""; }

    /**
     * @brief Render custom screen
     *
     * @param index Screen index (0 to getCustomScreenCount()-1)
     * @param canvas Drawing API
     */
    virtual void drawCustomScreen(size_t index, DisplayCanvas& canvas) {}

    // ========================================================================
    // Function Buttons (programmable actions)
    // ========================================================================

    /**
     * @brief Get number of available function button actions
     *
     * The user can assign any action to buttons 1-3. More actions than buttons
     * is fine - user cycles through them in configuration menu.
     *
     * @return Count of actions (0 = no programmable buttons)
     */
    virtual size_t getFunctionActionCount() const { return 0; }

    /**
     * @brief Get function action definition by index
     *
     * @param index Action index (0 to getFunctionActionCount()-1)
     * @return Action metadata (name, short label, callback, toggle state)
     */
    virtual FunctionAction getFunctionAction(size_t index) const {
        return FunctionAction{nullptr, nullptr, nullptr, false};
    }

    /**
     * @brief Function button pressed (optional override)
     *
     * Default implementation calls action callback from getFunctionAction().
     * Override if you need custom logic.
     *
     * @param buttonSlot Button index (0-2 for buttons 1-3)
     */
    virtual void onFunctionButton(size_t buttonSlot);

    // ========================================================================
    // Configuration & Persistence (OPTIONAL)
    // ========================================================================

    /**
     * @brief Get configuration schema (optional, for future web UI)
     *
     * JSON schema describing configurable parameters.
     * Future feature - not yet implemented.
     *
     * @return JSON schema string or nullptr
     */
    virtual const char* getConfigSchema() const { return nullptr; }

    /**
     * @brief Load configuration from JSON
     *
     * @param json Configuration JSON string
     */
    virtual void loadConfig(const char* json) {}

    /**
     * @brief Save configuration to JSON
     *
     * @param buffer Output buffer for JSON
     * @param bufferSize Max buffer size
     * @return Bytes written
     */
    virtual size_t saveConfig(char* buffer, size_t bufferSize) { return 0; }

    // ========================================================================
    // Advanced Features (OPTIONAL)
    // ========================================================================

    /**
     * @brief Handle encoder rotation (for custom screens)
     *
     * Only called when a custom screen is active.
     *
     * @param delta Rotation delta (+1 = clockwise, -1 = counterclockwise)
     */
    virtual void onEncoderRotate(int delta) {}

    /**
     * @brief Handle encoder button press (for custom screens)
     *
     * Only called when a custom screen is active.
     */
    virtual void onEncoderButton() {}

    /**
     * @brief Module has custom command processor
     *
     * If true, framework routes custom commands (sent via ESP-NOW command
     * channel) to handleCommand().
     *
     * @return true if module handles custom commands
     */
    virtual bool hasCommandProcessor() const { return false; }

    /**
     * @brief Process custom command from paired device
     *
     * Commands are strings like "pid show" or "stabilization status".
     *
     * @param command Command string (max 48 chars)
     */
    virtual void handleCommand(const char* command) {}

    /**
     * @brief Send custom command to paired device
     *
     * Uses ESP-NOW command channel (separate from telemetry).
     *
     * @param command Command string (max 48 chars)
     * @return true if sent successfully
     */
    bool sendCommand(const char* command);

    // ========================================================================
    // Framework-Provided Helper Functions
    // ========================================================================

protected:
    /**
     * @brief Get reference to InputManager
     * @return Input manager instance
     */
    InputManager& getInputs();

    /**
     * @brief Get reference to Preferences (EEPROM storage)
     * @return PreferencesManager instance
     */
    PreferencesManager& getPreferences();

    /**
     * @brief Get reference to Logger (connection log)
     * @return Logger instance
     */
    Logger& getLogger();

    /**
     * @brief Get reference to Audio feedback system
     * @return Audio instance
     */
    Audio& getAudio();

    /**
     * @brief Check if currently paired with a device
     * @return true if paired and connected
     */
    bool isPaired() const;

    /**
     * @brief Get time since last telemetry packet received
     * @return Milliseconds since last packet (0 if never received)
     */
    uint32_t getTimeSinceLastTelemetry() const;

    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~ILITEModule() = default;

private:
    friend class ModuleRegistry;
    friend class PacketRouter;
    friend class DisplayManager;

    // Framework-internal state (do not access directly)
    bool initialized_ = false;
    bool paired_ = false;
    bool active_ = false;
    uint32_t lastTelemetryTime_ = 0;
};
