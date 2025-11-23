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
#include <UIComponents.h>
#include <FrameworkEngine.h>
#include <ILITE.h>
#include <espnow_discovery.h>
#include <connection_log.h>
#include <strings.h>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {
std::vector<std::string> g_moduleMenuEntryIds;

template <typename T>
T clampValue(T value, T minValue, T maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}
}

bool controlSessionActive = false;
uint8_t targetAddress[6] = {0};

// Include platform module headers
#include <ControlBindingSystem.h>
#include <thegill.h>
#include <drongaze.h>
#include <bulky.h>

#include <cstring>

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

    size_t getCommandPacketTypeCount() const override { return 4; }
    PacketDescriptor getCommandPacketDescriptor(size_t index) const override {
        if (index == 0) {
            return {"TheGill Command", THEGILL_PACKET_MAGIC, sizeof(ThegillCommand),
                    sizeof(ThegillCommand), true, nullptr, 0};
        }
        if (index == 1) {
            return {"Peripheral Command", THEGILL_PERIPHERAL_MAGIC, sizeof(PeripheralCommand),
                    sizeof(PeripheralCommand), true, nullptr, 0};
        }
        if (index == 2) {
            return {"Configuration Packet", THEGILL_CONFIG_MAGIC, sizeof(ConfigurationPacket),
                    sizeof(ConfigurationPacket), true, nullptr, 0};
        }
        if (index == 3) {
            return {"Arm Command", ARM_COMMAND_MAGIC, sizeof(ArmControlCommand),
                    sizeof(ArmControlCommand), true, nullptr, 0};
        }
        return {};
    }

    size_t getTelemetryPacketTypeCount() const override { return 2; }
    PacketDescriptor getTelemetryPacketDescriptor(size_t index) const override {
        if (index == 0) {
            return {"Status Packet", THEGILL_STATUS_MAGIC, sizeof(StatusPacket),
                    sizeof(StatusPacket), true, nullptr, 0};
        }
        if (index == 1) {
            return {"Arm State Packet", THEGILL_ARM_STATE_MAGIC, sizeof(ArmStatePacket),
                    sizeof(ArmStatePacket), true, nullptr, 0};
        }
        return {};
    }

    bool hasCommandProcessor() const override { return true; }
    void handleCommand(const char* command) override {
        if (command == nullptr) {
            return;
        }

        String cmd = command;
        cmd.trim();
        cmd.toLowerCase();

        if (cmd == "status") {
            connectionLogAddf("[TheGill] Mode=%s, Mech Mode=%d",
                              profileLabel(thegillConfig.profile),
                              static_cast<int>(mechIaneMode));
            return;
        }

        if (cmd == "mode drive") {
            mechIaneMode = MechIaneMode::DriveMode;
            connectionLogAdd("[TheGill] Switched to Drive mode");
            return;
        }

        if (cmd == "mode arm" || cmd == "mode xyz") {
            mechIaneMode = MechIaneMode::ArmXYZ;
            connectionLogAdd("[TheGill] Switched to Arm XYZ mode");
            return;
        }

        if (cmd == "mode ori") {
            mechIaneMode = MechIaneMode::ArmOrientation;
            connectionLogAdd("[TheGill] Switched to Orientation mode");
            return;
        }

        if (cmd == "precision on") {
            setPrecisionMode(true);
            connectionLogAdd("[TheGill] Precision mode ON");
            return;
        }

        if (cmd == "precision off") {
            setPrecisionMode(false);
            connectionLogAdd("[TheGill] Precision mode OFF");
            return;
        }

        connectionLogAddf("[TheGill] Unknown command: %s", command);
    }

    void onInit() override {
        onThegillUnpaired();
        markThegillConfigDirty();
        Serial.println("[TheGillModule] Initialized");
    }

    void onPair() override {
        Serial.println("[TheGillModule] Paired");
        onThegillPaired();
    }

    void onUnpair() override {
        Serial.println("[TheGillModule] Unpaired");
        onThegillUnpaired();
    }

    void updateControl(const InputManager& inputs, float dt) override {
        (void)inputs;
        (void)dt;
        // Call the centralized control update function from thegill.cpp
        updateThegillControl();
    }

    size_t prepareCommandPacket(size_t typeIndex, uint8_t* buffer, size_t bufferSize) override {
        switch (typeIndex) {
            case 0:
                if (bufferSize >= sizeof(ThegillCommand)) {
                    memcpy(buffer, &thegillCommand, sizeof(ThegillCommand));
                    return sizeof(ThegillCommand);
                }
                break;
            case 1:
                if (bufferSize >= sizeof(PeripheralCommand)) {
                    PeripheralCommand pkt{};
                    if (acquirePeripheralCommand(pkt)) {
                        memcpy(buffer, &pkt, sizeof(PeripheralCommand));
                        return sizeof(PeripheralCommand);
                    }
                }
                break;
            case 2:
                if (bufferSize >= sizeof(ConfigurationPacket)) {
                    ConfigurationPacket pkt{};
                    if (acquireConfigurationPacket(pkt)) {
                        memcpy(buffer, &pkt, sizeof(ConfigurationPacket));
                        return sizeof(ConfigurationPacket);
                    }
                }
                break;
            case 3:
                if (bufferSize >= sizeof(ArmControlCommand)) {
                    ArmControlCommand pkt{};
                    if (acquireArmCommand(pkt)) {
                        memcpy(buffer, &pkt, sizeof(ArmControlCommand));
                        return sizeof(ArmControlCommand);
                    }
                }
                break;
        }
        return 0;
    }

    void handleTelemetry(size_t typeIndex, const uint8_t* data, size_t length) override {
        if (typeIndex == 0 && length >= sizeof(StatusPacket)) {
            StatusPacket pkt;
            memcpy(&pkt, data, sizeof(StatusPacket));
            processStatusPacket(pkt);
        } else if (typeIndex == 1 && length >= sizeof(ArmStatePacket)) {
            ArmStatePacket pkt;
            memcpy(&pkt, data, sizeof(ArmStatePacket));
            processArmStatePacket(pkt);
        }
    }

    void drawDashboard(DisplayCanvas& canvas) override {
        const int16_t top = 14;  // Below top strip

        if (mechIaneMode != MechIaneMode::DriveMode) {
            // Draw 3D arm visualization
            extern IKEngine::IKSolution lastArmSolution;
            extern void draw3DArmVisualization(DisplayCanvas& canvas, const IKEngine::IKSolution& solution);

            // Solve IK to get current solution
            extern IKEngine::Vec3 targetPosition;
            extern IKEngine::InverseKinematics ikSolver;
            extern ArmControlCommand armCommand;
            IKEngine::IKSolution solution;
            ikSolver.solvePlanar(targetPosition, armCommand.extensionMillimeters, solution);
            solution.joints.baseYawDeg = armCommand.baseDegrees;
            solution.joints.elbowExtensionMm = armCommand.extensionMillimeters;

            draw3DArmVisualization(canvas, solution);

            canvas.setFont(DisplayCanvas::TINY);
            canvas.drawText(0, top, (mechIaneMode == MechIaneMode::ArmXYZ) ? "ARM XYZ" : "ARM ORI");
            canvas.drawTextF(66, top, "Cam:%s", cameraViewLabel(thegillRuntime.cameraView));
            canvas.drawTextF(0, 54, "Cmd:%ums", thegillRuntime.commandAgeMs);
            canvas.drawTextF(0, 62, "Focus:%s", gripperFocusLabel(thegillRuntime.gripperTarget));
            drawBatteryWidget(canvas, 74, 48);
            return;
        }

        canvas.setFont(DisplayCanvas::TINY);
        int y = top;
        canvas.drawTextF(0, y, "Profile:%s", profileLabel(thegillConfig.profile));
        canvas.drawTextF(70, y, "Mode:%s", modeLabel(mechIaneMode));
        y += 6;
        canvas.drawTextF(0, y, "Speed:%3d%%",
                         static_cast<int>(roundf(thegillRuntime.driveSpeedScalar * 100.0f)));
        canvas.drawTextF(70, y, "Pump:%3u", thegillRuntime.pumpDuty);
        y += 6;
        canvas.drawTextF(0, y, "Ease:%s", easingLabel(thegillConfig.easing));
        canvas.drawTextF(70, y, "Rate:%3d%%",
                         static_cast<int>(roundf(thegillRuntime.easingRate * 100.0f)));
        y += 6;
        canvas.drawTextF(0, y, "Focus:%s", gripperFocusLabel(thegillRuntime.gripperTarget));
        canvas.drawTextF(70, y, "Cam:%s", cameraViewLabel(thegillRuntime.cameraView));
        y += 6;

        const float leftTarget = 0.5f * (thegillRuntime.targetLeftFront + thegillRuntime.targetLeftRear);
        const float rightTarget = 0.5f * (thegillRuntime.targetRightFront + thegillRuntime.targetRightRear);
        const float leftActual = 0.5f * (thegillRuntime.actualLeftFront + thegillRuntime.actualLeftRear);
        const float rightActual = 0.5f * (thegillRuntime.actualRightFront + thegillRuntime.actualRightRear);

        drawMotorBar(canvas, 2, y, leftActual, leftTarget);
        drawMotorBarLabels(canvas, 2, y, leftTarget, leftActual);
        drawMotorBar(canvas, 2, y + 12, rightActual, rightTarget);
        drawMotorBarLabels(canvas, 2, y + 12, rightTarget, rightActual);

        const bool linked = (thegillRuntime.statusFlags & StatusFlag::PairedLink) != 0;
        int tagY = top;
        drawStatusTag(canvas, 100, tagY, "DRV", thegillRuntime.driveEnabled);
        drawStatusTag(canvas, 100, tagY + 10, "ARM", thegillRuntime.armOutputsEnabled);
        drawStatusTag(canvas, 100, tagY + 20, "TEL", thegillRuntime.telemetryEnabled);
        drawStatusTag(canvas, 100, tagY + 30, "FS", thegillRuntime.failsafeEnabled);
        drawStatusTag(canvas, 100, tagY + 40, "LNK", linked);

        canvas.drawTextF(0, 60, "Cmd:%ums", thegillRuntime.commandAgeMs);
        drawBatteryWidget(canvas, 64, 52);
    }

    void buildModuleMenu(ModuleMenuBuilder& builder) override {
        builder.clear();
        ModuleMenuItem& root = builder.root();

        ModuleMenuItem& profileMenu = builder.addSubmenu("thegill.profile", "Drive Profile", ICON_ROBOT, &root);
        addProfileEntry(builder, profileMenu, GillDriveProfile::Tank, "Tank Drive");
        addProfileEntry(builder, profileMenu, GillDriveProfile::Differential, "Differential");

        ModuleMenuItem& easingMenu = builder.addSubmenu("thegill.easing", "Input Shaping", ICON_TUNING, &root);
        addEasingEntry(builder, easingMenu, GillDriveEasing::None, "None");
        addEasingEntry(builder, easingMenu, GillDriveEasing::SlewRate, "Slew Rate");
        addEasingEntry(builder, easingMenu, GillDriveEasing::Exponential, "Exponential");

        builder.addEditableFloat(
            "thegill.rate.edit",
            "Easing Strength",
            []() { return thegillConfig.easingRate; },
            [](float val) {
                thegillConfig.easingRate = constrain(val, 0.0f, 1.0f);
                markThegillConfigDirty();
                Serial.printf("[TheGill] Easing strength: %.2f\n", thegillConfig.easingRate);
            },
            0.0f,
            1.0f,
            0.05f,
            0.1f,
            ICON_TUNING,
            &root);

        builder.addToggle(
            "thegill.extension",
            "Enable Extension",
            []() { setExtensionEnabled(!isExtensionEnabled()); },
            []() { return isExtensionEnabled(); },
            ICON_SETTINGS,
            &root);

        // Camera view submenu (for arm visualization)
        ModuleMenuItem& cameraMenu = builder.addSubmenu("thegill.camera", "Camera View", ICON_HOME, &root);

        builder.addAction(
            "thegill.camera.topleft",
            "Top-Left Corner",
            []() {
                armCameraView = ArmCameraView::TopLeftCorner;
                Serial.println("[TheGill] Camera: Top-Left Corner");
            },
            ICON_SETTINGS,
            &cameraMenu);

        builder.addAction(
            "thegill.camera.3rdperson",
            "3rd Person",
            []() {
                armCameraView = ArmCameraView::ThirdPerson;
                Serial.println("[TheGill] Camera: 3rd Person");
            },
            ICON_SETTINGS,
            &cameraMenu);

        builder.addAction(
            "thegill.camera.overhead",
            "Overhead",
            []() {
                armCameraView = ArmCameraView::Overhead;
                Serial.println("[TheGill] Camera: Overhead");
            },
            ICON_SETTINGS,
            &cameraMenu);

        builder.addAction(
            "thegill.camera.side",
            "Side View",
            []() {
                armCameraView = ArmCameraView::Side;
                Serial.println("[TheGill] Camera: Side");
            },
            ICON_SETTINGS,
            &cameraMenu);

        builder.addAction(
            "thegill.camera.front",
            "Front View",
            []() {
                armCameraView = ArmCameraView::Front;
                Serial.println("[TheGill] Camera: Front");
            },
            ICON_SETTINGS,
            &cameraMenu);

        builder.addAction(
            "thegill.camera.operator_left",
            "Operator Left",
            []() {
                armCameraView = ArmCameraView::OperatorLeft;
                Serial.println("[TheGill] Camera: Operator Left");
            },
            ICON_SETTINGS,
            &cameraMenu);

        builder.addAction(
            "thegill.camera.operator_right",
            "Operator Right",
            []() {
                armCameraView = ArmCameraView::OperatorRight;
                Serial.println("[TheGill] Camera: Operator Right");
            },
            ICON_SETTINGS,
            &cameraMenu);

        builder.addAction(
            "thegill.camera.tool",
            "Tool Cam",
            []() {
                armCameraView = ArmCameraView::ToolTip;
                Serial.println("[TheGill] Camera: Tool Perspective");
            },
            ICON_SETTINGS,
            &cameraMenu);

        builder.addAction(
            "thegill.camera.orbit",
            "Orbit High",
            []() {
                armCameraView = ArmCameraView::OrbitHigh;
                Serial.println("[TheGill] Camera: Orbit High");
            },
            ICON_SETTINGS,
            &cameraMenu);

        builder.addAction(
            "thegill.camera.shoulder",
            "Shoulder Close",
            []() {
                armCameraView = ArmCameraView::ShoulderClose;
                Serial.println("[TheGill] Camera: Shoulder Close");
            },
            ICON_SETTINGS,
            &cameraMenu);

        builder.addAction(
            "thegill.camera.rear_quarter",
            "Rear Quarter",
            []() {
                armCameraView = ArmCameraView::RearQuarter;
                Serial.println("[TheGill] Camera: Rear Quarter");
            },
            ICON_SETTINGS,
            &cameraMenu);

        ModuleMenuItem& telemetryMenu = builder.addSubmenu("thegill.telemetry", "Telemetry", ICON_SIGNAL_FULL, &root);

        ModuleMenuItem& statusMenu = builder.addSubmenu("thegill.telemetry.status", "Status Packet", ICON_SIGNAL_FULL, &telemetryMenu);
        builder.addAction(
            "thegill.telemetry.status.wheels",
            "Wheel Speeds",
            []() {},
            ICON_SIGNAL_FULL,
            &statusMenu,
            0,
            nullptr,
            []() -> const char* {
                static char buffer[48];
                snprintf(buffer, sizeof(buffer), "LF:%d LR:%d RF:%d RR:%d",
                         thegillStatusPacket.wheelSpeedMmPerSec[0],
                         thegillStatusPacket.wheelSpeedMmPerSec[1],
                         thegillStatusPacket.wheelSpeedMmPerSec[2],
                         thegillStatusPacket.wheelSpeedMmPerSec[3]);
                return buffer;
            });

        builder.addAction(
            "thegill.telemetry.status.battery",
            "Battery",
            []() {},
            ICON_BATTERY_FULL,
            &statusMenu,
            0,
            nullptr,
            []() -> const char* {
                static char buffer[32];
                const uint16_t mv = thegillRuntime.batteryMillivolts;
                const uint8_t pct = batteryPercent4S(mv);
                snprintf(buffer, sizeof(buffer), "%umV (%u%%)", mv, pct);
                return buffer;
            });

        builder.addAction(
            "thegill.telemetry.status.led",
            "LED PWM",
            []() {},
            ICON_INFO,
            &statusMenu,
            0,
            nullptr,
            []() -> const char* {
                static char buffer[24];
                snprintf(buffer, sizeof(buffer), "%u/%u/%u",
                         thegillRuntime.ledPwm[0],
                         thegillRuntime.ledPwm[1],
                         thegillRuntime.ledPwm[2]);
                return buffer;
            });

        builder.addAction(
            "thegill.telemetry.status.pump",
            "Pump Duty",
            []() {},
            ICON_INFO,
            &statusMenu,
            0,
            nullptr,
            []() -> const char* {
                static char buffer[16];
                snprintf(buffer, sizeof(buffer), "%u", thegillRuntime.pumpDuty);
                return buffer;
            });

        builder.addAction(
            "thegill.telemetry.status.user",
            "User Mask",
            []() {},
            ICON_INFO,
            &statusMenu,
            0,
            nullptr,
            []() -> const char* {
                static char buffer[16];
                snprintf(buffer, sizeof(buffer), "0x%02X", thegillRuntime.userMask);
                return buffer;
            });

        builder.addAction(
            "thegill.telemetry.status.flags",
            "Status Flags",
            []() {},
            ICON_INFO,
            &statusMenu,
            0,
            nullptr,
            []() -> const char* {
                return formatStatusFlags(thegillRuntime.statusFlags);
            });

        builder.addAction(
            "thegill.telemetry.status.command_age",
            "Command Age",
            []() {},
            ICON_INFO,
            &statusMenu,
            0,
            nullptr,
            []() -> const char* {
                static char buffer[16];
                snprintf(buffer, sizeof(buffer), "%ums", thegillRuntime.commandAgeMs);
                return buffer;
            });

        ModuleMenuItem& armStateMenu = builder.addSubmenu("thegill.telemetry.arm", "Arm State", ICON_ROBOT, &telemetryMenu);
        builder.addAction(
            "thegill.telemetry.arm.base",
            "Base",
            []() {},
            ICON_ROBOT,
            &armStateMenu,
            0,
            nullptr,
            []() -> const char* {
                static char buffer[16];
                snprintf(buffer, sizeof(buffer), "%.1f deg", thegillArmStatePacket.baseDegrees);
                return buffer;
            });

        builder.addAction(
            "thegill.telemetry.arm.extension",
            "Extension",
            []() {},
            ICON_ROBOT,
            &armStateMenu,
            0,
            nullptr,
            []() -> const char* {
                static char buffer[16];
                snprintf(buffer, sizeof(buffer), "%.1fcm", thegillArmStatePacket.extensionCentimeters);
                return buffer;
            });

        const char* servoLabels[] = {"Shoulder", "Elbow", "Pitch", "Roll", "Yaw"};
        for (size_t i = 0; i < 5; ++i) {
            char id[48];
            snprintf(id, sizeof(id), "thegill.telemetry.arm.servo.%zu", i);
            builder.addAction(
                id,
                servoLabels[i],
                []() {},
                ICON_INFO,
                &armStateMenu,
                static_cast<int>(i),
                nullptr,
                [i]() -> const char* {
                    static char buffer[16];
                    snprintf(buffer, sizeof(buffer), "%.1f deg", thegillArmStatePacket.servoDegrees[i]);
                    return buffer;
                });
        }

        builder.addAction(
            "thegill.telemetry.arm.enabled",
            "Servo Enabled",
            []() {},
            ICON_INFO,
            &armStateMenu,
            10,
            nullptr,
            []() -> const char* {
                static char buffer[12];
                writeServoMask(thegillArmStatePacket.servoEnabledMask, buffer, sizeof(buffer));
                return buffer;
            });

        builder.addAction(
            "thegill.telemetry.arm.attached",
            "Servo Attached",
            []() {},
            ICON_INFO,
            &armStateMenu,
            11,
            nullptr,
            []() -> const char* {
                static char buffer[12];
                writeServoMask(thegillArmStatePacket.servoAttachedMask, buffer, sizeof(buffer));
                return buffer;
            });

        builder.addAction(
            "thegill.telemetry.arm.flags",
            "Arm Flags",
            []() {},
            ICON_INFO,
            &armStateMenu,
            12,
            nullptr,
            []() -> const char* {
                return formatArmFlags(thegillArmStatePacket.flags);
            });

        ModuleMenuItem& configMenu = builder.addSubmenu("thegill.telemetry.config", "Configuration", ICON_SETTINGS, &telemetryMenu);
        builder.addAction(
            "thegill.telemetry.config.easing_mode",
            "Easing Mode",
            []() {},
            ICON_SETTINGS,
            &configMenu,
            0,
            nullptr,
            []() -> const char* {
                static const char* labels[] = {"None", "Slew Rate", "Exponential"};
                uint8_t mode = thegillConfigurationPacket.easingMode;
                if (mode < (sizeof(labels) / sizeof(labels[0]))) {
                    return labels[mode];
                }
                return "Unknown";
            });

        builder.addAction(
            "thegill.telemetry.config.easing_rate",
            "Easing Strength",
            []() {},
            ICON_SETTINGS,
            &configMenu,
            1,
            nullptr,
            []() -> const char* {
                static char buffer[16];
                const float pct = (thegillConfigurationPacket.easingRate / 255.0f) * 100.0f;
                snprintf(buffer, sizeof(buffer), "%.0f%%", pct);
                return buffer;
            });

        builder.addAction(
            "thegill.telemetry.config.control",
            "Control Flags",
            []() {},
            ICON_INFO,
            &configMenu,
            2,
            nullptr,
            []() -> const char* {
                return formatControlFlags(thegillConfigurationPacket.controlFlags);
            });

        builder.addAction(
            "thegill.telemetry.config.safety",
            "Safety Flags",
            []() {},
            ICON_INFO,
            &configMenu,
            3,
            nullptr,
            []() -> const char* {
                return formatSafetyFlags(thegillConfigurationPacket.safetyFlags);
            });

        builder.addAction(
            "thegill.telemetry.config.cutoff",
            "Battery Cutoff",
            []() {},
            ICON_BATTERY_FULL,
            &configMenu,
            4,
            nullptr,
            []() -> const char* {
                static char buffer[16];
                snprintf(buffer, sizeof(buffer), "%umV", thegillConfigurationPacket.batteryCutoffMillivolts);
                return buffer;
            });

        builder.addAction(
            "thegill.telemetry.config.recover",
            "Battery Recover",
            []() {},
            ICON_BATTERY_FULL,
            &configMenu,
            5,
            nullptr,
            []() -> const char* {
                static char buffer[16];
                snprintf(buffer, sizeof(buffer), "%umV", thegillConfigurationPacket.batteryRecoverMillivolts);
                return buffer;
            });
    }

