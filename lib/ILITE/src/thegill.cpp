#include "thegill.h"
#include "ILITEModule.h"
#include "ModuleRegistry.h"
#include "AudioRegistry.h"
#include "InputManager.h"
#include "DisplayCanvas.h"
#include "InverseKinematics.h"
#include "input.h"
#include <math.h>
#include <cstring>

namespace {

constexpr int16_t kMaxWheelCommand = 1000;
constexpr float kMinDriveScale = 0.10f;
constexpr float kMaxDriveScale = 1.0f;
constexpr float kMinArmSpeedScalar = 0.02f;
constexpr float kMaxArmSpeedScalar = 1.0f;
constexpr uint8_t kMaxPumpDuty = 200;
constexpr uint32_t kPeripheralIntervalMs = 120;
constexpr uint32_t kConfigMinIntervalMs = 1000;
constexpr uint32_t kArmStateRequestIntervalMs = 250;
constexpr uint32_t kArmCommandMinIntervalMs = 40;
constexpr float kDriveDeadzone = 0.05f;
constexpr float kMaxWheelSpeedMmPerSec = 1200.0f;

static inline int16_t toWheelCommand(float normalized) {
    return static_cast<int16_t>(constrain(normalized, -1.0f, 1.0f) * kMaxWheelCommand);
}

} // namespace

ThegillCommand thegillCommand{
  THEGILL_PACKET_MAGIC,
  0, 0, 0, 0,
  0,
  0
};

PeripheralCommand thegillPeripheralCommand{
  THEGILL_PERIPHERAL_MAGIC,
  {0, 0, 0},
  0,
  0,
  {0, 0, 0}
};

ConfigurationPacket thegillConfigurationPacket{
  THEGILL_CONFIG_MAGIC,
  static_cast<uint8_t>(DriveEasingMode::None),
  0,
  ConfigFlag::DriveEnabled | ConfigFlag::ArmOutputs | ConfigFlag::FailsafeEnable,
  SafetyFlag::BatterySafety,
  20000,
  20500
};

StatusPacket thegillStatusPacket{
  THEGILL_STATUS_MAGIC,
  {0, 0, 0, 0},
  0,
  {0, 0, 0},
  0,
  0,
  0,
  0
};

ArmStatePacket thegillArmStatePacket{
  THEGILL_ARM_STATE_MAGIC,
  0.0f,
  0.0f,
  {0.f, 0.f, 0.f, 0.f, 0.f},
  0,
  0,
  0
};

ThegillConfig thegillConfig{
  GillDriveProfile::Tank,
  GillDriveEasing::None,
  0.35f
};

static float driveSpeedScalar = 0.85f;
static float armSpeedScalar = 0.6f;
enum class PotTarget { DriveSpeed, Pump, Led1, ArmSpeed };
static PotTarget potTarget = PotTarget::DriveSpeed;
static bool pumpTargetActive = false;
static bool ledTargetActive = false;

ThegillRuntime thegillRuntime{
  0.f, 0.f, 0.f, 0.f,
  0.f, 0.f, 0.f, 0.f,
  false,
  false,
  true,
  true,
  false,
  false,
  0,
  0,
  {0, 0, 0},
  0,
  0,
  0,
  driveSpeedScalar,
  thegillConfig.easingRate,
  GripperControlTarget::Both,
  ArmCameraView::TopLeftCorner
};

static bool precisionMode = false;
static bool brakeLatch = false;
static bool honkActive = false;
static bool telemetryEnabled = false;
static bool buzzerEnabled = true;
static bool driveEnabled = true;
static bool failsafeEnabled = true;
static bool armOutputsEnabled = false;
static bool requestStatusPulse = false;
static bool requestArmStatePending = false;
static bool armStateSynced = false;
static bool peripheralDirty = true;
static bool configDirty = true;
static bool extensionEnabled = true;
static uint32_t lastPeripheralSendMs = 0;
static uint32_t lastConfigSendMs = 0;
static uint32_t lastArmStateRequestMs = 0;
static uint32_t lastArmCommandSendMs = 0;
static uint32_t lastStatusPacketMs = 0;
static uint32_t lastArmStatePacketMs = 0;
static float lastLeftCommand = 0.0f;
static float lastRightCommand = 0.0f;
static bool armCommandDirty = false;
static uint8_t systemCommandLatch = 0;
static MechIaneMode previousMode = MechIaneMode::DriveMode;

static bool updateByte(uint8_t& target, uint8_t value) {
    if (target == value) {
        return false;
    }
    target = value;
    return true;
}

static void refreshPeripheralState(float driveMagnitude) {
    bool changed = false;
    const uint8_t driveLed = static_cast<uint8_t>(constrain(driveMagnitude, 0.0f, 1.0f) * 255.0f);
    const uint8_t armLed = isArmStateSynced() ? 220 : 90;
    const uint8_t gripperLed = isGripperOpen() ? 230 : 80;

    if (!ledTargetActive) {
        changed |= updateByte(thegillPeripheralCommand.ledPwm[0], driveLed);
        thegillRuntime.ledPwm[0] = thegillPeripheralCommand.ledPwm[0];
    }
    changed |= updateByte(thegillPeripheralCommand.ledPwm[1], armLed);
    changed |= updateByte(thegillPeripheralCommand.ledPwm[2], gripperLed);
    thegillRuntime.ledPwm[1] = thegillPeripheralCommand.ledPwm[1];
    thegillRuntime.ledPwm[2] = thegillPeripheralCommand.ledPwm[2];

    uint8_t mask = 0;
    if (thegillRuntime.brakeActive) mask |= 0x01;
    if (thegillRuntime.honkActive) mask |= 0x02;
    if (mechIaneMode != MechIaneMode::DriveMode) mask |= 0x04;
    if (precisionMode) mask |= 0x08;
    if (armStateSynced) mask |= 0x10;
    changed |= updateByte(thegillPeripheralCommand.userMask, mask);

    if (changed) {
        peripheralDirty = true;
    }
}

static void latchSystemCommand(uint8_t bits) {
    systemCommandLatch |= bits;
}

static void syncConfigurationPacket() {
    thegillConfigurationPacket.magic = THEGILL_CONFIG_MAGIC;

    DriveEasingMode easingMode = DriveEasingMode::None;
    uint8_t easingRateByte = 0;
    switch (thegillConfig.easing) {
        case GillDriveEasing::SlewRate:
            easingMode = DriveEasingMode::SlewRate;
            easingRateByte = static_cast<uint8_t>(constrain(thegillConfig.easingRate * 255.0f, 0.0f, 255.0f));
            break;
        case GillDriveEasing::Exponential:
            easingMode = DriveEasingMode::Exponential;
            easingRateByte = static_cast<uint8_t>(constrain(thegillConfig.easingRate * 255.0f, 0.0f, 255.0f));
            break;
        case GillDriveEasing::None:
        default:
            easingMode = DriveEasingMode::None;
            easingRateByte = 0;
            break;
    }

    thegillConfigurationPacket.easingMode = static_cast<uint8_t>(easingMode);
    thegillConfigurationPacket.easingRate = easingRateByte;

    uint8_t controlFlags = 0;
    if (!buzzerEnabled)      controlFlags |= ConfigFlag::MuteAudio;
    if (driveEnabled)        controlFlags |= ConfigFlag::DriveEnabled;
    if (armOutputsEnabled)   controlFlags |= ConfigFlag::ArmOutputs;
    if (failsafeEnabled)     controlFlags |= ConfigFlag::FailsafeEnable;
    thegillConfigurationPacket.controlFlags = controlFlags;
    thegillConfigurationPacket.safetyFlags = SafetyFlag::BatterySafety;
}

