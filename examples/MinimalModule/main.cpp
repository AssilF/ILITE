/**
 * @file main.cpp
 * @brief Example Main File - Using Custom ILITE Module
 *
 * This demonstrates the complete user experience:
 * 1. Include ILITE framework
 * 2. Include your custom modules
 * 3. Call ILITE.begin() in setup()
 * 4. Call ILITE.update() in loop()
 * 5. That's it!
 *
 * The framework handles everything else:
 * - ESP-NOW discovery and pairing
 * - Input polling and calibration
 * - Display rendering
 * - Module lifecycle management
 * - Packet routing
 * - Audio feedback
 * - Connection logging
 */

#include <Arduino.h>
#include <ILITE.h>              // ILITE Framework core

// Include your custom module(s)
#include "MinimalRobot.h"       // Your robot module

// The REGISTER_MODULE macro in MinimalRobot.cpp already registered the module.
// No need for manual setup!

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=================================");
    Serial.println("ILITE Controller Starting...");
    Serial.println("=================================\n");

    /**
     * Initialize ILITE Framework
     *
     * This single call:
     * - Initializes ESP32 hardware (WiFi, ESP-NOW, I2C, GPIO)
     * - Sets up display (OLED), inputs (joysticks, buttons, encoder)
     * - Initializes audio feedback system
     * - Discovers all registered modules
     * - Calls onInit() for each module
     * - Starts FreeRTOS tasks (display, communication)
     * - Begins ESP-NOW discovery protocol
     *
     * Configuration options (all optional):
     */
    ILITEConfig config;
    config.wifiSSID = "ILITE-AP";               // Access point SSID for OTA
    config.wifiPassword = "ilite123";           // Access point password
    config.enableOTA = true;                    // Enable over-the-air updates
    config.displayRefreshHz = 10;               // Display update rate
    config.controlLoopHz = 50;                  // Control loop frequency
    config.enableAudio = true;                  // Audio feedback enabled
    config.enableConnectionLog = true;          // Connection event logging
    config.joystickDeadzone = 0.05f;           // Joystick deadzone (5%)

    ILITE.begin(config);

    // Or use defaults:
    // ILITE.begin();

    Serial.println("\n=================================");
    Serial.println("ILITE Ready!");
    Serial.println("=================================");
    Serial.printf("Registered Modules: %zu\n", ModuleRegistry::getModuleCount());

    // List all discovered modules
    for (size_t i = 0; i < ModuleRegistry::getModuleCount(); ++i) {
        ILITEModule* module = ModuleRegistry::getModuleByIndex(i);
        Serial.printf("  [%zu] %s (%s) by %s\n",
                      i,
                      module->getModuleName(),
                      module->getVersion(),
                      module->getAuthor() ? module->getAuthor() : "Unknown");
    }

    Serial.println("\nWaiting for device pairing...");
    Serial.println("Use the controller's pairing menu to connect.");
}

void loop() {
    /**
     * Update ILITE Framework
     *
     * This single call handles:
     * - OTA update processing
     * - ESP-NOW discovery maintenance
     * - Audio feedback updates
     * - Connection timeout detection
     * - Module activation/deactivation
     * - Display mode switching
     *
     * All actual control logic runs in FreeRTOS tasks:
     * - commTask (Core 0, 50Hz): Calls module's updateControl() and transmits packets
     * - displayTask (Core 1, 10Hz): Calls module's drawDashboard() and renders to OLED
     *
     * The main loop only handles non-time-critical tasks like OTA and discovery.
     */
    ILITE.update();

    // Optional: Add your own non-time-critical code here
    // (logging, monitoring, etc.)
}

/**
 * That's it! Complete ILITE controller application in ~80 lines.
 *
 * ## What Happens at Runtime:
 *
 * 1. **Boot Sequence:**
 *    - ILITE framework initializes
 *    - MinimalRobotModule registered and initialized
 *    - Display shows boot screen → home screen
 *    - ESP-NOW begins broadcasting discovery packets
 *
 * 2. **Pairing:**
 *    - User navigates to Pairing menu
 *    - Nearby devices appear in list
 *    - User selects "MinimalBot-001"
 *    - Framework searches for keyword match: "minimal" ✓
 *    - MinimalRobotModule auto-selected
 *    - Pairing handshake completes
 *    - MinimalRobotModule::onPair() called
 *    - Dashboard switches to MinimalRobotModule
 *
 * 3. **Control Loop (50Hz):**
 *    - commTask reads joysticks via InputManager
 *    - Calls MinimalRobotModule::updateControl(inputs, dt)
 *    - Module updates command_ struct
 *    - Calls MinimalRobotModule::prepareCommandPacket()
 *    - Framework sends packet via ESP-NOW
 *    - Device sends telemetry back
 *    - Framework routes to MinimalRobotModule::handleTelemetry()
 *
 * 4. **Display Loop (10Hz):**
 *    - displayTask calls MinimalRobotModule::drawDashboard(canvas)
 *    - Canvas API draws widgets
 *    - Framework renders to OLED hardware
 *
 * 5. **User Interaction:**
 *    - User presses function button 1
 *    - Framework calls MinimalRobotModule::onFunctionButton(0)
 *    - Module toggles lights
 *    - Audio feedback plays
 *    - Dashboard updates to show new state
 *
 * 6. **Disconnection:**
 *    - Link timeout detected (no telemetry for 3 seconds)
 *    - MinimalRobotModule::onUnpair() called
 *    - Module emergency-stops motors
 *    - Display shows connection log
 *    - Discovery resumes automatically
 *
 * ## Adding More Modules:
 *
 * Want to control multiple robot types? Just add more modules!
 *
 * ```cpp
 * #include "MinimalRobot.h"
 * #include "MyDrone.h"
 * #include "MyBoat.h"
 * ```
 *
 * That's it! Each module's REGISTER_MODULE() adds it to the system.
 * The framework auto-detects which module to use based on device name.
 *
 * ## Next Steps:
 *
 * - See examples/TankDrive/ for differential drive
 * - See examples/QuadcopterBasic/ for flight controller
 * - See examples/CustomScreens/ for multi-screen UI
 * - See examples/AdvancedTelemetry/ for multiple packet types
 * - Read docs/Module_Development_Guide.md for detailed tutorial
 */