private:
    static constexpr float kDeadzone = 0.05f;

    static float applyDeadzone(float value) {
        return (fabsf(value) < kDeadzone) ? 0.0f : value;
    }

    static int16_t toMotor(float value) {
        value = constrain(value, -1.0f, 1.0f);
        return static_cast<int16_t>(roundf(value * 32767.0f));
    }

    void writeMotorCommands(float left, float right) {
        const int16_t leftValue = toMotor(left);
        const int16_t rightValue = toMotor(right);

        thegillCommand.leftFront = leftValue;
        thegillCommand.leftRear = leftValue;
        thegillCommand.rightFront = rightValue;
        thegillCommand.rightRear = rightValue;

        thegillRuntime.targetLeftFront = left;
        thegillRuntime.targetLeftRear = left;
        thegillRuntime.targetRightFront = right;
        thegillRuntime.targetRightRear = right;
    }

    const char* profileLabel(GillDriveProfile profile) const {
        switch (profile) {
            case GillDriveProfile::Tank: return "Tank";
            case GillDriveProfile::Differential: return "Differential";
            default: return "Unknown";
        }
    }

    const char* easingLabel(GillDriveEasing easing) const {
        switch (easing) {
            case GillDriveEasing::None: return "None";
            case GillDriveEasing::SlewRate: return "Slew";
            case GillDriveEasing::Exponential: return "Expo";
            default: return "Unknown";
        }
    }

    void addProfileEntry(ModuleMenuBuilder& builder, ModuleMenuItem& parent, GillDriveProfile profile, const char* label) {
        builder.addAction(
            label,
            label,
            [this, profile]() { setDriveProfile(profile); },
            ICON_ROBOT,
            &parent,
            static_cast<int>(profile),
            nullptr,
            [this, profile]() { return thegillConfig.profile == profile ? "Active" : ""; });
    }

    void addEasingEntry(ModuleMenuBuilder& builder, ModuleMenuItem& parent, GillDriveEasing easing, const char* label) {
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

    void setDriveProfile(GillDriveProfile profile) {
        if (thegillConfig.profile == profile) {
            return;
        }
        thegillConfig.profile = profile;
        markThegillConfigDirty();
        Serial.printf("[TheGillModule] Profile set to %d\n", static_cast<int>(profile));
    }

    void setEasing(GillDriveEasing easing) {
        if (thegillConfig.easing == easing) {
            return;
        }
        thegillConfig.easing = easing;
        markThegillConfigDirty();
        Serial.printf("[TheGillModule] Easing set to %d\n", static_cast<int>(easing));
    }

    // ============================================================================
    // Dashboard Helper Functions
    // ============================================================================

    void drawMotorBar(DisplayCanvas& canvas, int x, int y, float actual, float target) {
        const int width = 72;
        const int height = 10;
        const int mid = x + width / 2;
        const int range = width / 2 - 2;

        // Draw frame
        canvas.drawRect(x, y, width, height, false);

        const int actualPixels = static_cast<int>(roundf(constrain(actual, -1.0f, 1.0f) * range));
        if (actualPixels >= 0) {
            canvas.drawRect(mid, y + 1, actualPixels, height - 2, true);
        } else {
            canvas.drawRect(mid + actualPixels, y + 1, -actualPixels, height - 2, true);
        }

        const int targetPixels = static_cast<int>(roundf(constrain(target, -1.0f, 1.0f) * range));
        const int targetX = mid + targetPixels;
        canvas.drawVLine(targetX, y, height);
    }

    void drawMotorBarLabels(DisplayCanvas& canvas, int x, int y,
                            float targetValue, float actualValue) {
        const int width = 72;
        const int height = 10;

        char targetBuf[8];
        char actualBuf[8];
        formatMotorPercent(targetValue, targetBuf, sizeof(targetBuf));
        formatMotorPercent(actualValue, actualBuf, sizeof(actualBuf));

        char line[24];
        snprintf(line, sizeof(line), "T:%s A:%s", targetBuf, actualBuf);

        int textWidth = strlen(line) * 5;
        int cursorX = x + (width - textWidth) / 2;
        if (cursorX < x + 1) cursorX = x + 1;

        canvas.setDrawColor(2);
        canvas.drawText(cursorX, y + height - 2, line);
        canvas.setDrawColor(1);
    }

    void formatMotorPercent(float value, char* buffer, size_t size) {
        int percent = static_cast<int>(roundf(constrain(value, -1.0f, 1.0f) * 100.0f));
        snprintf(buffer, size, "%+d%%", percent);
    }

    const char* modeLabel(MechIaneMode mode) const {
        switch (mode) {
            case MechIaneMode::DriveMode: return "Drive";
            case MechIaneMode::ArmXYZ: return "Arm XYZ";
            case MechIaneMode::ArmOrientation: return "Arm ORI";
        }
        return "Unknown";
    }

    const char* gripperFocusLabel(GripperControlTarget target) const {
        switch (target) {
            case GripperControlTarget::Both: return "Both";
            case GripperControlTarget::Left: return "Left";
            case GripperControlTarget::Right: return "Right";
        }
        return "Both";
    }

    const char* cameraViewLabel(ArmCameraView view) const {
        switch (view) {
            case ArmCameraView::TopLeftCorner: return "TL";
            case ArmCameraView::ThirdPerson: return "3P";
            case ArmCameraView::Overhead: return "OH";
            case ArmCameraView::Side: return "Side";
            case ArmCameraView::Front: return "Front";
            case ArmCameraView::OperatorLeft: return "OpL";
            case ArmCameraView::OperatorRight: return "OpR";
            case ArmCameraView::ToolTip: return "Tip";
            case ArmCameraView::OrbitHigh: return "Orb";
            case ArmCameraView::ShoulderClose: return "Shld";
            case ArmCameraView::RearQuarter: return "Rear";
        }
        return "Cam";
    }

    void drawStatusTag(DisplayCanvas& canvas, int x, int y, const char* label, bool active) {
        const int width = 24;
        const int height = 9;
        canvas.drawRoundRect(x, y, width, height, 2,0);
        if (active) {
            canvas.drawRect(x + 1, y + 1, width - 2, height - 2,0);
            canvas.setDrawColor(2);
        }
        canvas.drawText(x + 3, y + height - 2, label);
        if (active) {
            canvas.setDrawColor(1);
        }
    }

    static void appendFlagLabel(char* buffer, size_t size, bool active, const char* label) {
        if (!active || size == 0 || label == nullptr) {
            return;
        }
        size_t len = strlen(buffer);
        if (len >= size - 1) {
            return;
        }
        snprintf(buffer + len, size - len, "%s%s", (len > 0) ? " " : "", label);
    }

    static const char* formatStatusFlags(uint8_t flags) {
        static char buffer[48];
        buffer[0] = '\0';
        appendFlagLabel(buffer, sizeof(buffer), (flags & StatusFlag::DriveArmed) != 0, "Drive");
        appendFlagLabel(buffer, sizeof(buffer), (flags & StatusFlag::ArmOutputsEnabled) != 0, "Arm");
        appendFlagLabel(buffer, sizeof(buffer), (flags & StatusFlag::TelemetryOn) != 0, "Tel");
        appendFlagLabel(buffer, sizeof(buffer), (flags & StatusFlag::FailsafeArmed) != 0, "FS");
        appendFlagLabel(buffer, sizeof(buffer), (flags & StatusFlag::PairedLink) != 0, "Link");
        appendFlagLabel(buffer, sizeof(buffer), (flags & StatusFlag::BatteryLatch) != 0, "Batt");
        if (buffer[0] == '\0') {
            snprintf(buffer, sizeof(buffer), "%s", "None");
        }
        return buffer;
    }

    static const char* formatControlFlags(uint8_t flags) {
        static char buffer[48];
        buffer[0] = '\0';
        appendFlagLabel(buffer, sizeof(buffer), (flags & ConfigFlag::DriveEnabled) != 0, "Drive");
        appendFlagLabel(buffer, sizeof(buffer), (flags & ConfigFlag::ArmOutputs) != 0, "Arm");
        appendFlagLabel(buffer, sizeof(buffer), (flags & ConfigFlag::FailsafeEnable) != 0, "Failsafe");
        appendFlagLabel(buffer, sizeof(buffer), (flags & ConfigFlag::MuteAudio) != 0, "Mute");
        if (buffer[0] == '\0') {
            snprintf(buffer, sizeof(buffer), "%s", "None");
        }
        return buffer;
    }

    static const char* formatSafetyFlags(uint8_t flags) {
        static char buffer[24];
        buffer[0] = '\0';
        appendFlagLabel(buffer, sizeof(buffer), (flags & SafetyFlag::BatterySafety) != 0, "Battery");
        if (buffer[0] == '\0') {
            snprintf(buffer, sizeof(buffer), "%s", "None");
        }
        return buffer;
    }

    static const char* formatArmFlags(uint8_t flags) {
        static char buffer[32];
        buffer[0] = '\0';
        appendFlagLabel(buffer, sizeof(buffer), (flags & 0x01) != 0, "Outputs");
        appendFlagLabel(buffer, sizeof(buffer), (flags & 0x02) != 0, "Servos");
        if (buffer[0] == '\0') {
            snprintf(buffer, sizeof(buffer), "%s", "None");
        }
        return buffer;
    }

    static void writeServoMask(uint8_t mask, char* buffer, size_t size) {
        if (size < 3) {
            return;
        }
        buffer[0] = '0';
        buffer[1] = 'b';
        size_t digits = (size - 2 > 8) ? 8 : (size - 2);
        for (size_t i = 0; i < digits; ++i) {
            buffer[2 + i] = (mask & (1u << (7 - i))) ? '1' : '0';
        }
        buffer[2 + digits] = '\0';
    }

    static uint8_t batteryPercent4S(uint16_t millivolts) {
        if (millivolts == 0) {
            return 0;
        }
        float perCell = (static_cast<float>(millivolts) / 1000.0f) / 4.0f;
        if (perCell >= 4.2f) {
            return 100;
        } else if (perCell >= 4.0f) {
            return static_cast<uint8_t>(90.0f + ((perCell - 4.0f) / 0.2f) * 10.0f);
        } else if (perCell >= 3.85f) {
            return static_cast<uint8_t>(75.0f + ((perCell - 3.85f) / 0.15f) * 15.0f);
        } else if (perCell >= 3.7f) {
            return static_cast<uint8_t>(50.0f + ((perCell - 3.7f) / 0.15f) * 25.0f);
        } else if (perCell >= 3.5f) {
            return static_cast<uint8_t>(25.0f + ((perCell - 3.5f) / 0.2f) * 25.0f);
        } else if (perCell >= 3.3f) {
            return static_cast<uint8_t>(10.0f + ((perCell - 3.3f) / 0.2f) * 15.0f);
        } else if (perCell <= 3.0f) {
            return 0;
        }
        return static_cast<uint8_t>(((perCell - 3.0f) / 0.3f) * 10.0f);
    }

    void drawBatteryWidget(DisplayCanvas& canvas, int16_t x, int16_t y) const {
        const uint16_t millivolts = thegillRuntime.batteryMillivolts;
        const uint8_t percent = batteryPercent4S(millivolts);
        const int width = 46;
        const int height = 9;
        canvas.drawRect(x, y, width, height, false);
        const int fillWidth = static_cast<int>((width - 2) * percent / 100);
        if (fillWidth > 0) {
            canvas.drawRect(x + 1, y + 1, fillWidth, height - 2, true);
        }
        canvas.drawRect(x + width, y + height / 4, 3, height / 2, false);
        canvas.setFont(DisplayCanvas::TINY);
        if (millivolts > 0) {
            canvas.drawTextF(x, y - 6, "%.1fV", millivolts / 1000.0f);
            canvas.drawTextF(x + width + 6, y + height - 1, "%u%%", percent);
        } else {
            canvas.drawText(x, y - 6, "Batt:--");
            canvas.drawText(x + width + 6, y + height - 1, "--%");
        }
    }

    // ============================================================================
    // Framework v2.0 Support - Button Events & Encoder Functions
    // ============================================================================

    void onActivate() override {
        // Register encoder functions when module activates
        FrameworkEngine& fw = FrameworkEngine::getInstance();

        EncoderFunction f1;
        f1.label = "MODE";
        f1.fullName = "Control Mode";
        f1.callback = []() {
            cycleControlMode();
        };
        f1.isToggle = false;
        f1.toggleState = nullptr;
        fw.setEncoderFunction(0, f1);

        EncoderFunction f2;
        f2.label = "CAM";
        f2.fullName = "Arm Camera View";
        f2.callback = []() {
            cycleArmCameraView(1);
        };
        f2.isToggle = false;
        f2.toggleState = nullptr;
        fw.setEncoderFunction(1, f2);

        Serial.println("[TheGillModule] Activated with encoder functions");
    }

    void onDeactivate() override {
        // Clear encoder functions when module deactivates
        FrameworkEngine& fw = FrameworkEngine::getInstance();
        fw.clearEncoderFunction(0);
        fw.clearEncoderFunction(1);

        // Clear button bindings
        ControlBindingSystem::clearModuleBindings();

        Serial.println("[TheGillModule] Deactivated");
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
        drongazeCommand.magic = DRONGAZE_PACKET_MAGIC;
        drongazeCommand.throttle = 1000;
        drongazeCommand.pitchAngle = 0;
        drongazeCommand.rollAngle = 0;
        drongazeCommand.yawAngle = 0;
        drongazeCommand.arm_motors = false;
        Serial.println("[DrongazeModule] Initialized");
    }

    void onPair() override {
        Serial.println("[DrongazeModule] Paired");
        controlSessionActive = true;
        const uint8_t* mac = ILITE.getDiscovery().getPairedMac();
        if (mac != nullptr) {
            memcpy(targetAddress, mac, sizeof(targetAddress));
        }
    }

    void onUnpair() override {
        Serial.println("[DrongazeModule] Unpaired");
        initDrongazeState();
        controlSessionActive = false;
        memset(targetAddress, 0, sizeof(targetAddress));
        drongazeCommand.arm_motors = false;
    }

    void updateControl(const InputManager& inputs, float dt) override {
        (void)dt;

        controlSessionActive = ILITE.isPaired();
        if (controlSessionActive) {
            const uint8_t* mac = ILITE.getDiscovery().getPairedMac();
            if (mac != nullptr) {
                memcpy(targetAddress, mac, sizeof(targetAddress));
            }
        } else {
            memset(targetAddress, 0, sizeof(targetAddress));
        }

        const float throttleNorm = clampValue((inputs.getJoystickA_Y() + 1.0f) * 0.5f, 0.0f, 1.0f);
        float throttle = 1000.0f + throttleNorm * 1000.0f;
        const float trim = clampValue(inputs.getPotentiometer(), 0.0f, 1.0f) * 300.0f;
        throttle = clampValue(throttle + trim, 1000.0f, 2000.0f);
        drongazeCommand.throttle = static_cast<uint16_t>(throttle);
        drongazeState.currentSpeed = static_cast<int8_t>((throttle - 1000.0f) / 10.0f);

        const float yawInput = applyDeadzone(inputs.getJoystickA_X());
        drongazeState.yawCommand = clampValue<int16_t>(
            drongazeState.yawCommand + static_cast<int16_t>(yawInput * 12.0f), -180, 180);
        drongazeCommand.yawAngle = static_cast<int8_t>(drongazeState.yawCommand);

        const float rollInput = applyDeadzone(inputs.getJoystickB_X());
        const float pitchInput = applyDeadzone(inputs.getJoystickB_Y());
        const float axisScale = drongazeState.precisionMode ? 45.0f : 90.0f;

        drongazeCommand.rollAngle = static_cast<int8_t>(
            clampValue(rollInput * axisScale, -90.0f, 90.0f));
        drongazeCommand.pitchAngle = static_cast<int8_t>(
            clampValue(pitchInput * axisScale, -90.0f, 90.0f));
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
            memcpy(&drongazeTelemetry, data, sizeof(DrongazeTelemetry));
            drongazeState.stabilizationMask = drongazeTelemetry.stabilizationMask;
            drongazeState.stabilizationGlobal =
                (drongazeTelemetry.stabilizationMask & DRONGAZE_STABILIZATION_GLOBAL_BIT) != 0;
        }
    }

    void drawDashboard(DisplayCanvas& canvas) override {
        // 3D orientation cube visualization matching original ILITE
        const float xSize = 20.0f;
        const float ySize = 10.0f;
        const float zSize = 5.0f;
        const int16_t top = 14;
        const int cx = 64;  // Screen width/2
        const int cy = top + (64 - top) / 2;  // Center Y in available space

        // 3D cube vertices
        struct Vec3 { float x, y, z; };
        const Vec3 verts[8] = {
            {-xSize,-ySize,-zSize}, { xSize,-ySize,-zSize}, { xSize, ySize,-zSize}, {-xSize, ySize,-zSize},
            {-xSize,-ySize, zSize}, { xSize,-ySize, zSize}, { xSize, ySize, zSize}, {-xSize, ySize, zSize}
        };

        int px[8];
        int py[8];

        // Convert telemetry angles to radians
        float roll  = drongazeTelemetry.roll * DEG_TO_RAD;
        float pitch = -drongazeTelemetry.pitch * DEG_TO_RAD;
        float yaw   = -drongazeTelemetry.yaw * DEG_TO_RAD + PI;

        // Rotate each vertex
        for (int i = 0; i < 8; i++) {
            float x = verts[i].x;
            float y = verts[i].y;
            float z = verts[i].z;

            // Rotate around X (roll)
            float y1 = y * cos(roll) - z * sin(roll);
            float z1 = y * sin(roll) + z * cos(roll);
            y = y1; z = z1;

            // Rotate around Y (pitch)
            float x1 = x * cos(pitch) + z * sin(pitch);
            float z2 = -x * sin(pitch) + z * cos(pitch);
            x = x1; z = z2;

            // Rotate around Z (yaw)
            float x2 = x * cos(yaw) - y * sin(yaw);
            float y2 = x * sin(yaw) + y * cos(yaw);
            x = x2; y = y2;

            // Project to 2D
            px[i] = cx + static_cast<int>(x);
            py[i] = cy - static_cast<int>(y);
        }

        // Draw cube edges
        const uint8_t edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},  // Back face
            {4,5},{5,6},{6,7},{7,4},  // Front face
            {0,4},{1,5},{2,6},{3,7}   // Connecting edges
        };

        for (int i = 0; i < 12; i++) {
            canvas.drawLine(px[edges[i][0]], py[edges[i][0]],
                          px[edges[i][1]], py[edges[i][1]]);
        }

        // Draw vertical acceleration arrow
        int arrowLen = map(static_cast<int>(drongazeTelemetry.verticalAcc * 100), -1000, 1000, -20, 20);
        arrowLen = constrain(arrowLen, -25, 25);
        canvas.drawLine(cx, cy, cx, cy - arrowLen);

        // Draw arrow head
        if (arrowLen != 0) {
            int head = cy - arrowLen;
            int dir = (arrowLen > 0) ? -5 : 5;
            canvas.drawTriangle(cx, head, cx - 3, head + dir, cx + 3, head + dir, true);
        }

        // Telemetry text overlay (left side)
        canvas.setFont(DisplayCanvas::SMALL);
        int16_t textY = top + 6;
        canvas.drawText(0, textY, "Alt:N/A");
        textY += 8;
        canvas.drawTextF(0, textY, "Thr:%u", drongazeCommand.throttle);
        textY += 8;
        canvas.drawTextF(0, textY, "P:%d", drongazeCommand.pitchAngle);
        textY += 8;
        canvas.drawTextF(0, textY, "R:%d", drongazeCommand.rollAngle);
        textY += 8;
        canvas.drawTextF(0, textY, "Y:%d", drongazeCommand.yawAngle);
    }

    void buildModuleMenu(ModuleMenuBuilder& builder) override {
        builder.clear();
        ModuleMenuItem& root = builder.root();

        builder.addToggle(
            "drongaze.arm",
            "Arm Motors",
            [this]() { toggleArm(); },
            [this]() { return drongazeCommand.arm_motors; },
            ICON_LOCK,
            &root);

        builder.addToggle(
            "drongaze.precision",
            "Precision Mode",
            [this]() { togglePrecision(); },
            [this]() { return drongazeState.precisionMode; },
            ICON_JOYSTICK,
            &root);

        builder.addAction(
            "drongaze.yaw.reset",
            "Reset Yaw to 0",
            [this]() {
                drongazeState.yawCommand = 0;
                drongazeCommand.yawAngle = 0;
                Serial.println("[DroneGaze] Yaw reset to 0");
            },
            ICON_SETTINGS,
            &root,
            0,
            nullptr,
            [this]() {
                static char yawStr[16];
                snprintf(yawStr, sizeof(yawStr), "Yaw:%d", drongazeState.yawCommand);
                return yawStr;
            });

        builder.addAction(
            "drongaze.pid.refresh",
            "Request PID Gains",
            []() { requestDrongazePidGains(); },
            ICON_TUNING,
            &root);

        // Stabilization controls submenu
        ModuleMenuItem& stabMenu = builder.addSubmenu("drongaze.stab", "Stabilization", ICON_SETTINGS, &root);

        builder.addAction(
            "drongaze.stab.pitch",
            "Toggle Pitch",
            []() { toggleDrongazeAxisStabilization(0); },
            ICON_TUNING,
            &stabMenu);

        builder.addAction(
            "drongaze.stab.roll",
            "Toggle Roll",
            []() { toggleDrongazeAxisStabilization(1); },
            ICON_TUNING,
            &stabMenu);

        builder.addAction(
            "drongaze.stab.yaw",
            "Toggle Yaw",
            []() { toggleDrongazeAxisStabilization(2); },
            ICON_TUNING,
            &stabMenu);
    }

    // ========================================================================
    // Custom Screens - PID Tuner
    // ========================================================================

    size_t getCustomScreenCount() const override { return 1; }

    const char* getCustomScreenName(size_t index) const override {
        if (index == 0) return "PID Tuner";
        return "";
    }

    void drawCustomScreen(size_t index, DisplayCanvas& canvas) override {
        if (index == 0) {
            // Cast away const since renderPidTuner modifies state
            const_cast<DrongazeModule*>(this)->renderPidTuner(canvas);
        }
    }

    void onEncoderRotate(int delta) override {
        handlePidTunerEncoderRotate(delta);
    }

    void onEncoderButton() override {
        handlePidTunerEncoderPress();
    }