// Mech'Iane Arm Control State
static constexpr float kGripperOpenDeg = 0.0f;
static constexpr float kGripperClosedDeg = 180.0f;
static constexpr float kGripperNeutralDeg = 90.0f;
ArmControlCommand armCommand{
  ARM_COMMAND_MAGIC,
  0.0f, 0.0f, 90.0f, 90.0f, 90.0f, 90.0f, 90.0f, kGripperNeutralDeg, kGripperNeutralDeg,
  0,
  0,
  0
};

MechIaneMode mechIaneMode = MechIaneMode::DriveMode;
ArmCameraView armCameraView = ArmCameraView::TopLeftCorner;

// Mech'Iane control state variables (non-static for external access)
IKEngine::Vec3 targetPosition{200.0f, 0.0f, 0.0f};    // Planar XY target in mm
IKEngine::Vec3 targetOrientation{0.0f, 0.0f, 1.0f};   // Tool direction vector
IKEngine::InverseKinematics ikSolver;
static bool gripperOpen = false;
static bool orientationAnglesValid = false;
static float orientationPitchRad = 0.0f;
static float orientationYawRad = 0.0f;
static float manualPitchDeg = 90.0f;
static float manualYawDeg = 90.0f;
static float manualRollDeg = 90.0f;
static float targetToolRollDeg = 90.0f;
static float manualBaseYawDeg = 0.0f;
static float manualExtensionMm = 0.0f;
static GripperControlTarget gripperTarget = GripperControlTarget::Both;
static bool honkCommandActive = false;
static uint32_t lastControlLoopMs = 0;

static constexpr ArmCameraView kCameraSequence[] = {
    ArmCameraView::TopLeftCorner,
    ArmCameraView::ThirdPerson,
    ArmCameraView::OrbitHigh,
    ArmCameraView::Overhead,
    ArmCameraView::Side,
    ArmCameraView::Front,
    ArmCameraView::OperatorLeft,
    ArmCameraView::OperatorRight,
    ArmCameraView::ToolTip,
    ArmCameraView::ShoulderClose,
    ArmCameraView::RearQuarter
};

static void setDriveSpeedScalar(float value) {
    driveSpeedScalar = constrain(value, kMinDriveScale, kMaxDriveScale);
    thegillRuntime.driveSpeedScalar = driveSpeedScalar;
}

static void setArmSpeedScalar(float scalar) {
    armSpeedScalar = constrain(scalar, kMinArmSpeedScalar, kMaxArmSpeedScalar);
}

static IKEngine::Vec3 planarToWorld(float planarX, float planarY, float yawDeg) {
    const float yawRad = yawDeg * DEG_TO_RAD;
    const float cosYaw = cosf(yawRad);
    const float sinYaw = sinf(yawRad);
    return {
        planarX * cosYaw,
        planarY,
        planarX * sinYaw
    };
}

static void setPlanarTargetFromWorld(const IKEngine::Vec3& world, float yawDeg) {
    const float yawRad = yawDeg * DEG_TO_RAD;
    const float cosYaw = cosf(yawRad);
    const float sinYaw = sinf(yawRad);
    const float horizontal = world.x * cosYaw + world.z * sinYaw;
    targetPosition.x = constrain(horizontal, 0.0f, 780.0f);
    targetPosition.y = constrain(world.y, -780.0f, 780.0f);
    targetPosition.z = 0.0f;
}

static void setPumpDuty(uint8_t duty) {
    duty = duty > kMaxPumpDuty ? kMaxPumpDuty : duty;
    if (updateByte(thegillPeripheralCommand.pumpDuty, duty)) {
        peripheralDirty = true;
    }
    thegillRuntime.pumpDuty = thegillPeripheralCommand.pumpDuty;
}

static void setLedLevel(uint8_t index, uint8_t value) {
    if (index >= 3) {
        return;
    }
    if (updateByte(thegillPeripheralCommand.ledPwm[index], value)) {
        peripheralDirty = true;
    }
    thegillRuntime.ledPwm[index] = thegillPeripheralCommand.ledPwm[index];
}

static void resetPotTargetForMode() {
    potTarget = (mechIaneMode == MechIaneMode::DriveMode) ? PotTarget::DriveSpeed : PotTarget::ArmSpeed;
}

static void disablePumpTarget(bool playSound) {
    if (!pumpTargetActive) {
        return;
    }
    pumpTargetActive = false;
    setPumpDuty(0);
    resetPotTargetForMode();
    if (playSound) {
        AudioRegistry::play("menu_back");
    }
}

static void disableLedTarget(bool playSound) {
    if (!ledTargetActive) {
        return;
    }
    ledTargetActive = false;
    setLedLevel(0, 0);
    resetPotTargetForMode();
    if (playSound) {
        AudioRegistry::play("menu_back");
    }
}

static void enablePumpTarget() {
    pumpTargetActive = true;
    ledTargetActive = false;
    potTarget = PotTarget::Pump;
    AudioRegistry::play("menu_select");
}

static void enableLedTarget() {
    ledTargetActive = true;
    pumpTargetActive = false;
    potTarget = PotTarget::Led1;
    AudioRegistry::play("menu_select");
}

static void applyPotValue(float potValue) {
    const float clamped = constrain(potValue, 0.0f, 1.0f);
    switch (potTarget) {
        case PotTarget::DriveSpeed: {
            float mapped = kMinDriveScale + (kMaxDriveScale - kMinDriveScale) * clamped;
            setDriveSpeedScalar(mapped);
            break;
        }
        case PotTarget::Pump: {
            uint8_t duty = static_cast<uint8_t>(roundf(clamped * kMaxPumpDuty));
            setPumpDuty(duty);
            break;
        }
        case PotTarget::Led1: {
            uint8_t pwm = static_cast<uint8_t>(roundf(clamped * 255.0f));
            setLedLevel(0, pwm);
            break;
        }
        case PotTarget::ArmSpeed: {
            float mapped = kMinArmSpeedScalar + (kMaxArmSpeedScalar - kMinArmSpeedScalar) * clamped;
            setArmSpeedScalar(mapped);
            break;
        }
    }
}

static bool setFingerDegrees(uint8_t fingerIndex, float degrees) {
    float clamped = constrain(degrees, kGripperOpenDeg, kGripperClosedDeg);
    float* target = (fingerIndex == 0) ? &armCommand.gripper1Degrees : &armCommand.gripper2Degrees;
    if (fabsf(*target - clamped) < 1e-3f) {
        return false;
    }
    *target = clamped;
    if (fingerIndex == 0) {
        armCommand.validMask |= ArmCommandMask::Gripper1;
    } else {
        armCommand.validMask |= ArmCommandMask::Gripper2;
    }
    return true;
}

void setExtensionEnabled(bool enabled) {
    if (extensionEnabled == enabled) {
        return;
    }
    extensionEnabled = enabled;
    ikSolver.setExtensionEnabled(extensionEnabled);
    if (!extensionEnabled) {
        manualExtensionMm = 0.0f;
        armCommand.extensionMillimeters = 0.0f;
        armCommand.validMask |= ArmCommandMask::Extension;
        armCommandDirty = true;
    }
}

bool isExtensionEnabled() {
    return extensionEnabled;
}

