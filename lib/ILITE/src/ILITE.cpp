/**
 * @file ILITE.cpp
 * @brief ILITE Framework Implementation
 */
#include "Arduino.h"
#include "ILITE.h"
#include "espnow_discovery.h"
#include "audio_feedback.h"
#include "input.h"
#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoOTA.h>
#include <Wire.h>

// Extension systems (optional)
#include "IconLibrary.h"
#include "AudioRegistry.h"
#include "MenuRegistry.h"
#include "ScreenRegistry.h"
#include "ControlBindingSystem.h"
#include "FrameworkEngine.h"
#include "connection_log.h"

// ============================================================================
// Global Instances
// ============================================================================

// Discovery instance (used globally throughout the framework)
EspNowDiscovery discovery;

// ============================================================================
// Static Instance Management
// ============================================================================

ILITEFramework* ILITEFramework::instance_ = nullptr;

ILITEFramework& ILITEFramework::getInstance() {
    static ILITEFramework instance;
    return instance;
}

// Global ILITE instance for user access
ILITEFramework& ILITE = ILITEFramework::getInstance();

// ============================================================================
// Constructor
// ============================================================================

ILITEFramework::ILITEFramework()
    : config_(),
      initialized_(false),
      bootTime_(0),
      activeModule_(nullptr),
      previousModule_(nullptr),
      paired_(false),
      lastTelemetryTime_(0),
      packetTxCount_(0),
      packetRxCount_(0),
      commTaskHandle_(nullptr),
      displayTaskHandle_(nullptr),
      u8g2_(nullptr),
      displayCanvas_(nullptr),
      discovery_(nullptr),
      frameworkEngine_(&FrameworkEngine::getInstance())
{
    instance_ = this;
}

// ============================================================================
// Main Initialization
// ============================================================================

bool ILITEFramework::begin(const ILITEConfig& config) {
    // Store configuration
    config_ = config;
    bootTime_ = millis();

    Serial.println("=====================================");
    Serial.println("   ILITE Framework v1.0.0");
    Serial.println("=====================================");

    // Step 1: Initialize hardware
    Serial.println("\n[1/7] Initializing hardware...");
    if (!initHardware()) {
        Serial.println("ERROR: Hardware initialization failed!");
        return false;
    }
    Serial.println("  ✓ Hardware initialized");

    // Initialize connection log
    connectionLogInit();
    connectionLogSetRecordingEnabled(true);
    connectionLogAdd("[ILITE] Framework starting...");
    connectionLogAdd("[ILITE] Hardware initialized");

    // Step 2: Initialize packet router
    Serial.println("\n[2/7] Initializing packet router...");
    if (!PacketRouter::getInstance().begin()) {
        Serial.println("ERROR: PacketRouter initialization failed!");
        return false;
    }
    Serial.println("  ✓ Packet router initialized");

    // Step 3: Register and initialize modules
    Serial.println("\n[3/7] Initializing modules...");
    registerBuiltInModules();
    if (!initModules()) {
        Serial.println("ERROR: Module initialization failed!");
        return false;
    }
    Serial.printf("  ✓ %zu modules initialized\n", ModuleRegistry::getModuleCount());

    // Step 4: Create FreeRTOS tasks
    Serial.println("\n[4/7] Creating FreeRTOS tasks...");
    if (!createTasks()) {
        Serial.println("ERROR: Task creation failed!");
        return false;
    }
    Serial.println("  ✓ Control and display tasks created");

    // Step 5: Initialize OTA if enabled
    if (config_.enableOTA) {
        Serial.println("\n[5/7] Initializing OTA...");
        if (!initOTA()) {
            Serial.println("WARNING: OTA initialization failed (continuing anyway)");
        } else {
            Serial.println("  ✓ OTA initialized");
        }
    } else {
        Serial.println("\n[5/7] OTA disabled (skipped)");
    }

    // Step 6: Start discovery
    if (config_.enableDiscovery) {
        Serial.println("\n[6/7] Starting ESP-NOW discovery...");
        discovery.setDiscoveryEnabled(true);
        Serial.println("  ✓ Discovery enabled");
    } else {
        Serial.println("\n[6/7] Discovery disabled (skipped)");
    }

    // Step 7: Initialize extension systems (optional, non-breaking)
    Serial.println("\n[7/7] Initializing extension systems...");
    IconLibrary::initBuiltInIcons();
    MenuRegistry::initBuiltInMenus();
    ControlBindingSystem::begin();
    Serial.println("  ✓ Extension systems initialized");

    initialized_ = true;

    Serial.println("\n=====================================");
    Serial.println("   ILITE Framework Ready!");
    Serial.println("=====================================");
    Serial.printf("Uptime: %lu ms\n", getUptimeMs());
    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Control loop: %u Hz\n", config_.controlLoopHz);
    Serial.printf("Display refresh: %u Hz\n", config_.displayRefreshHz);
    Serial.println();

    return true;
}