private:
    static constexpr float kDeadzone = 0.05f;

    static float applyDeadzone(float value) {
        return (fabsf(value) < kDeadzone) ? 0.0f : value;
    }

    void toggleArm() {
        drongazeCommand.arm_motors = !drongazeCommand.arm_motors;
        Serial.printf("[DrongazeModule] Motors %s\n", drongazeCommand.arm_motors ? "armed" : "disarmed");
    }

    void togglePrecision() {
        drongazeState.precisionMode = !drongazeState.precisionMode;
        Serial.printf("[DrongazeModule] Precision mode %s\n",
                      drongazeState.precisionMode ? "enabled" : "disabled");
    }

    // ========================================================================
    // PID Tuner Screen
    // ========================================================================

    struct PidTunerState {
        int cursorPos = 0;      // 0=axis, 1=param, 2=value, 3=send
        int selectedAxis = 0;    // 0=pitch, 1=roll, 2=yaw
        int selectedParam = 0;   // 0=Kp, 1=Ki, 2=Kd
        bool coarseStep = false; // false=fine (0.01), true=coarse (0.1)
    };

    PidTunerState pidTuner;

    void renderPidTuner(DisplayCanvas& canvas) {
        const int16_t top = 14;
        canvas.setFont(DisplayCanvas::SMALL);

        // Title
        canvas.drawText(30, top, "PID TUNER");

        int16_t y = top + 12;

        // Axis selection
        canvas.drawText(0, y, "Axis:");
        const char* axisNames[3] = {"Pitch", "Roll", "Yaw"};
        if (pidTuner.cursorPos == 0) canvas.drawText(30, y, ">");
        canvas.drawText(36, y, axisNames[pidTuner.selectedAxis]);
        y += 10;

        // Parameter selection
        canvas.drawText(0, y, "Param:");
        const char* paramNames[3] = {"Kp", "Ki", "Kd"};
        if (pidTuner.cursorPos == 1) canvas.drawText(30, y, ">");
        canvas.drawText(36, y, paramNames[pidTuner.selectedParam]);
        y += 10;

        // Current value and step size
        float* gainPtr = nullptr;
        if (pidTuner.selectedParam == 0) {
            gainPtr = &drongazeState.pidGains[pidTuner.selectedAxis].kp;
        } else if (pidTuner.selectedParam == 1) {
            gainPtr = &drongazeState.pidGains[pidTuner.selectedAxis].ki;
        } else {
            gainPtr = &drongazeState.pidGains[pidTuner.selectedAxis].kd;
        }

        canvas.drawText(0, y, "Value:");
        if (pidTuner.cursorPos == 2) canvas.drawText(30, y, ">");
        canvas.drawTextF(36, y, "%.2f", *gainPtr);
        y += 10;

        // Step size toggle
        canvas.drawText(0, y, "Step:");
        if (pidTuner.cursorPos == 2) canvas.drawText(30, y, ">");
        canvas.drawText(36, y, pidTuner.coarseStep ? "Coarse" : "Fine");
        y += 2;

        // Send button
        y += 10;
        if (pidTuner.cursorPos == 3) {
            canvas.drawRect(30, y - 8, 70, 10, false);
        }
        canvas.drawText(32, y, "[Send to Drone]");
    }

    void handlePidTunerEncoderRotate(int delta) {
        if (pidTuner.cursorPos == 0) {
            // Rotate through axes
            pidTuner.selectedAxis = (pidTuner.selectedAxis + delta + 3) % 3;
        } else if (pidTuner.cursorPos == 1) {
            // Rotate through parameters
            pidTuner.selectedParam = (pidTuner.selectedParam + delta + 3) % 3;
        } else if (pidTuner.cursorPos == 2) {
            // Adjust value
            float step = pidTuner.coarseStep ? 0.1f : 0.01f;
            float adjustment = delta * step;

            float* gainPtr = nullptr;
            if (pidTuner.selectedParam == 0) {
                gainPtr = &drongazeState.pidGains[pidTuner.selectedAxis].kp;
            } else if (pidTuner.selectedParam == 1) {
                gainPtr = &drongazeState.pidGains[pidTuner.selectedAxis].ki;
            } else {
                gainPtr = &drongazeState.pidGains[pidTuner.selectedAxis].kd;
            }

            *gainPtr = constrain(*gainPtr + adjustment, 0.0f, 10.0f);
            AudioRegistry::play("menu_select");
        } else {
            // Navigate to/from send button
            pidTuner.cursorPos = constrain(pidTuner.cursorPos + delta, 0, 3);
        }
    }

    void handlePidTunerEncoderPress() {
        if (pidTuner.cursorPos < 2) {
            // Move to next field
            pidTuner.cursorPos++;
            AudioRegistry::play("menu_select");
        } else if (pidTuner.cursorPos == 2) {
            // Toggle step size
            pidTuner.coarseStep = !pidTuner.coarseStep;
            AudioRegistry::play("menu_select");
        } else if (pidTuner.cursorPos == 3) {
            // Send PID gains to drone
            sendDrongazePidGains(pidTuner.selectedAxis);
            AudioRegistry::play("paired");
            Serial.printf("[DroneGaze] Sent PID gains for axis %d\n", pidTuner.selectedAxis);
        }
    }

    // ============================================================================
    // Framework v2.0 Support - Button Events & Encoder Functions
    // ============================================================================

    void onActivate() override {
        // Register encoder functions when module activates
        FrameworkEngine& fw = FrameworkEngine::getInstance();

        // F1: ARM/DISARM (toggle)
        EncoderFunction f1;
        f1.label = "ARM";
        f1.fullName = "Arm Motors";
        f1.callback = [this]() {
            toggleArm();
        };
        f1.isToggle = true;
        f1.toggleState = &drongazeCommand.arm_motors;
        fw.setEncoderFunction(0, f1);

        // F2: Precision mode (toggle)
        EncoderFunction f2;
        f2.label = "PREC";
        f2.fullName = "Precision Mode";
        f2.callback = [this]() {
            togglePrecision();
            AudioRegistry::play("menu_select");
        };
        f2.isToggle = true;
        f2.toggleState = &drongazeState.precisionMode;
        fw.setEncoderFunction(1, f2);

        Serial.println("[DrongazeModule] Activated with encoder functions");
    }

    void onDeactivate() override {
        // Clear encoder functions when module deactivates
        FrameworkEngine& fw = FrameworkEngine::getInstance();
        fw.clearEncoderFunction(0);
        fw.clearEncoderFunction(1);
        Serial.println("[DrongazeModule] Deactivated");
    }

    void onButtonEvent(size_t buttonIndex, int event) override {
        ButtonEvent evt = static_cast<ButtonEvent>(event);

        switch (buttonIndex) {
            case 0: // Button 1 - ARM/DISARM
                if (evt == ButtonEvent::PRESSED) {
                    toggleArm();
                }
                break;

            case 1: // Button 2 - Precision Mode
                if (evt == ButtonEvent::PRESSED) {
                    togglePrecision();
                    AudioRegistry::play("menu_select");
                }
                break;

            case 2: // Button 3 - Emergency stop (long press)
                if (evt == ButtonEvent::LONG_PRESS) {
                    drongazeCommand.arm_motors = false;
                    drongazeCommand.throttle = 1000;
                    drongazeCommand.pitchAngle = 0;
                    drongazeCommand.rollAngle = 0;
                    drongazeCommand.yawAngle = 0;
                    AudioRegistry::play("error");
                    Serial.println("[DrongazeModule] EMERGENCY STOP!");
                }
                break;
        }
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
        bulkyCommand.replyIndex = 0;
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
        (void)dt;

        // Read RAW joystick values (0-4095) using InputManager - no framework easing
        int16_t joyX = inputs.getJoystickB_X_Raw();
        int16_t joyY = inputs.getJoystickB_Y_Raw();

        // Map joystick Y to speed (-100 to 100)
        int16_t mappedY = map(joyY, 0, 4095, -100, 100);

        // Apply deadzone
        if (abs(mappedY) < 10) {
            mappedY = 0;
        }

        bulkyState.targetSpeed = static_cast<int8_t>(mappedY);

        // Map joystick X to motion state (for turning/steering)
        int16_t mappedX = map(joyX, 0, 4095, -100, 100);

        // Simple motion state encoding:
        // 0 = stop, 1 = forward, 2 = backward, 3 = turn left, 4 = turn right
        if (abs(mappedY) < 10 && abs(mappedX) < 10) {
            bulkyState.motionState = 0; // Stop
        } else if (abs(mappedX) > abs(mappedY)) {
            // Turning dominates
            bulkyState.motionState = (mappedX > 0) ? 4 : 3; // Turn right : Turn left
        } else {
            // Forward/backward dominates
            bulkyState.motionState = (mappedY > 0) ? 1 : 2; // Forward : Backward
        }

        // Read speed potentiometer for speed scaling
        uint16_t potValue = inputs.getPotentiometer_Raw();
        uint8_t speedScale = map(potValue, 0, 4095, 0, 100);

        // Apply slow mode limit
        if (bulkyState.slowModeActive) {
            speedScale = std::min<uint8_t>(speedScale, 40);
        }

        // Apply speed scaling
        bulkyCommand.speed = (bulkyState.targetSpeed * speedScale) / 100;
        bulkyCommand.motionState = bulkyState.motionState;

        // Read button states using InputManager
        bulkyCommand.buttonStates[0] = inputs.getButton1() ? 1 : 0;
        bulkyCommand.buttonStates[1] = inputs.getButton2() ? 1 : 0;
        bulkyCommand.buttonStates[2] = inputs.getButton3() ? 1 : 0;

        // Increment reply index
        bulkyCommand.replyIndex++;
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
        // Graphical dashboard matching original ILITE Bulky layout
        const int16_t baseY = 14;  // Below top strip

        // Fire position bar (horizontal with icon showing servo position)
        drawFirePosition(canvas, baseY + 7);

        // Line follower (4-segment display)
        drawLineFollower(canvas, baseY - 13);

        // Motion joystick (right side, lower)
        drawMotionJoystick(canvas, baseY + 18);

        // Peripheral joystick (right side, upper)
        drawPeripheralJoystick(canvas, baseY - 1);

        // Proximity sensors (vertical bars)
        drawProximitySensors(canvas, baseY + 8);

        // Speed bar (horizontal with value overlay)
        drawSpeedBar(canvas, baseY + 20);
    }