static void commandGrippers(float commandValue) {
    float targetDeg = kGripperNeutralDeg;
    if (commandValue > 0.1f) {
        targetDeg = kGripperClosedDeg;
    } else if (commandValue < -0.1f) {
        targetDeg = kGripperOpenDeg;
    }

    bool changed = false;
    changed |= setFingerDegrees(0, targetDeg);
    changed |= setFingerDegrees(1, targetDeg);

    if (changed) {
        armCommand.flags |= ArmCommandFlag::EnableOutputs;
        armCommandDirty = true;
        gripperOpen = targetDeg <= (kGripperNeutralDeg - 1.0f);
    }
}

static size_t cameraIndex(ArmCameraView view) {
    for (size_t i = 0; i < sizeof(kCameraSequence) / sizeof(kCameraSequence[0]); ++i) {
        if (kCameraSequence[i] == view) {
            return i;
        }
    }
    return 0;
}

static void applyCameraStep(int delta) {
    const size_t count = sizeof(kCameraSequence) / sizeof(kCameraSequence[0]);
    size_t idx = cameraIndex(armCameraView);
    int next = static_cast<int>(idx) + delta;
    while (next < 0) next += static_cast<int>(count);
    next %= static_cast<int>(count);
    armCameraView = kCameraSequence[next];
    thegillRuntime.cameraView = armCameraView;
    AudioRegistry::play("menu_select");
}

void cycleControlMode() {
    switch (mechIaneMode) {
        case MechIaneMode::DriveMode:
            mechIaneMode = MechIaneMode::ArmXYZ;
            forceArmStateResync();
            break;
        case MechIaneMode::ArmXYZ:
            mechIaneMode = MechIaneMode::ArmOrientation;
            requestOrientationRetarget();
            break;
        case MechIaneMode::ArmOrientation:
            mechIaneMode = MechIaneMode::DriveMode;
            break;
    }
    AudioRegistry::play("menu_select");
}

void cycleArmCameraView(int delta) {
    if (delta == 0) {
        delta = 1;
    }
    applyCameraStep(delta);
}

// ============================================================================
// TheGill Control Update (called at 50Hz)
// ============================================================================