// ============================================================================
// Hardware Initialization
// ============================================================================

bool ILITEFramework::initHardware() {
    // Initialize WiFi for ESP-NOW
    Serial.println("  - WiFi...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);

    if (config_.enableOTA) {
        WiFi.softAP(config_.wifiSSID, config_.wifiPassword);
        Serial.printf("    AP: %s\n", config_.wifiSSID);
    }

    // Initialize ESP-NOW
    Serial.println("  - ESP-NOW...");
    if (esp_now_init() != ESP_OK) {
        Serial.println("    ERROR: ESP-NOW init failed");
        return false;
    }

    // Initialize discovery system
    Serial.println("  - Discovery protocol...");
    discovery.begin();
    discovery_ = &discovery;

    // Initialize GPIO for inputs
    Serial.println("  - Input GPIOs...");
    initInput();  // From input.h/cpp

    // Initialize InputManager
    Serial.println("  - InputManager...");
    InputManager& inputMgr = InputManager::getInstance();
    inputMgr.setJoystickDeadzone(config_.joystickDeadzone);
    inputMgr.setJoystickSensitivity(config_.joystickSensitivity);
    inputMgr.setJoystickFiltering(config_.joystickFiltering);

    // Initialize I2C for display
    Serial.println("  - I2C bus...");
    Wire.begin();

    // Initialize U8G2 display (SH1106 128x64 OLED)
    Serial.println("  - OLED display...");
    u8g2_ = new U8G2_SH1106_128X64_NONAME_F_HW_I2C(
        U8G2_R0,  // Rotation
        /* reset=*/ U8X8_PIN_NONE
    );

    if (!u8g2_->begin()) {
        Serial.println("    ERROR: Display init failed");
        return false;
    }

    u8g2_->setContrast(config_.displayContrast);

    // Initialize DisplayCanvas wrapper
    Serial.println("  - DisplayCanvas...");
    displayCanvas_ = new DisplayCanvas(*u8g2_);

    // Draw boot screen
    displayCanvas_->clear();
    displayCanvas_->setFont(DisplayCanvas::NORMAL);
    displayCanvas_->drawTextCentered(28, "ILITE Framework");
    displayCanvas_->setFont(DisplayCanvas::SMALL);
    displayCanvas_->drawTextCentered(40, "Initializing...");
    displayCanvas_->sendBuffer();

    // Initialize audio
    if (config_.enableAudio) {
        Serial.println("  - Audio feedback...");
        audioSetup();
        audioPlayStartup();
    }

    // Initialize Framework Engine (v2.0)
    Serial.println("  - FrameworkEngine...");
    frameworkEngine_->begin();

    return true;
}

// ============================================================================
// Module Initialization
// ============================================================================

bool ILITEFramework::initModules() {
    size_t moduleCount = ModuleRegistry::getModuleCount();

    if (moduleCount == 0) {
        Serial.println("  WARNING: No modules registered!");
        return true;  // Not an error, just no modules
    }

    // Call onInit() for each registered module
    for (size_t i = 0; i < moduleCount; ++i) {
        ILITEModule* module = ModuleRegistry::getModuleByIndex(i);

        Serial.printf("  - Module %zu: %s v%s\n",
                     i + 1,
                     module->getModuleName(),
                     module->getVersion());

        module->onInit();
    }

    return true;
}

// ============================================================================
// Task Creation
// ============================================================================

bool ILITEFramework::createTasks() {
    // Create communication task (Core 0, high priority)
    BaseType_t result = xTaskCreatePinnedToCore(
        commTask,                          // Task function
        "CommTask",                        // Name
        4096,                              // Stack size
        this,                              // Parameter (this instance)
        2,                                 // Priority (high)
        &commTaskHandle_,                  // Handle
        0                                  // Core 0
    );

    if (result != pdPASS) {
        Serial.println("  ERROR: Failed to create CommTask");
        return false;
    }

    Serial.println("  - CommTask created (Core 0, Priority 2)");

    // Create display task (Core 1, lower priority)
    result = xTaskCreatePinnedToCore(
        displayTask,                       // Task function
        "DisplayTask",                     // Name
        4096,                              // Stack size
        this,                              // Parameter (this instance)
        1,                                 // Priority (lower)
        &displayTaskHandle_,               // Handle
        1                                  // Core 1
    );

    if (result != pdPASS) {
        Serial.println("  ERROR: Failed to create DisplayTask");
        return false;
    }

    Serial.println("  - DisplayTask created (Core 1, Priority 1)");

    return true;
}

// ============================================================================
// OTA Initialization
// ============================================================================

bool ILITEFramework::initOTA() {
    ArduinoOTA.setHostname(config_.wifiSSID);

    ArduinoOTA.onStart([]() {
        Serial.println("OTA: Starting update...");
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA: Update complete!");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA: Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA: Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    ArduinoOTA.begin();

    return true;
}

// ============================================================================
// Control Loop Task (50Hz default)
// ============================================================================

void ILITEFramework::commTask(void* parameter) {
    ILITEFramework* framework = static_cast<ILITEFramework*>(parameter);
    const TickType_t period = pdMS_TO_TICKS(1000 / framework->config_.controlLoopHz);
    TickType_t lastWakeTime = xTaskGetTickCount();

    uint32_t lastLoopTime = millis();

    Serial.println("CommTask: Started");

    while (true) {
        uint32_t now = millis();
        float dt = (now - lastLoopTime) / 1000.0f;
        lastLoopTime = now;

        // Update Framework Engine (button events, encoder, etc)
        framework->frameworkEngine_->update();

        // Get active module
        ILITEModule* module = framework->activeModule_;

        // Update control scheme when module is loaded (even if not paired)
        if (module != nullptr) {
            // Read inputs
            InputManager& inputs = InputManager::getInstance();
            inputs.update();

            // Call module's control loop (runs regardless of pairing for testing)
            module->updateControl(inputs, dt);

            // Only send command packets if paired
            if (framework->paired_) {
                size_t packetTypeCount = module->getCommandPacketTypeCount();
                for (size_t i = 0; i < packetTypeCount; ++i) {
                    PacketDescriptor desc = module->getCommandPacketDescriptor(i);

                    uint8_t buffer[256];  // Max ESP-NOW packet size = 250
                    size_t packetSize = module->prepareCommandPacket(i, buffer, sizeof(buffer));

                    if (packetSize > 0 && packetSize <= desc.maxSize) {
                        // Send via ESP-NOW
                        const uint8_t* peerMac = framework->discovery_->getPairedMac();
                        if (peerMac != nullptr) {
                            esp_now_send(peerMac, buffer, packetSize);
                            framework->packetTxCount_++;
                        }
                    }
                }
            }
        }

        // Wait for next period
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// ============================================================================
// Display Rendering Task (10Hz default)
// ============================================================================

void ILITEFramework::displayTask(void* parameter) {
    ILITEFramework* framework = static_cast<ILITEFramework*>(parameter);
    const TickType_t period = pdMS_TO_TICKS(1000 / framework->config_.displayRefreshHz);
    TickType_t lastWakeTime = xTaskGetTickCount();

    Serial.println("DisplayTask: Started");

    while (true) {
        DisplayCanvas& canvas = *framework->displayCanvas_;
        ILITEModule* module = framework->activeModule_;

        // Update and draw custom screen if active (extension system)
        if (ScreenRegistry::hasActiveScreen()) {
            ScreenRegistry::updateActiveScreen();
            ScreenRegistry::drawActiveScreen(canvas);
        } else {
            // Use FrameworkEngine v2.0 rendering system
            canvas.clear();
            framework->frameworkEngine_->render(canvas);
        }

        canvas.sendBuffer();

        // Wait for next period
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// ============================================================================
// Main Loop Update
// ============================================================================

void ILITEFramework::update() {
    if (!initialized_) {
        return;
    }

    // Handle OTA updates
    if (config_.enableOTA) {
        handleOTA();
    }

    // Handle discovery protocol
    if (config_.enableDiscovery) {
        handleDiscovery();
    }

    // Handle pairing state
    handlePairing();

    // Handle connection timeout
    if (paired_) {
        handleConnectionTimeout();
    }

    // Update control bindings (extension system)
    ControlBindingSystem::update();

    // Update audio feedback
    if (config_.enableAudio) {
        audioUpdate();
    }
}

// ============================================================================
// Runtime Helpers
// ============================================================================

void ILITEFramework::handleOTA() {
    ArduinoOTA.handle();
}

void ILITEFramework::handleDiscovery() {
    discovery.discover();
}

void ILITEFramework::handlePairing() {
    // Check if we just became paired
    if (discovery.isPaired() && !paired_) {
        paired_ = true;
        frameworkEngine_->setPaired(true);  // Sync with FrameworkEngine
        lastTelemetryTime_ = millis();

        const Identity* peerIdentity = discovery.getPairedIdentity();
        if (peerIdentity != nullptr) {
            Logger::getInstance().logf("Paired with: %s", peerIdentity->customId);

            // Try to find matching module by keywords
            ILITEModule* matchedModule = ModuleRegistry::findModuleByName(peerIdentity->customId);

            if (matchedModule != nullptr) {
                setActiveModule(matchedModule);
                Logger::getInstance().logf("Auto-selected module: %s",
                                          matchedModule->getModuleName());
            } else {
                Logger::getInstance().log("No matching module found for device");
            }

            if (config_.enableAudio) {
                audioFeedback(AudioCue::PeerAcknowledge);
            }
        }
    }

    // Check if we just became unpaired
    if (!discovery.isPaired() && paired_) {
        Logger::getInstance().log("Unpaired from device");
        unpair();
    }
}

void ILITEFramework::handleConnectionTimeout() {
    if (lastTelemetryTime_ > 0) {
        uint32_t timeSinceLastPacket = millis() - lastTelemetryTime_;

        if (timeSinceLastPacket > config_.connectionTimeoutMs) {
            Logger::getInstance().logf("Connection timeout (%lu ms)", timeSinceLastPacket);
            unpair();
        }
    }
}

// ============================================================================
// Module Management
// ============================================================================

void ILITEFramework::setActiveModule(ILITEModule* module) {
    // Deactivate previous module
    if (previousModule_ != nullptr && previousModule_ != module) {
        previousModule_->onDeactivate();
        Logger::getInstance().logf("Deactivated: %s", previousModule_->getModuleName());
    }

    // Set new active module
    activeModule_ = module;
    previousModule_ = activeModule_;

    // Sync with FrameworkEngine
    frameworkEngine_->loadModule(module);

    // Update packet router
    PacketRouter::getInstance().setActiveModule(module);

    // Activate new module
    if (module != nullptr) {
        module->onActivate();
        Logger::getInstance().logf("Activated: %s", module->getModuleName());

        // Call onPair if we're already paired
        if (paired_) {
            module->onPair();
        }
    }
}

ILITEModule* ILITEFramework::getActiveModule() const {
    return activeModule_;
}

bool ILITEFramework::activateModuleById(const char* moduleId) {
    ILITEModule* module = ModuleRegistry::findModuleById(moduleId);
    if (module != nullptr) {
        setActiveModule(module);
        return true;
    }
    return false;
}

bool ILITEFramework::activateModuleByName(const char* moduleName) {
    ILITEModule* module = ModuleRegistry::findModuleByName(moduleName);
    if (module != nullptr) {
        setActiveModule(module);
        return true;
    }
    return false;
}

// ============================================================================
// Pairing Management
// ============================================================================

bool ILITEFramework::isPaired() const {
    return paired_;
}

uint32_t ILITEFramework::getTimeSinceLastTelemetry() const {
    if (lastTelemetryTime_ == 0) {
        return 0;
    }
    return millis() - lastTelemetryTime_;
}

void ILITEFramework::unpair() {
    if (activeModule_ != nullptr) {
        activeModule_->onUnpair();
    }

    paired_ = false;
    frameworkEngine_->setPaired(false);  // Sync with FrameworkEngine
    lastTelemetryTime_ = 0;
    discovery.resetLinkState();

    Logger::getInstance().log("Unpairing complete");
}

// ============================================================================
// State Queries
// ============================================================================

uint32_t ILITEFramework::getUptimeMs() const {
    return millis() - bootTime_;
}

const ILITEConfig& ILITEFramework::getConfig() const {
    return config_;
}

EspNowDiscovery& ILITEFramework::getDiscovery() {
    return *discovery_;
}

bool ILITEFramework::isInitialized() const {
    return initialized_;
}

// ============================================================================
// Statistics
// ============================================================================

uint32_t ILITEFramework::getPacketTxCount() const {
    return packetTxCount_;
}

uint32_t ILITEFramework::getPacketRxCount() const {
    return packetRxCount_;
}