private:
    void drawFirePosition(DisplayCanvas& canvas, int16_t frameY) {
        // 71x12px horizontal bar showing servo position (0-180 degrees)
        const int16_t x = 28;
        const int16_t w = 71;
        const int16_t h = 12;

        canvas.drawRoundRect(x, frameY, w, h, 4, false);

        // Map fire position (0-180) to bar position
        int iconX = map(bulkyTelemetry.firePose, 0, 180, x + w - 8, x + 8);
        iconX = constrain(iconX, x + 2, x + w - 10);

        // Draw icon/marker at position
        canvas.setFont(DisplayCanvas::SMALL);
        canvas.drawText(iconX, frameY + h - 2, "^");
    }

    void drawLineFollower(DisplayCanvas& canvas, int16_t frameY) {
        // 4-segment line follower display (20x15px)
        const int16_t x = 1;
        const int16_t w = 20;
        const int16_t h = 15;

        canvas.drawRoundRect(x, frameY, w, h, 3, false);

        // Draw 4 segments based on line position bits
        uint8_t linePos = bulkyTelemetry.linePosition;
        int16_t fillY = frameY + 2;
        int16_t fillH = 11;

        if (linePos & 0x08) canvas.drawRect(x + 2, fillY, 3, fillH, true);   // Bit 3
        if (linePos & 0x04) canvas.drawRect(x + 6, fillY, 3, fillH, true);   // Bit 2
        if (linePos & 0x02) canvas.drawRect(x + 11, fillY, 3, fillH, true);  // Bit 1
        if (linePos & 0x01) canvas.drawRect(x + 15, fillY, 3, fillH, true);  // Bit 0
    }

    void drawMotionJoystick(DisplayCanvas& canvas, int16_t frameY) {
        // 25x18px frame with 3x3 dot showing joystick B position
        const int16_t x = 100;
        const int16_t w = 25;
        const int16_t h = 18;

        canvas.drawRoundRect(x, frameY, w, h, 3, false);

        // Map joystick values (assume -1 to +1 range) to frame
        const InputManager& inputs = InputManager::getInstance();
        float joyX = inputs.getJoystickB_X();  // -1 to +1
        float joyY = inputs.getJoystickB_Y();  // -1 to +1

        int dotX = x + w/2 + static_cast<int>((joyX * (w-6) / 2.0f));
        int dotY = frameY + h/2 + static_cast<int>((joyY * (h-6) / 2.0f));

        canvas.drawRoundRect(dotX, dotY, 3, 3, 1, true);
    }

    void drawPeripheralJoystick(DisplayCanvas& canvas, int16_t frameY) {
        // 25x18px frame with 3x3 dot showing joystick A position
        const int16_t x = 100;
        const int16_t w = 25;
        const int16_t h = 18;

        canvas.drawRoundRect(x, frameY, w, h, 3, false);

        const InputManager& inputs = InputManager::getInstance();
        float joyX = inputs.getJoystickA_X();
        float joyY = inputs.getJoystickA_Y();

        int dotX = x + w/2 + static_cast<int>((joyX * (w-6) / 2.0f));
        int dotY = frameY + h/2 + static_cast<int>((joyY * (h-6) / 2.0f));

        canvas.drawRoundRect(dotX, dotY, 3, 3, 1, true);
    }

    void drawProximitySensors(DisplayCanvas& canvas, int16_t baseY) {
        // Two vertical bars showing front/bottom distance (0-50cm)
        const int16_t w = 12;
        const int16_t h = 28;
        const int16_t y = baseY;

        // Bottom sensor (left bar)
        canvas.drawRoundRect(1, y, w, h, 3, false);
        int bottomFill = map(bulkyTelemetry.bottomDistance, 0, 50, 27, 0);
        bottomFill = constrain(bottomFill, 0, 27);
        if (bottomFill > 0) {
            canvas.drawRect(3, y + 21, 8, bottomFill, true);
        }

        // Front sensor (right bar)
        canvas.drawRoundRect(14, y, w, h, 3, false);
        int frontFill = map(bulkyTelemetry.frontDistance, 0, 50, 27, 0);
        frontFill = constrain(frontFill, 0, 27);
        if (frontFill > 0) {
            canvas.drawRect(16, y + 21, 8, frontFill, true);
        }
    }

    void drawSpeedBar(DisplayCanvas& canvas, int16_t frameY) {
        // 71x12px bar with speed value and pot indicator
        const int16_t x = 28;
        const int16_t w = 71;
        const int16_t h = 12;

        canvas.drawRoundRect(x, frameY, w, h, 4, false);

        // Fill based on command speed (-100 to +100)
        int speed = abs(bulkyCommand.speed);
        int fillW = map(speed, 0, 100, 0, w - 4);
        fillW = constrain(fillW, 0, w - 4);
        if (fillW > 0) {
            canvas.drawRect(x + 2, frameY + 2, fillW, h - 4, true);
        }

        // Draw speed value (XOR mode for visibility)
        canvas.setFont(DisplayCanvas::NORMAL);
        canvas.setDrawColor(2);  // XOR
        char speedStr[8];
        snprintf(speedStr, sizeof(speedStr), "%d", speed);
        canvas.drawText(x + w/2 - 8, frameY + h - 2, speedStr);
        canvas.setDrawColor(1);  // Normal

        // Pot position indicator line (below bar)
        const InputManager& inputs = InputManager::getInstance();
        float potVal = inputs.getPotentiometer();  // 0.0 to 1.0
        int lineX = x + static_cast<int>(potVal * (w - 1));
        canvas.drawVLine(lineX, frameY + h + 1, 3);
    }