void updateThegillControl() {
    const InputManager& inputs = InputManager::getInstance();
    const float potValue = inputs.getPotentiometer();
    const uint32_t now = millis();

    if (lastControlLoopMs == 0) {
        lastControlLoopMs = now;
    }
    float dt = (now - lastControlLoopMs) * 0.001f;
    lastControlLoopMs = now;
    if (dt < 0.0f) {
        dt = 0.0f;
    }

    const bool shiftDown = inputs.getButton2();
    const bool leftDown = inputs.getButton1();
    const bool rightDown = inputs.getButton3();
    const bool leftPressed = inputs.getButton1Pressed();
    const bool rightPressed = inputs.getButton3Pressed();
    const bool shiftPressed = inputs.getButton2Pressed();
    const bool inDriveMode = (mechIaneMode == MechIaneMode::DriveMode);

    // Joystick button detection with beep feedback
    const bool joyBtnAPressed = inputs.getJoystickButtonA_Pressed();
    const bool joyBtnBPressed = inputs.getJoystickButtonB_Pressed();
    const bool joyBtnAState = inputs.getJoystickButtonA();
    const bool joyBtnBState = inputs.getJoystickButtonB();

    // Debug: Print raw button states periodically
    static uint32_t lastButtonDebugMs = 0;
    if ((now - lastButtonDebugMs) > 2000) {
        Serial.printf("[Thegill Debug] JoyBtn A: %s (GPIO19), JoyBtn B: %s (GPIO13)\n",
                      joyBtnAState ? "PRESSED" : "released",
                      joyBtnBState ? "PRESSED" : "released");
        lastButtonDebugMs = now;
    }

    if (joyBtnAPressed) {
        AudioRegistry::play("menu_select");
        Serial.println("[Thegill] *** Joystick A button PRESSED EVENT ***");
    }

    if (joyBtnBPressed) {
        AudioRegistry::play("menu_select");
        Serial.println("[Thegill] *** Joystick B button PRESSED EVENT ***");
    }

    if (!inDriveMode) {
        disablePumpTarget(false);
        disableLedTarget(false);
        potTarget = PotTarget::ArmSpeed;
    } else if (!pumpTargetActive && !ledTargetActive) {
        potTarget = PotTarget::DriveSpeed;
    }

    auto comboTriggered = [&](bool buttonPressed, bool buttonHeld) {
        return (buttonPressed && shiftDown) || (shiftPressed && buttonHeld);
    };

    if (inDriveMode) {
        if (comboTriggered(rightPressed, rightDown)) {
            if (pumpTargetActive) {
                disablePumpTarget(true);
            } else {
                disableLedTarget(false);
                enablePumpTarget();
            }
        } else if (comboTriggered(leftPressed, leftDown)) {
            if (ledTargetActive) {
                disableLedTarget(true);
            } else {
                disablePumpTarget(false);
                enableLedTarget();
            }
        }
    } else if (shiftPressed && !leftDown && !rightDown) {
        if (mechIaneMode == MechIaneMode::ArmXYZ) {
            mechIaneMode = MechIaneMode::ArmOrientation;
            requestOrientationRetarget();
        } else {
            mechIaneMode = MechIaneMode::ArmXYZ;
        }
        potTarget = PotTarget::ArmSpeed;
        AudioRegistry::play("menu_select");
    }

    applyPotValue(potValue);

    const float precisionScalar = precisionMode ? 0.45f : 1.0f;
    const float adaptiveScalar = driveSpeedScalar * precisionScalar;
    const float armControlScalar = armSpeedScalar * precisionScalar;

    if (previousMode == MechIaneMode::DriveMode && mechIaneMode != MechIaneMode::DriveMode) {
        forceArmStateResync();
    }
    previousMode = mechIaneMode;

    auto shapeAxis = [](float value) {
        float v = (fabsf(value) < kDriveDeadzone) ? 0.0f : value;
        if (thegillConfig.easing == GillDriveEasing::Exponential) {
            const float expo = constrain(thegillConfig.easingRate, 0.0f, 1.0f);
            const float power = 1.0f + expo * 1.5f;
            v = copysignf(powf(fabsf(v), power), v);
        }
        return constrain(v, -1.0f, 1.0f);
    };

    auto applySlew = [](float target, float previous) {
        if (thegillConfig.easing != GillDriveEasing::SlewRate) {
            return target;
        }
        const float rate = constrain(thegillConfig.easingRate, 0.02f, 1.0f);
        const float maxDelta = rate * 0.25f;
        float delta = target - previous;
        if (delta > maxDelta) delta = maxDelta;
        if (delta < -maxDelta) delta = -maxDelta;
        return constrain(previous + delta, -1.0f, 1.0f);
    };

    if (inDriveMode) {
        float left = shapeAxis(inputs.getJoystickA_Y());
        float right = shapeAxis(inputs.getJoystickB_Y());

        if (thegillConfig.profile == GillDriveProfile::Differential) {
            const float throttle = shapeAxis(inputs.getJoystickA_Y());
            const float turn = shapeAxis(inputs.getJoystickA_X());
            left = throttle + turn;
            right = throttle - turn;
        }

        left = constrain(left * adaptiveScalar, -1.0f, 1.0f);
        right = constrain(right * adaptiveScalar, -1.0f, 1.0f);

        left = applySlew(left, lastLeftCommand);
        right = applySlew(right, lastRightCommand);
        lastLeftCommand = left;
        lastRightCommand = right;

        thegillCommand.leftFront = toWheelCommand(left);
        thegillCommand.leftRear = toWheelCommand(left);
        thegillCommand.rightFront = toWheelCommand(right);
        thegillCommand.rightRear = toWheelCommand(right);

        thegillRuntime.targetLeftFront = left;
        thegillRuntime.targetLeftRear = left;
        thegillRuntime.targetRightFront = right;
        thegillRuntime.targetRightRear = right;

        armCommand.validMask = 0;
        armCommandDirty = false;

        refreshPeripheralState(fmaxf(fabsf(left), fabsf(right)));
        commandGrippers(0.0f);
    } else {
        thegillCommand.leftFront = 0;
        thegillCommand.leftRear = 0;
        thegillCommand.rightFront = 0;
        thegillCommand.rightRear = 0;

        const float joyAX = shapeAxis(inputs.getJoystickA_X());
        const float joyAY = shapeAxis(inputs.getJoystickA_Y());
        const float joyBX = shapeAxis(inputs.getJoystickB_X());
        const float joyBY = shapeAxis(inputs.getJoystickB_Y());

        const auto& ikConfig = ikSolver.getConfiguration();
        const float baseMin = fminf(ikConfig.baseYaw.minDeg, ikConfig.baseYaw.maxDeg);
        const float baseMax = fmaxf(ikConfig.baseYaw.minDeg, ikConfig.baseYaw.maxDeg);
        const float baseYawSpeed = 6.0f * armControlScalar;
        manualBaseYawDeg = constrain(manualBaseYawDeg + joyBX * baseYawSpeed, baseMin, baseMax);

        if (mechIaneMode == MechIaneMode::ArmXYZ) {
            const float positionSpeed = 9.0f * armControlScalar;
            targetPosition.x = constrain(targetPosition.x + joyAX * positionSpeed, 0.0f, 780.0f);
            targetPosition.y = constrain(targetPosition.y + joyAY * positionSpeed, -780.0f, 780.0f);
            targetPosition.z = 0.0f;

            const float extensionSpeed = 8.0f * armControlScalar;
            const float extMin = fminf(ikConfig.elbowExtension.minMm, ikConfig.elbowExtension.maxMm);
            const float extMax = fmaxf(ikConfig.elbowExtension.minMm, ikConfig.elbowExtension.maxMm);
            manualExtensionMm = constrain(manualExtensionMm + joyBY * extensionSpeed, extMin, extMax);
        } else {
            if (!orientationAnglesValid) {
                manualPitchDeg = constrain(armCommand.pitchDegrees, 0.0f, 180.0f);
                manualYawDeg = constrain(armCommand.yawDegrees, 0.0f, 180.0f);
                manualRollDeg = constrain(armCommand.rollDegrees, 0.0f, 180.0f);
                targetToolRollDeg = manualRollDeg;
                orientationPitchRad = (manualPitchDeg - 90.0f) * DEG_TO_RAD;
                orientationYawRad = (manualYawDeg - 90.0f) * DEG_TO_RAD;
                orientationAnglesValid = true;
            }

            const float orientSpeedDeg = 0.08f * RAD_TO_DEG * armControlScalar;
            const float rollSpeedDeg = 12.0f * armControlScalar;

            const float prevPitch = manualPitchDeg;
            const float prevYaw = manualYawDeg;

            manualPitchDeg = constrain(manualPitchDeg + joyAY * orientSpeedDeg, 5.0f, 175.0f);
            manualYawDeg = constrain(manualYawDeg + joyAX * orientSpeedDeg, 0.0f, 180.0f);
            manualRollDeg = constrain(manualRollDeg + joyBY * rollSpeedDeg, 0.0f, 180.0f);
            targetToolRollDeg = manualRollDeg;

            if (fabsf(manualPitchDeg - prevPitch) > 0.05f || fabsf(manualYawDeg - prevYaw) > 0.05f) {
                Serial.printf("[Thegill Orientation] Pitch: %.2f deg, Yaw: %.2f deg, Roll: %.2f deg (joyAX: %.2f, joyAY: %.2f)\n",
                              manualPitchDeg, manualYawDeg, manualRollDeg, joyAX, joyAY);
            }

            orientationPitchRad = (manualPitchDeg - 90.0f) * DEG_TO_RAD;
            orientationYawRad = (manualYawDeg - 90.0f) * DEG_TO_RAD;
            targetOrientation.x = cosf(orientationPitchRad) * cosf(orientationYawRad);
            targetOrientation.y = cosf(orientationPitchRad) * sinf(orientationYawRad);
            targetOrientation.z = sinf(orientationPitchRad);
        }

        float gripperCommandValue = 0.0f;
        bool gripperOverride = false;
        if (joyBtnAState && !joyBtnBState) {
            gripperCommandValue = 1.0f;
            gripperOverride = true;
        } else if (joyBtnBState && !joyBtnAState) {
            gripperCommandValue = -1.0f;
            gripperOverride = true;
        }
        if (!gripperOverride && !shiftDown && (leftDown ^ rightDown)) {
            gripperCommandValue = leftDown ? -1.0f : 1.0f;
            gripperOverride = true;
        }
        if (!gripperOverride) {
            gripperCommandValue = 0.0f;
        }
        commandGrippers(gripperCommandValue);

        const float extMin = fminf(ikConfig.elbowExtension.minMm, ikConfig.elbowExtension.maxMm);
        const float extMax = fmaxf(ikConfig.elbowExtension.minMm, ikConfig.elbowExtension.maxMm);
        manualExtensionMm = constrain(manualExtensionMm, extMin, extMax);
        float commandedExtension = extensionEnabled ? manualExtensionMm : 0.0f;

        armCommand.baseDegrees = manualBaseYawDeg;
        armCommand.extensionMillimeters = commandedExtension;
        IKEngine::IKSolution solution;
        ikSolver.setExtensionEnabled(extensionEnabled);
        if (ikSolver.solvePlanar(targetPosition, commandedExtension, solution)) {
            solution.joints.baseYawDeg = manualBaseYawDeg;
            solution.joints.elbowExtensionMm = commandedExtension;
            armCommand.shoulderDegrees = solution.joints.shoulderDeg;
            armCommand.elbowDegrees = solution.joints.elbowDeg;
            armCommand.pitchDegrees = solution.joints.gripperPitchDeg;
            armCommand.rollDegrees = targetToolRollDeg;
            armCommand.yawDegrees = solution.joints.gripperYawDeg;
            armCommand.validMask = ArmCommandMask::AllServos |
                                   ArmCommandMask::Extension |
                                   ArmCommandMask::Base |
                                   ArmCommandMask::AllGrippers;
            armCommand.flags = ArmCommandFlag::EnableOutputs;
            armCommandDirty = true;

            if (mechIaneMode == MechIaneMode::ArmOrientation) {
                armCommand.pitchDegrees = manualPitchDeg;
                armCommand.rollDegrees = manualRollDeg;
                armCommand.yawDegrees = manualYawDeg;
            }

            // Debug: Log arm command when in orientation mode
            static uint32_t lastDebugPrintMs = 0;
            if (mechIaneMode == MechIaneMode::ArmOrientation && (now - lastDebugPrintMs) > 500) {
                Serial.printf("[Thegill ArmCmd] Pitch: %.1f°, Yaw: %.1f°, Roll: %.1f° | Shoulder: %.1f°, Elbow: %.1f°\n",
                              armCommand.pitchDegrees, armCommand.yawDegrees, armCommand.rollDegrees,
                              armCommand.shoulderDegrees, armCommand.elbowDegrees);
                lastDebugPrintMs = now;
            }

            manualPitchDeg = armCommand.pitchDegrees;
            manualYawDeg = armCommand.yawDegrees;
            manualRollDeg = armCommand.rollDegrees;
        } else {
            armCommand.validMask = ArmCommandMask::Extension |
                                   ArmCommandMask::Base |
                                   ArmCommandMask::AllGrippers;
            armCommand.flags = ArmCommandFlag::EnableOutputs;
            armCommandDirty = true;
        }

        refreshPeripheralState(0.0f);
    }

    honkCommandActive = inDriveMode && shiftDown && !leftDown && !rightDown;

    if (requestStatusPulse) {
        latchSystemCommand(GillSystemCommand_RequestStatus);
        requestStatusPulse = false;
    }

    if (requestArmStatePending && (now - lastArmStateRequestMs) >= kArmStateRequestIntervalMs) {
        latchSystemCommand(GillSystemCommand_RequestArmState);
        lastArmStateRequestMs = now;
    }

    uint8_t commandFlags = 0;
    if (brakeLatch) {
        commandFlags |= GillCommandFlag_Brake;
    }
    if (honkCommandActive) {
        commandFlags |= GillCommandFlag_Honk;
    }
    thegillCommand.flags = commandFlags;

    thegillCommand.magic = THEGILL_PACKET_MAGIC;
    thegillCommand.system = systemCommandLatch;
    systemCommandLatch = 0;

    thegillRuntime.brakeActive = (commandFlags & GillCommandFlag_Brake) != 0;
    thegillRuntime.honkActive = honkCommandActive;
    thegillRuntime.gripperTarget = gripperTarget;
    thegillRuntime.driveSpeedScalar = driveSpeedScalar;
    thegillRuntime.easingRate = thegillConfig.easingRate;
    thegillRuntime.cameraView = armCameraView;
}


