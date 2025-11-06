/**
 * @file ModuleRegistration.cpp
 * @brief Explicit module registration for platform modules
 *
 * This file forces the linker to include and register the platform modules
 * (TheGill, DroneGaze, Bulky) that are compiled into the ILITE library.
 *
 * The REGISTER_MODULE macro uses static initialization which doesn't work
 * properly when modules are in a library archive (.a file), so we explicitly
 * instantiate and register them here in the main application.
 */

#include <ILITEModule.h>
#include <ModuleRegistry.h>
#include <DisplayCanvas.h>
#include <ModuleMenu.h>
#include <IconLibrary.h>
#include <ScreenRegistry.h>
#include <InputManager.h>
#include <AudioRegistry.h>

// Include platform module headers
#include <thegill.h>
#include <drongaze.h>
#include <bulky.h>

#include <cstring>

// Forward declarations for original dashboard functions from display.cpp
extern void drawThegillDashboard();
extern void drawDrongazeDashboard();
extern void drawBulkyDashboard();

// Forward declarations for control update functions
extern void updateThegillControl();
extern void updateDrongazeControl();
extern void updateBulkyControl();

// ============================================================================
// TheGill Module
// ============================================================================

// TheGill logo (32x32 XBM - horizontal stripes)
static const uint8_t thegill_logo_32x32[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

class TheGillModule : public ILITEModule {
public:
    const char* getModuleId() const override { return "com.ilite.thegill"; }
    const char* getModuleName() const override { return "TheGill"; }
    const char* getVersion() const override { return "1.0.0"; }

    const char** getDetectionKeywords() const override {
        static const char* keywords[] = {"gill", "thegill", "tgll"};
        return keywords;
    }
    size_t getDetectionKeywordCount() const override { return 3; }
    const uint8_t* getLogo32x32() const override { return thegill_logo_32x32; }

    size_t getCommandPacketTypeCount() const override { return 1; }
    PacketDescriptor getCommandPacketDescriptor(size_t index) const override {
        if (index == 0) {
            return {"TheGill Command", THEGILL_PACKET_MAGIC, sizeof(ThegillCommand),
                    sizeof(ThegillCommand), true, nullptr, 0};
        }
        return {};
    }

    size_t getTelemetryPacketTypeCount() const override { return 1; }
    PacketDescriptor getTelemetryPacketDescriptor(size_t index) const override {
        if (index == 0) {
            return {"TheGill Telemetry", THEGILL_PACKET_MAGIC, sizeof(ThegillTelemetryPacket),
                    sizeof(ThegillTelemetryPacket), true, nullptr, 0};
        }
        return {};
    }

    void onInit() override {
        thegillCommand.magic = THEGILL_PACKET_MAGIC;
        thegillCommand.easingRate = 4.0f;
        thegillCommand.mode = GillMode::Default;
        thegillCommand.easing = kDefaultGillEasing;
        Serial.println("[TheGillModule] Initialized");
    }

    void onPair() override {
        Serial.println("[TheGillModule] Paired");
    }

    void onUnpair() override {
        Serial.println("[TheGillModule] Unpaired");
        thegillCommand.leftFront = 0;
        thegillCommand.leftRear = 0;
        thegillCommand.rightFront = 0;
        thegillCommand.rightRear = 0;
        thegillCommand.flags = 0;
    }

    void updateControl(const InputManager& inputs, float dt) override {
        updateThegillControl();
    }

    size_t prepareCommandPacket(size_t typeIndex, uint8_t* buffer, size_t bufferSize) override {
        if (typeIndex == 0 && bufferSize >= sizeof(ThegillCommand)) {
            memcpy(buffer, &thegillCommand, sizeof(ThegillCommand));
            return sizeof(ThegillCommand);
        }
        return 0;
    }

    void handleTelemetry(size_t typeIndex, const uint8_t* data, size_t length) override {
        if (typeIndex == 0 && length >= sizeof(ThegillTelemetryPacket)) {
            memcpy(&thegillTelemetryPacket, data, sizeof(ThegillTelemetryPacket));
        }
    }

    void drawDashboard(DisplayCanvas& canvas) override {
        drawThegillDashboard();
    }

    void buildModuleMenu(ModuleMenuBuilder& builder) override {
        auto& modesMenu = builder.addSubmenu("thegill.modes", "Drive Modes", ICON_ROBOT);
        addModeEntry(builder, modesMenu, GillMode::Default, "Default Mode");
        addModeEntry(builder, modesMenu, GillMode::Differential, "Mech'Iane Mode");

        auto& easingMenu = builder.addSubmenu("thegill.easing", "Easing Curve", ICON_TUNING);
        addEasingEntry(builder, easingMenu, GillEasing::None, "None");
        addEasingEntry(builder, easingMenu, GillEasing::Linear, "Linear");
        addEasingEntry(builder, easingMenu, GillEasing::EaseIn, "Ease In");
        addEasingEntry(builder, easingMenu, GillEasing::EaseOut, "Ease Out");
        addEasingEntry(builder, easingMenu, GillEasing::EaseInOut, "Ease In/Out");
        addEasingEntry(builder, easingMenu, GillEasing::Sine, "Sine");
    }

private:
    void addModeEntry(ModuleMenuBuilder& builder, ModuleMenuItem& parent, GillMode mode, const char* label) {
        builder.addAction(
            label,
            label,
            [this, mode]() { setMode(mode); },
            ICON_ROBOT,
            &parent,
            static_cast<int>(mode),
            nullptr,
            [this, mode]() { return thegillConfig.mode == mode ? "Active" : ""; });
    }

    void addEasingEntry(ModuleMenuBuilder& builder, ModuleMenuItem& parent, GillEasing easing, const char* label) {
        builder.addAction(
            label,
            label,
            [this, easing]() { setEasing(easing); },
            ICON_TUNING,
            &parent,
            static_cast<int>(easing),
            nullptr,
            [this, easing]() { return thegillConfig.easing == easing ? "Active" : ""; });
    }

    void setMode(GillMode mode) {
        if (thegillConfig.mode == mode) {
            return;
        }
        thegillConfig.mode = mode;
        thegillCommand.mode = mode;
        Serial.printf("[TheGillModule] Mode set to %d\n", static_cast<int>(mode));
    }

    void setEasing(GillEasing easing) {
        if (thegillConfig.easing == easing) {
            return;
        }
        thegillConfig.easing = easing;
        thegillCommand.easing = easing;
        Serial.printf("[TheGillModule] Easing set to %d\n", static_cast<int>(easing));
    }
};

// ============================================================================
// DroneGaze Module
// ============================================================================

// DroneGaze logo (32x32 XBM - quadcopter X pattern)
static const uint8_t drongaze_logo_32x32[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x1E, 0x00,
    0x00, 0xFC, 0x3F, 0x00, 0x00, 0xFE, 0x7F, 0x00, 0x00, 0xFF, 0xFF, 0x00,
    0x80, 0x07, 0xE0, 0x01, 0xC0, 0x03, 0xC0, 0x03, 0xE0, 0x01, 0x80, 0x07,
    0xF0, 0x00, 0x00, 0x0F, 0x78, 0x00, 0x00, 0x1E, 0x3C, 0x00, 0x00, 0x3C,
    0x1E, 0x00, 0x00, 0x78, 0x0F, 0x00, 0x00, 0xF0, 0x07, 0x00, 0x00, 0xE0,
    0x03, 0x00, 0x00, 0xC0, 0x03, 0x00, 0x00, 0xC0, 0x07, 0x00, 0x00, 0xE0,
    0x0F, 0x00, 0x00, 0xF0, 0x1E, 0x00, 0x00, 0x78, 0x3C, 0x00, 0x00, 0x3C,
    0x78, 0x00, 0x00, 0x1E, 0xF0, 0x00, 0x00, 0x0F, 0xE0, 0x01, 0x80, 0x07,
    0xC0, 0x03, 0xC0, 0x03, 0x80, 0x07, 0xE0, 0x01, 0x00, 0xFF, 0xFF, 0x00,
    0x00, 0xFE, 0x7F, 0x00, 0x00, 0xFC, 0x3F, 0x00, 0x00, 0x78, 0x1E, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Forward declarations from drongaze.cpp
extern void initDrongazeState();
extern void updateDrongazeControl();
extern void drawDrongazeDashboard();

class DrongazeModule : public ILITEModule {
public:
    const char* getModuleId() const override { return "com.ilite.drongaze"; }
    const char* getModuleName() const override { return "DroneGaze"; }
    const char* getVersion() const override { return "1.0.0"; }

    const char** getDetectionKeywords() const override {
        static const char* keywords[] = {"drongaze", "drone", "gaze", "quad"};
        return keywords;
    }
    size_t getDetectionKeywordCount() const override { return 4; }
    const uint8_t* getLogo32x32() const override { return drongaze_logo_32x32; }

    size_t getCommandPacketTypeCount() const override { return 1; }
    PacketDescriptor getCommandPacketDescriptor(size_t index) const override {
        if (index == 0) {
            return {"Drongaze Command", DRONGAZE_PACKET_MAGIC, sizeof(DrongazeCommand),
                    sizeof(DrongazeCommand), true, nullptr, 0};
        }
        return {};
    }

    size_t getTelemetryPacketTypeCount() const override { return 1; }
    PacketDescriptor getTelemetryPacketDescriptor(size_t index) const override {
        if (index == 0) {
            return {"Drongaze Telemetry", DRONGAZE_PACKET_MAGIC, sizeof(DrongazeTelemetry),
                    sizeof(DrongazeTelemetry), true, nullptr, 0};
        }
        return {};
    }

    void onInit() override {
        initDrongazeState();
        Serial.println("[DrongazeModule] Initialized");
    }

    void onPair() override { Serial.println("[DrongazeModule] Paired"); }
    void onUnpair() override {
        Serial.println("[DrongazeModule] Unpaired");
        initDrongazeState();
    }

    void updateControl(const InputManager& inputs, float dt) override {
        updateDrongazeControl();
    }

    size_t prepareCommandPacket(size_t typeIndex, uint8_t* buffer, size_t bufferSize) override {
        if (typeIndex == 0 && bufferSize >= sizeof(DrongazeCommand)) {
            memcpy(buffer, &drongazeCommand, sizeof(DrongazeCommand));
            return sizeof(DrongazeCommand);
        }
        return 0;
    }

    void handleTelemetry(size_t typeIndex, const uint8_t* data, size_t length) override {
        if (typeIndex == 0 && length >= sizeof(DrongazeTelemetry)) {
            size_t copySize = length;
            if (copySize > sizeof(drongazeTelemetry)) copySize = sizeof(drongazeTelemetry);
            memcpy(&drongazeTelemetry, data, copySize);
            drongazeState.stabilizationMask = drongazeTelemetry.stabilizationMask;
            drongazeState.stabilizationGlobal = (drongazeTelemetry.stabilizationMask & DRONGAZE_STABILIZATION_GLOBAL_BIT) != 0;
        }
    }

    void drawDashboard(DisplayCanvas& canvas) override {
        drawDrongazeDashboard();
    }
};

// ============================================================================
// Bulky Module
// ============================================================================

// Bulky logo (32x32 XBM - robot with wheels)
static const uint8_t bulky_logo_32x32[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x03, 0x00,
    0x00, 0xC0, 0x03, 0x00, 0x00, 0xF0, 0x0F, 0x00, 0x00, 0xF8, 0x1F, 0x00,
    0x00, 0xFC, 0x3F, 0x00, 0x00, 0xFE, 0x7F, 0x00, 0x00, 0xFE, 0x7F, 0x00,
    0x00, 0xFE, 0x7F, 0x00, 0x00, 0xFE, 0x7F, 0x00, 0x00, 0xFE, 0x7F, 0x00,
    0x00, 0xFE, 0x7F, 0x00, 0x00, 0xFE, 0x7F, 0x00, 0x00, 0xFE, 0x7F, 0x00,
    0x00, 0xFE, 0x7F, 0x00, 0x00, 0xFE, 0x7F, 0x00, 0x00, 0xFE, 0x7F, 0x00,
    0x00, 0xFE, 0x7F, 0x00, 0x00, 0xFE, 0x7F, 0x00, 0x00, 0xFE, 0x7F, 0x00,
    0x00, 0xFC, 0x3F, 0x00, 0x00, 0xF8, 0x1F, 0x00, 0x38, 0xF0, 0x0F, 0x1C,
    0x7C, 0x00, 0x00, 0x3E, 0xFE, 0x00, 0x00, 0x7F, 0xFE, 0x00, 0x00, 0x7F,
    0x7C, 0x00, 0x00, 0x3E, 0x38, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Forward declarations from bulky.cpp
extern void initBulkyState();
extern void updateBulkyControl();
extern void drawBulkyDashboard();
extern void handleBulkyTelemetry(const uint8_t* data, size_t length);

class BulkyModule : public ILITEModule {
public:
    const char* getModuleId() const override { return "com.ilite.bulky"; }
    const char* getModuleName() const override { return "Bulky"; }
    const char* getVersion() const override { return "1.0.0"; }

    const char** getDetectionKeywords() const override {
        static const char* keywords[] = {"bulky", "carrier", "utility"};
        return keywords;
    }
    size_t getDetectionKeywordCount() const override { return 3; }
    const uint8_t* getLogo32x32() const override { return bulky_logo_32x32; }

    size_t getCommandPacketTypeCount() const override { return 1; }
    PacketDescriptor getCommandPacketDescriptor(size_t index) const override {
        if (index == 0) {
            return {"Bulky Command", BULKY_PACKET_MAGIC, sizeof(BulkyCommand),
                    sizeof(BulkyCommand), false, nullptr, 0};
        }
        return {};
    }

    size_t getTelemetryPacketTypeCount() const override { return 1; }
    PacketDescriptor getTelemetryPacketDescriptor(size_t index) const override {
        if (index == 0) {
            return {"Bulky Telemetry", BULKY_PACKET_MAGIC, sizeof(BulkyTelemetry),
                    sizeof(BulkyTelemetry), true, nullptr, 0};
        }
        return {};
    }

    void onInit() override {
        initBulkyState();
        Serial.println("[BulkyModule] Initialized");
    }

    void onPair() override {
        Serial.println("[BulkyModule] Paired");
        bulkyState.connectionActive = true;
    }

    void onUnpair() override {
        Serial.println("[BulkyModule] Unpaired");
        bulkyState.connectionActive = false;
        initBulkyState();
    }

    void updateControl(const InputManager& inputs, float dt) override {
        updateBulkyControl();
    }

    size_t prepareCommandPacket(size_t typeIndex, uint8_t* buffer, size_t bufferSize) override {
        if (typeIndex == 0 && bufferSize >= sizeof(BulkyCommand)) {
            memcpy(buffer, &bulkyCommand, sizeof(BulkyCommand));
            return sizeof(BulkyCommand);
        }
        return 0;
    }

    void handleTelemetry(size_t typeIndex, const uint8_t* data, size_t length) override {
        if (typeIndex == 0) {
            handleBulkyTelemetry(data, length);
        }
    }

    void drawDashboard(DisplayCanvas& canvas) override {
        drawBulkyDashboard();
    }
};

// ============================================================================
// Explicit Registration (called before main)
// ============================================================================

// ============================================================================
// Built-in Module Registration Function
// ============================================================================

/**
 * @brief Register all built-in platform modules
 *
 * This function is called explicitly from ILITEFramework::begin() to ensure
 * the modules are properly linked and registered, avoiding linker issues
 * with static initialization in library archives.
 */
void registerBuiltInModules() {
    static TheGillModule theGillModule;
    static DrongazeModule drongazeModule;
    static BulkyModule bulkyModule;

    ModuleRegistry::registerModule(&theGillModule);
    ModuleRegistry::registerModule(&drongazeModule);
    ModuleRegistry::registerModule(&bulkyModule);

    Serial.println("[ILITE] Built-in modules registered: TheGill, DroneGaze, Bulky");
}