public:

    void buildModuleMenu(ModuleMenuBuilder& builder) override {
        builder.clear();
        ModuleMenuItem& root = builder.root();

        builder.addToggle(
            "bulky.honk",
            "Toggle Horn",
            [this]() { toggleHonk(); },
            [this]() { return bulkyState.honkActive; },
            ICON_PLAY,
            &root);

        builder.addToggle(
            "bulky.light",
            "Toggle Lights",
            [this]() { toggleLight(); },
            [this]() { return bulkyState.lightActive; },
            ICON_INFO,
            &root);

        builder.addToggle(
            "bulky.slow",
            "Slow Mode",
            [this]() { toggleSlowMode(); },
            [this]() { return bulkyState.slowModeActive; },
            ICON_WARNING,
            &root);
    }

private:
    static constexpr float kDeadzone = 0.05f;

    static float applyDeadzone(float value) {
        return (fabsf(value) < kDeadzone) ? 0.0f : value;
    }

    static uint8_t computeMotionState(float forward, float turn) {
        const float absForward = fabsf(forward);
        const float absTurn = fabsf(turn);

        if (absForward < kDeadzone && absTurn < kDeadzone) {
            return 0; // stop
        }
        if (absTurn > absForward) {
            return turn > 0.0f ? 4 : 3; // turn right / left
        }
        return forward > 0.0f ? 1 : 2; // forward / backward
    }

    void toggleHonk() {
        bulkyState.honkActive = !bulkyState.honkActive;
        Serial.printf("[BulkyModule] Honk %s\n", bulkyState.honkActive ? "enabled" : "disabled");
    }

    void toggleLight() {
        bulkyState.lightActive = !bulkyState.lightActive;
        Serial.printf("[BulkyModule] Lights %s\n", bulkyState.lightActive ? "enabled" : "disabled");
    }

    void toggleSlowMode() {
        bulkyState.slowModeActive = !bulkyState.slowModeActive;
        Serial.printf("[BulkyModule] Slow mode %s\n", bulkyState.slowModeActive ? "enabled" : "disabled");
    }

    static const char* motionLabel(uint8_t state) {
        switch (state) {
            case 1: return "Forward";
            case 2: return "Backward";
            case 3: return "Turn Left";
            case 4: return "Turn Right";
            default: return "Stopped";
        }
    }

    // ============================================================================
    // Framework v2.0 Support - Button Events & Encoder Functions
    // ============================================================================

    void onActivate() override {
        // Register encoder functions when module activates
        FrameworkEngine& fw = FrameworkEngine::getInstance();

        // F1: Toggle lights
        EncoderFunction f1;
        f1.label = "LITE";
        f1.fullName = "Toggle Lights";
        f1.callback = [this]() {
            bulkyState.lightActive = !bulkyState.lightActive;
            bulkyCommand.buttonStates[1] = bulkyState.lightActive ? 1 : 0;
            AudioRegistry::play("menu_select");
            Serial.printf("[BulkyModule] Lights %s\n", bulkyState.lightActive ? "ON" : "OFF");
        };
        f1.isToggle = true;
        f1.toggleState = &bulkyState.lightActive;
        fw.setEncoderFunction(0, f1);

        // F2: Toggle slow mode
        EncoderFunction f2;
        f2.label = "SLOW";
        f2.fullName = "Slow Mode";
        f2.callback = [this]() {
            bulkyState.slowModeActive = !bulkyState.slowModeActive;
            bulkyCommand.buttonStates[2] = bulkyState.slowModeActive ? 1 : 0;
            AudioRegistry::play("menu_select");
            Serial.printf("[BulkyModule] Slow mode %s\n", bulkyState.slowModeActive ? "ON" : "OFF");
        };
        f2.isToggle = true;
        f2.toggleState = &bulkyState.slowModeActive;
        fw.setEncoderFunction(1, f2);

        Serial.println("[BulkyModule] Activated with encoder functions");
    }

    void onDeactivate() override {
        // Clear encoder functions when module deactivates
        FrameworkEngine& fw = FrameworkEngine::getInstance();
        fw.clearEncoderFunction(0);
        fw.clearEncoderFunction(1);
        Serial.println("[BulkyModule] Deactivated");
    }

    void onButtonEvent(size_t buttonIndex, int event) override {
        ButtonEvent evt = static_cast<ButtonEvent>(event);

        switch (buttonIndex) {
            case 0: // Button 1 - Honk
                if (evt == ButtonEvent::PRESSED) {
                    bulkyState.honkActive = true;
                    bulkyCommand.buttonStates[0] = 1;
                    AudioRegistry::play("paired");
                } else if (evt == ButtonEvent::RELEASED) {
                    bulkyState.honkActive = false;
                    bulkyCommand.buttonStates[0] = 0;
                }
                break;

            case 1: // Button 2 - Toggle lights
                if (evt == ButtonEvent::PRESSED) {
                    bulkyState.lightActive = !bulkyState.lightActive;
                    bulkyCommand.buttonStates[1] = bulkyState.lightActive ? 1 : 0;
                    AudioRegistry::play("menu_select");
                    Serial.printf("[BulkyModule] Lights %s\n", bulkyState.lightActive ? "ON" : "OFF");
                }
                break;

            case 2: // Button 3 - Toggle slow mode
                if (evt == ButtonEvent::PRESSED) {
                    bulkyState.slowModeActive = !bulkyState.slowModeActive;
                    bulkyCommand.buttonStates[2] = bulkyState.slowModeActive ? 1 : 0;
                    AudioRegistry::play("menu_select");
                    Serial.printf("[BulkyModule] Slow mode %s\n", bulkyState.slowModeActive ? "ON" : "OFF");
                }
                break;
        }
    }
};
// ============================================================================
// Explicit Registration (called before main)
// ============================================================================