// ============================================================================
// TheGill Module Class (ILITE Framework Integration)
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

// ============================================================================
// 3D Arm Visualization Helper Functions
// ============================================================================

// Simple 3D to 2D isometric projection
struct Point2D {
    int16_t x;
    int16_t y;
};

Point2D projectIsometric(float x, float y, float z, float scale = 0.12f) {
    Point2D p;

    switch (armCameraView) {
        case ArmCameraView::TopLeftCorner: {
            // Original view - top-left isometric
            const float angleX = 0.615f;  // ~35 degrees
            const float angleZ = 0.785f;  // 45 degrees

            float cosX = cosf(angleX);
            float sinX = sinf(angleX);
            float cosZ = cosf(angleZ);
            float sinZ = sinf(angleZ);

            // Rotate around Z then X
            float x1 = x * cosZ - y * sinZ;
            float y1 = x * sinZ + y * cosZ;
            float z1 = z;

            float x2 = x1;
            float y2 = y1 * cosX - z1 * sinX;

            p.x = static_cast<int16_t>(x2 * scale + 84);
            p.y = static_cast<int16_t>(-y2 * scale + 45);
            break;
        }

        case ArmCameraView::ThirdPerson: {
            // Low-height 3rd person view (behind and below)
            // Camera positioned at (-300, -400, 50) looking at origin
            const float camX = -300.0f;
            const float camY = -400.0f;
            const float camZ = 50.0f;

            // Translate to camera space
            float dx = x - camX;
            float dy = y - camY;
            float dz = z - camZ;

            // Rotate to face camera forward (+Y in camera space)
            const float angleY = 0.3f;  // Slight upward tilt
            float cosY = cosf(angleY);
            float sinY = sinf(angleY);

            float y2 = dy * cosY - dz * sinY;
            float z2 = dy * sinY + dz * cosY;

            // Perspective projection
            float depth = y2 + 600.0f;  // Prevent division by zero
            if (depth < 10.0f) depth = 10.0f;

            float perspective = 200.0f / depth;
            p.x = static_cast<int16_t>(dx * perspective + 64);
            p.y = static_cast<int16_t>(-z2 * perspective + 48);
            break;
        }

        case ArmCameraView::Overhead: {
            // Top-down view
            p.x = static_cast<int16_t>(x * 0.15f + 64);
            p.y = static_cast<int16_t>(y * 0.15f + 32);
            break;
        }

        case ArmCameraView::Side: {
            // Side view (looking from X axis)
            p.x = static_cast<int16_t>(y * 0.12f + 64);
            p.y = static_cast<int16_t>(-z * 0.12f + 48);
            break;
        }

        case ArmCameraView::Front: {
            // Front view (looking from Y axis)
            p.x = static_cast<int16_t>(x * 0.12f + 64);
            p.y = static_cast<int16_t>(-z * 0.12f + 48);
            break;
        }

        case ArmCameraView::OperatorLeft: {
            const float camX = -150.0f;
            const float camY = -250.0f;
            const float camZ = 120.0f;
            float dx = x - camX;
            float dy = y - camY;
            float dz = z - camZ;

            const float yaw = 0.4f;
            const float pitch = 0.35f;
            float cosYaw = cosf(yaw);
            float sinYaw = sinf(yaw);
            float cosPitch = cosf(pitch);
            float sinPitch = sinf(pitch);

            float x1 = dx * cosYaw - dy * sinYaw;
            float y1 = dx * sinYaw + dy * cosYaw;
            float y2 = y1 * cosPitch - dz * sinPitch;
            float z2 = y1 * sinPitch + dz * cosPitch;

            float depth = y2 + 500.0f;
            if (depth < 10.0f) depth = 10.0f;
            float perspective = 220.0f / depth;
            p.x = static_cast<int16_t>(x1 * perspective + 64);
            p.y = static_cast<int16_t>(-z2 * perspective + 44);
            break;
        }

        case ArmCameraView::OperatorRight: {
            const float camX = 150.0f;
            const float camY = -250.0f;
            const float camZ = 140.0f;
            float dx = x - camX;
            float dy = y - camY;
            float dz = z - camZ;

            const float yaw = -0.4f;
            const float pitch = 0.4f;
            float cosYaw = cosf(yaw);
            float sinYaw = sinf(yaw);
            float cosPitch = cosf(pitch);
            float sinPitch = sinf(pitch);

            float x1 = dx * cosYaw - dy * sinYaw;
            float y1 = dx * sinYaw + dy * cosYaw;
            float y2 = y1 * cosPitch - dz * sinPitch;
            float z2 = y1 * sinPitch + dz * cosPitch;

            float depth = y2 + 500.0f;
            if (depth < 10.0f) depth = 10.0f;
            float perspective = 220.0f / depth;
            p.x = static_cast<int16_t>(x1 * perspective + 64);
            p.y = static_cast<int16_t>(-z2 * perspective + 44);
            break;
        }

        case ArmCameraView::ToolTip: {
            // Camera positioned ahead of the tool looking back toward base
            const float camX = 420.0f;
            const float camY = 0.0f;
            const float camZ = 180.0f;
            float dx = x - camX;
            float dy = y - camY;
            float dz = z - camZ;

            const float yaw = PI;          // Look back toward base
            const float pitch = -0.2f;     // Slight downward tilt
            float cosYaw = cosf(yaw);
            float sinYaw = sinf(yaw);
            float cosPitch = cosf(pitch);
            float sinPitch = sinf(pitch);

            float x1 = dx * cosYaw - dy * sinYaw;
            float y1 = dx * sinYaw + dy * cosYaw;
            float y2 = y1 * cosPitch - dz * sinPitch;
            float z2 = y1 * sinPitch + dz * cosPitch;

            float depth = y2 + 400.0f;
            if (depth < 10.0f) depth = 10.0f;
            float perspective = 240.0f / depth;
            p.x = static_cast<int16_t>(x1 * perspective + 64);
            p.y = static_cast<int16_t>(-z2 * perspective + 38);
            break;
        }

        case ArmCameraView::OrbitHigh: {
            const float angleX = 0.9f;
            const float angleZ = 0.45f;
            float cosX = cosf(angleX);
            float sinX = sinf(angleX);
            float cosZ = cosf(angleZ);
            float sinZ = sinf(angleZ);

            float x1 = x * cosZ - y * sinZ;
            float y1 = x * sinZ + y * cosZ;
            float y2 = y1 * cosX - z * sinX;

            p.x = static_cast<int16_t>(x1 * 0.10f + 70);
            p.y = static_cast<int16_t>(-y2 * 0.10f + 30);
            break;
        }

        case ArmCameraView::ShoulderClose: {
            const float focusX = 120.0f;
            const float focusY = 0.0f;
            const float focusZ = 140.0f;
            const float localScale = 0.20f;
            p.x = static_cast<int16_t>((x - focusX) * localScale + 64);
            p.y = static_cast<int16_t>((focusZ - z) * localScale + 42);
            break;
        }

        case ArmCameraView::RearQuarter: {
            const float camX = 200.0f;
            const float camY = -360.0f;
            const float camZ = 150.0f;
            float dx = x - camX;
            float dy = y - camY;
            float dz = z - camZ;

            const float yaw = 0.55f;
            const float pitch = 0.4f;
            float cosYaw = cosf(yaw);
            float sinYaw = sinf(yaw);
            float cosPitch = cosf(pitch);
            float sinPitch = sinf(pitch);

            float x1 = dx * cosYaw - dy * sinYaw;
            float y1 = dx * sinYaw + dy * cosYaw;
            float y2 = y1 * cosPitch - dz * sinPitch;
            float z2 = y1 * sinPitch + dz * cosPitch;

            float depth = y2 + 520.0f;
            if (depth < 10.0f) depth = 10.0f;
            float perspective = 210.0f / depth;
            p.x = static_cast<int16_t>(x1 * perspective + 64);
            p.y = static_cast<int16_t>(-z2 * perspective + 46);
            break;
        }
    }

    return p;
}

void drawDottedLine(DisplayCanvas& canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t dotSpacing = 3) {
    // Draw a dotted line by plotting dots at intervals
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);

    if (len < 1.0f) return;

    float stepX = dx / len;
    float stepY = dy / len;

    for (float t = 0; t < len; t += dotSpacing) {
        int16_t px = x0 + static_cast<int16_t>(stepX * t);
        int16_t py = y0 + static_cast<int16_t>(stepY * t);
        canvas.drawPixel(px, py);
    }
}

void drawCylinder(DisplayCanvas& canvas, Point2D p1, Point2D p2, int16_t radius = 2) {
    // Draw a 3D cylinder outline between two points
    // Calculate perpendicular offset for cylinder walls
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    float len = sqrtf(dx * dx + dy * dy);

    if (len < 0.1f) {
        // Too short, just draw a circle
        canvas.drawCircle(p1.x, p1.y, radius, false);
        return;
    }

    // Perpendicular vector
    float perpX = -dy / len * radius;
    float perpY = dx / len * radius;

    // Draw four edges of the cylinder
    canvas.drawLine(p1.x + perpX, p1.y + perpY, p2.x + perpX, p2.y + perpY);
    canvas.drawLine(p1.x - perpX, p1.y - perpY, p2.x - perpX, p2.y - perpY);

    // Draw end caps
    canvas.drawCircle(p1.x, p1.y, radius, false);
    canvas.drawCircle(p2.x, p2.y, radius, false);
}