static void registerModuleMenuEntries() {
    auto& modules = ModuleRegistry::getModules();
    if (modules.empty()) {
        return;
    }

    const size_t existingSize = g_moduleMenuEntryIds.size();
    g_moduleMenuEntryIds.reserve(existingSize + modules.size());

    for (ILITEModule* module : modules) {
        if (module == nullptr) {
            continue;
        }

        std::string id = std::string("module.") + module->getModuleId();
        if (std::find(g_moduleMenuEntryIds.begin(), g_moduleMenuEntryIds.end(), id) != g_moduleMenuEntryIds.end()) {
            continue;
        }

        g_moduleMenuEntryIds.push_back(id);

        const std::string& storedId = g_moduleMenuEntryIds.back();
        MenuEntry entry;
        entry.id = storedId.c_str();
        entry.parent = MENU_MODULES;
        entry.icon = ICON_ROBOT;
        entry.label = module->getModuleName();
        entry.shortLabel = nullptr;
        entry.onSelect = [module]() {
            if (module == nullptr) {
                return;
            }
            Serial.printf("[ModuleRegistration] Loading module: %s\n", module->getModuleName());
            ILITEFramework& app = ILITEFramework::getInstance();
            app.requestModuleActivation(module);
            FrameworkEngine::getInstance().closeMenu();
            AudioRegistry::play("paired");
        };
        entry.condition = nullptr;
        entry.getValue = nullptr;
        entry.priority = static_cast<int>(g_moduleMenuEntryIds.size());
        entry.isSubmenu = false;
        entry.isToggle = false;
        entry.getToggleState = nullptr;
        entry.isReadOnly = false;
        entry.customDraw = nullptr;
        // Editable fields use struct defaults
        MenuRegistry::registerEntry(entry);
    }
}

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
    static bool registered = false;
    static TheGillModule theGillModule;
    static DrongazeModule drongazeModule;
    static BulkyModule bulkyModule;

    if (registered) {
        return;
    }

    ModuleRegistry::registerModule(&theGillModule);
    ModuleRegistry::registerModule(&drongazeModule);
    ModuleRegistry::registerModule(&bulkyModule);

    registerModuleMenuEntries();
    registered = true;
    Serial.println("[ILITE] Built-in modules registered: TheGill, DroneGaze, Bulky");
}