static IKEngine::Vec3 crossVec(const IKEngine::Vec3& a, const IKEngine::Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static void drawExtensionSegment(DisplayCanvas& canvas, Point2D start, Point2D end) {
    canvas.drawLine(start.x, start.y, end.x, end.y);
    canvas.drawLine(start.x + 1, start.y, end.x + 1, end.y);
    canvas.drawLine(start.x - 1, start.y, end.x - 1, end.y);
}

void draw3DArmVisualization(DisplayCanvas& canvas, const IKEngine::IKSolution& solution) {
    // Draw 3D isometric view of the arm based on IK solution
    canvas.setDrawColor(1);

    // Draw dotted grid on XY plane for depth perception
    const int gridSize = 500;  // Grid extends +/- 500mm
    const int gridStep = 100;  // 100mm grid spacing

    for (int x = -gridSize; x <= gridSize; x += gridStep) {
        Point2D p1 = projectIsometric(x, -gridSize, 0);
        Point2D p2 = projectIsometric(x, gridSize, 0);
        drawDottedLine(canvas, p1.x, p1.y, p2.x, p2.y, 4);
    }

    for (int y = -gridSize; y <= gridSize; y += gridStep) {
        Point2D p1 = projectIsometric(-gridSize, y, 0);
        Point2D p2 = projectIsometric(gridSize, y, 0);
        drawDottedLine(canvas, p1.x, p1.y, p2.x, p2.y, 4);
    }

    // Base position
    float baseX = 0.0f;
    float baseY = 0.0f;
    float baseZ = 0.0f;

    // Calculate arm link positions using FK from IK solution
    float baseYawRad = solution.joints.baseYawDeg * DEG_TO_RAD;
    float shoulderRad = solution.joints.shoulderDeg * DEG_TO_RAD;
    float elbowRad = solution.joints.elbowDeg * DEG_TO_RAD;

    // Shoulder joint position (rotates around base)
    float shoulderLen = 155.0f;  // From IK config
    float shoulderX = shoulderLen * cosf(shoulderRad) * cosf(baseYawRad);
    float shoulderZ = shoulderLen * cosf(shoulderRad) * sinf(baseYawRad);
    float shoulderY = shoulderLen * sinf(shoulderRad);

    // Elbow joint position
    float forearmLen = 170.0f + solution.joints.elbowExtensionMm;
    float elbowAngleAbs = shoulderRad - elbowRad;  // Absolute elbow angle
    float forearmDirX = cosf(elbowAngleAbs) * cosf(baseYawRad);
    float forearmDirZ = cosf(elbowAngleAbs) * sinf(baseYawRad);
    float forearmDirY = sinf(elbowAngleAbs);
    float extensionLen = fmaxf(solution.joints.elbowExtensionMm, 0.0f);
    float rigidLen = forearmLen - extensionLen;
    if (rigidLen < 0.0f) {
        extensionLen += rigidLen;
        rigidLen = 0.0f;
    }
    float extensionBaseX = shoulderX + rigidLen * forearmDirX;
    float extensionBaseZ = shoulderZ + rigidLen * forearmDirZ;
    float extensionBaseY = shoulderY + rigidLen * forearmDirY;
    float elbowX = extensionBaseX + extensionLen * forearmDirX;
    float elbowZ = extensionBaseZ + extensionLen * forearmDirZ;
    float elbowY = extensionBaseY + extensionLen * forearmDirY;

    // End effector (wrist center)
    float wristLen = 0.0f;  // Reference at extension tip
    float wristX = elbowX + solution.toolDirection.x * wristLen;
    float wristY = elbowY + solution.toolDirection.y * wristLen;
    float wristZ = elbowZ + solution.toolDirection.z * wristLen;

    // Project to 2D
    Point2D pBase = projectIsometric(baseX, baseY, baseZ);
    Point2D pShoulder = projectIsometric(shoulderX, shoulderY, shoulderZ);
    Point2D pElbow = projectIsometric(elbowX, elbowY, elbowZ);
    Point2D pWrist = projectIsometric(wristX, wristY, wristZ);
    IKEngine::Vec3 targetWorld = planarToWorld(targetPosition.x, targetPosition.y, solution.joints.baseYawDeg);
    Point2D pTarget = projectIsometric(targetWorld.x, targetWorld.y, targetWorld.z);

    // Draw arm links as cylinders for 3D effect
    Point2D pExtensionBase = projectIsometric(extensionBaseX, extensionBaseY, extensionBaseZ);
    drawCylinder(canvas, pBase, pShoulder, 2);      // Upper arm
    drawCylinder(canvas, pShoulder, pExtensionBase, 2);
    drawExtensionSegment(canvas, pExtensionBase, pElbow);                   // Telescoping section
    drawCylinder(canvas, pElbow, pWrist, 1);        // Wrist (thinner)

    // Draw joints as filled circles
    canvas.drawCircle(pBase.x, pBase.y, 3, true);
    canvas.drawCircle(pShoulder.x, pShoulder.y, 3, true);
    canvas.drawCircle(pElbow.x, pElbow.y, 3, true);

    // Draw end effector (gripper)
    canvas.drawCircle(pWrist.x, pWrist.y, 4, false);
    if (gripperOpen) {
        // Draw open gripper jaws
        canvas.drawLine(pWrist.x - 3, pWrist.y - 3, pWrist.x - 5, pWrist.y - 5);
        canvas.drawLine(pWrist.x + 3, pWrist.y - 3, pWrist.x + 5, pWrist.y - 5);
    } else {
        // Draw closed gripper
        canvas.drawLine(pWrist.x, pWrist.y - 3, pWrist.x, pWrist.y - 5);
    }

    // Draw target position as crosshair with circle
    canvas.drawLine(pTarget.x - 4, pTarget.y, pTarget.x + 4, pTarget.y);
    canvas.drawLine(pTarget.x, pTarget.y - 4, pTarget.x, pTarget.y + 4);
    canvas.drawCircle(pTarget.x, pTarget.y, 5, false);

    IKEngine::Vec3 wristVec{wristX, wristY, wristZ};
    IKEngine::Vec3 forward = IKEngine::InverseKinematics::normalise(solution.toolDirection);
    IKEngine::Vec3 worldUp{0.0f, 0.0f, 1.0f};
    IKEngine::Vec3 right = crossVec(forward, worldUp);
    float rightNorm = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
    if (rightNorm < 1e-3f) {
        right = {1.0f, 0.0f, 0.0f};
    }
    right = IKEngine::InverseKinematics::normalise(right);
    IKEngine::Vec3 up = IKEngine::InverseKinematics::normalise(crossVec(right, forward));
    const float rollRad = solution.joints.gripperRollDeg * DEG_TO_RAD;
    if (fabsf(rollRad) > 1e-3f) {
        const float c = cosf(rollRad);
        const float s = sinf(rollRad);
        IKEngine::Vec3 rotatedRight{
            right.x * c + up.x * s,
            right.y * c + up.y * s,
            right.z * c + up.z * s
        };
        IKEngine::Vec3 rotatedUp{
            up.x * c - right.x * s,
            up.y * c - right.y * s,
            up.z * c - right.z * s
        };
        right = rotatedRight;
        up = rotatedUp;
    }
    const float axisLen = 60.0f;
    auto drawAxis = [&](const IKEngine::Vec3& dir) {
        Point2D tip = projectIsometric(wristVec.x + dir.x * axisLen,
                                       wristVec.y + dir.y * axisLen,
                                       wristVec.z + dir.z * axisLen);
        canvas.drawLine(pWrist.x, pWrist.y, tip.x, tip.y);
    };
    drawAxis(forward);
    drawAxis(up);
    drawAxis(right);

    // Draw mode and gripper state indicators
    canvas.setFont(DisplayCanvas::TINY);
    if (mechIaneMode == MechIaneMode::ArmXYZ) {
        canvas.drawText(0, 63, "XYZ");
    } else {
        canvas.drawText(0, 63, "ORI");
    }

    // Draw gripper state
    if (gripperOpen) {
        canvas.drawText(110, 63, "OPEN");
    } else {
        canvas.drawText(110, 63, "CLSD");
    }
}

void setPrecisionMode(bool enabled) {
    precisionMode = enabled;
}

bool isPrecisionModeEnabled() {
    return precisionMode;
}

void requestOrientationRetarget() {
    orientationAnglesValid = false;
}

void setTargetToolRoll(float degrees) {
    targetToolRollDeg = constrain(degrees, 0.0f, 180.0f);
}

float getTargetToolRoll() {
    return targetToolRollDeg;
}

void setUnifiedGripper(bool open) {
    commandGrippers(open ? -1.0f : 1.0f);
}

void toggleUnifiedGripper() {
    setUnifiedGripper(!gripperOpen);
}

bool isGripperOpen() {
    return gripperOpen;
}

void setGripperFingerPosition(uint8_t fingerIndex, float degrees) {
    if (fingerIndex > 1) {
        return;
    }
    if (setFingerDegrees(fingerIndex, degrees)) {
        armCommand.flags |= ArmCommandFlag::EnableOutputs;
        armCommandDirty = true;
        const float average = 0.5f * (armCommand.gripper1Degrees + armCommand.gripper2Degrees);
        gripperOpen = average <= (kGripperNeutralDeg - 1.0f);
    }
}

static IKEngine::Vec3 estimateToolPosition(const ArmStatePacket& packet) {
    constexpr float shoulderLen = 155.0f;
    constexpr float elbowBaseLen = 170.0f;
    constexpr float wristLen = 0.0f;

    const float baseYawRad = packet.baseDegrees * DEG_TO_RAD;
    const float shoulderRad = packet.servoDegrees[0] * DEG_TO_RAD;
    const float elbowRad = packet.servoDegrees[1] * DEG_TO_RAD;
    const float extensionMm = packet.extensionCentimeters * 10.0f;
    const float forearmLen = elbowBaseLen + extensionMm;
    const float elbowAngleAbs = shoulderRad - elbowRad;

    const float shoulderX = shoulderLen * cosf(shoulderRad) * cosf(baseYawRad);
    const float shoulderZ = shoulderLen * cosf(shoulderRad) * sinf(baseYawRad);
    const float shoulderY = shoulderLen * sinf(shoulderRad);

    const float elbowX = shoulderX + forearmLen * cosf(elbowAngleAbs) * cosf(baseYawRad);
    const float elbowZ = shoulderZ + forearmLen * cosf(elbowAngleAbs) * sinf(baseYawRad);
    const float elbowY = shoulderY + forearmLen * sinf(elbowAngleAbs);

    const float pitchRad = packet.servoDegrees[2] * DEG_TO_RAD;
    const float yawRad = packet.servoDegrees[4] * DEG_TO_RAD;
    IKEngine::Vec3 dir{
        cosf(pitchRad) * cosf(yawRad),
        cosf(pitchRad) * sinf(yawRad),
        sinf(pitchRad)
    };

    IKEngine::Vec3 wrist{
        elbowX + dir.x * wristLen,
        elbowY + dir.y * wristLen,
        elbowZ + dir.z * wristLen
    };
    return wrist;
}

bool acquirePeripheralCommand(PeripheralCommand& out) {
    const uint32_t now = millis();
    if (!peripheralDirty && (now - lastPeripheralSendMs) < kPeripheralIntervalMs) {
        return false;
    }
    lastPeripheralSendMs = now;
    peripheralDirty = false;
    thegillPeripheralCommand.magic = THEGILL_PERIPHERAL_MAGIC;
    out = thegillPeripheralCommand;
    return true;
}

bool acquireConfigurationPacket(ConfigurationPacket& out) {
    const uint32_t now = millis();
    if (!configDirty) {
        return false;
    }
    if (lastConfigSendMs != 0 && (now - lastConfigSendMs) < kConfigMinIntervalMs) {
        return false;
    }
    syncConfigurationPacket();
    configDirty = false;
    lastConfigSendMs = now;
    out = thegillConfigurationPacket;
    return true;
}

bool acquireArmCommand(ArmControlCommand& out) {
    if (mechIaneMode == MechIaneMode::DriveMode) {
        return false;
    }
    const uint32_t now = millis();
    if (!armCommandDirty && (now - lastArmCommandSendMs) < kArmCommandMinIntervalMs) {
        return false;
    }
    lastArmCommandSendMs = now;
    armCommand.magic = ARM_COMMAND_MAGIC;
    out = armCommand;
    armCommandDirty = false;
    return true;
}

void processStatusPacket(const StatusPacket& packet) {
    if (packet.magic != THEGILL_STATUS_MAGIC) {
        return;
    }
    thegillStatusPacket = packet;
    lastStatusPacketMs = millis();

    auto normalize = [](int16_t value) {
        return constrain(static_cast<float>(value) / kMaxWheelSpeedMmPerSec, -1.2f, 1.2f);
    };

    thegillRuntime.actualLeftFront = normalize(packet.wheelSpeedMmPerSec[0]);
    thegillRuntime.actualLeftRear = normalize(packet.wheelSpeedMmPerSec[1]);
    thegillRuntime.actualRightFront = normalize(packet.wheelSpeedMmPerSec[2]);
    thegillRuntime.actualRightRear = normalize(packet.wheelSpeedMmPerSec[3]);

    thegillRuntime.batteryMillivolts = packet.batteryMillivolts;
    thegillRuntime.commandAgeMs = packet.commandAgeMs;
    memcpy(thegillRuntime.ledPwm, packet.ledPwm, sizeof(packet.ledPwm));
    thegillRuntime.pumpDuty = packet.pumpDuty;
    thegillRuntime.userMask = packet.userMask;
    thegillRuntime.statusFlags = packet.flags;

    thegillRuntime.armOutputsEnabled = (packet.flags & StatusFlag::ArmOutputsEnabled) != 0;
    thegillRuntime.failsafeEnabled = (packet.flags & StatusFlag::FailsafeArmed) != 0;
    thegillRuntime.telemetryEnabled = (packet.flags & StatusFlag::TelemetryOn) != 0;
    thegillRuntime.driveEnabled = (packet.flags & StatusFlag::DriveArmed) != 0;
    thegillRuntime.driveSpeedScalar = driveSpeedScalar;
    thegillRuntime.easingRate = thegillConfig.easingRate;
    thegillRuntime.gripperTarget = gripperTarget;
    thegillRuntime.cameraView = armCameraView;
}

void processArmStatePacket(const ArmStatePacket& packet) {
    if (packet.magic != THEGILL_ARM_STATE_MAGIC) {
        return;
    }
    thegillArmStatePacket = packet;
    lastArmStatePacketMs = millis();
    armStateSynced = true;
    requestArmStatePending = false;

    IKEngine::Vec3 wristTarget = estimateToolPosition(packet);
    manualBaseYawDeg = packet.baseDegrees;
    manualExtensionMm = packet.extensionCentimeters * 10.0f;
    setPlanarTargetFromWorld(wristTarget, manualBaseYawDeg);

    const float pitchRad = packet.servoDegrees[2] * DEG_TO_RAD;
    const float yawRad = packet.servoDegrees[4] * DEG_TO_RAD;
    targetOrientation.x = cosf(pitchRad) * cosf(yawRad);
    targetOrientation.y = cosf(pitchRad) * sinf(yawRad);
    targetOrientation.z = sinf(pitchRad);
    targetOrientation = IKEngine::InverseKinematics::normalise(targetOrientation);

    orientationPitchRad = pitchRad;
    orientationYawRad = yawRad;
    orientationAnglesValid = true;
    targetToolRollDeg = packet.servoDegrees[3];

    armCommand.extensionMillimeters = manualExtensionMm;
    armCommand.baseDegrees = manualBaseYawDeg;
    armCommand.shoulderDegrees = packet.servoDegrees[0];
    armCommand.elbowDegrees = packet.servoDegrees[1];
    armCommand.pitchDegrees = packet.servoDegrees[2];
    armCommand.rollDegrees = packet.servoDegrees[3];
    armCommand.yawDegrees = packet.servoDegrees[4];
    manualPitchDeg = armCommand.pitchDegrees;
    manualYawDeg = armCommand.yawDegrees;
    manualRollDeg = armCommand.rollDegrees;
    armCommand.gripper1Degrees = kGripperNeutralDeg;
    armCommand.gripper2Degrees = kGripperNeutralDeg;
    armCommand.validMask = ArmCommandMask::AllServos |
                           ArmCommandMask::Extension |
                           ArmCommandMask::Base |
                           ArmCommandMask::AllGrippers;
    armCommand.flags = (packet.flags & 0x01) ? ArmCommandFlag::EnableOutputs : 0;
    armCommandDirty = false;
    peripheralDirty = true;
}

void queueStatusRequest() {
    requestStatusPulse = true;
}

void forceArmStateResync() {
    armStateSynced = false;
    requestArmStatePending = true;
    lastArmStateRequestMs = 0;
    latchSystemCommand(GillSystemCommand_RequestArmState);
    armCommand.flags |= ArmCommandFlag::EnableOutputs;
    armCommandDirty = true;
}

bool isArmStateSynced() {
    return armStateSynced;
}

void onThegillPaired() {
    driveEnabled = true;
    failsafeEnabled = true;
    armOutputsEnabled = false;
    buzzerEnabled = true;
    syncConfigurationPacket();
    configDirty = true;
    peripheralDirty = true;
    queueStatusRequest();
    forceArmStateResync();
}

void onThegillUnpaired() {
    memset(&thegillCommand.leftFront, 0, sizeof(int16_t) * 4);
    thegillCommand.flags = 0;
    thegillCommand.system = 0;
    lastLeftCommand = 0.0f;
    lastRightCommand = 0.0f;
    armCommandDirty = false;
    armStateSynced = false;
    requestArmStatePending = false;
    armCommand.validMask = 0;
    thegillRuntime = {};
    thegillRuntime.driveSpeedScalar = driveSpeedScalar;
    thegillRuntime.easingRate = thegillConfig.easingRate;
    thegillRuntime.gripperTarget = gripperTarget;
    thegillRuntime.cameraView = armCameraView;
    thegillRuntime.pumpDuty = thegillPeripheralCommand.pumpDuty;
    peripheralDirty = true;
}

void markThegillConfigDirty() {
    configDirty = true;
}


