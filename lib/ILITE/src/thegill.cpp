#include "thegill.h"
#include "ILITEModule.h"
#include "ModuleRegistry.h"
#include "InputManager.h"
#include "DisplayCanvas.h"
#include "InverseKinematics.h"
#include "input.h"
#include <math.h>
#include <cstring>

namespace {

constexpr int16_t kMaxWheelCommand = 1000;
constexpr float kMinDriveScale = 0.35f;
constexpr float kMaxDriveScale = 1.0f;
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
  0
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

static void refreshPeripheralState(float driveMagnitude, float potValue) {
    bool changed = false;
    const uint8_t driveLed = static_cast<uint8_t>(constrain(driveMagnitude, 0.0f, 1.0f) * 255.0f);
    const uint8_t armLed = isArmStateSynced() ? 220 : 90;
    const uint8_t gripperLed = isGripperOpen() ? 230 : 80;

    changed |= updateByte(thegillPeripheralCommand.ledPwm[0], driveLed);
    changed |= updateByte(thegillPeripheralCommand.ledPwm[1], armLed);
    changed |= updateByte(thegillPeripheralCommand.ledPwm[2], gripperLed);

    const uint8_t pumpDuty = static_cast<uint8_t>(constrain(potValue, 0.0f, 1.0f) * 200.0f);
    changed |= updateByte(thegillPeripheralCommand.pumpDuty, pumpDuty);

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
ArmControlCommand armCommand{
  ARM_COMMAND_MAGIC,
  0.0f, 90.0f, 90.0f, 90.0f, 90.0f, 90.0f, 90.0f, 90.0f,
  0,
  0,
  0
};

MechIaneMode mechIaneMode = MechIaneMode::DriveMode;
ArmCameraView armCameraView = ArmCameraView::TopLeftCorner;

// Mech'Iane control state variables (non-static for external access)
IKEngine::Vec3 targetPosition{200.0f, 0.0f, 150.0f};  // Target XYZ in mm
IKEngine::Vec3 targetOrientation{0.0f, 0.0f, 1.0f};   // Tool direction vector
IKEngine::InverseKinematics ikSolver;
static bool gripperOpen = false;
static bool lastButton3State = false;
static bool lastJoyBtnAState = false;
static float gripperAngleOpen = 45.0f;
static float gripperAngleClosed = 90.0f;
static bool orientationAnglesValid = false;
static float orientationPitchRad = 0.0f;
static float orientationYawRad = 0.0f;
static float targetToolRollDeg = 90.0f;

// ============================================================================
// TheGill Control Update (called at 50Hz)
// ============================================================================

void updateThegillControl() {
    const InputManager& inputs = InputManager::getInstance();
    const float potValue = inputs.getPotentiometer();
    const float precisionScalar = precisionMode ? 0.45f : 1.0f;
    const float adaptiveScalar =
        (kMinDriveScale + (kMaxDriveScale - kMinDriveScale) * potValue) * precisionScalar;
    const uint32_t now = millis();

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

    if (mechIaneMode == MechIaneMode::DriveMode) {
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

        refreshPeripheralState(fmaxf(fabsf(left), fabsf(right)), potValue);
    } else {
        thegillCommand.leftFront = 0;
        thegillCommand.leftRear = 0;
        thegillCommand.rightFront = 0;
        thegillCommand.rightRear = 0;

        const float joyAX = shapeAxis(inputs.getJoystickA_X());
        const float joyAY = shapeAxis(inputs.getJoystickA_Y());
        const float joyBY = shapeAxis(inputs.getJoystickB_Y());

        if (mechIaneMode == MechIaneMode::ArmXYZ) {
            const float positionSpeed = 9.0f * adaptiveScalar;
            targetPosition.x = constrain(targetPosition.x + joyAX * positionSpeed, 50.0f, 450.0f);
            targetPosition.y = constrain(targetPosition.y + joyAY * positionSpeed, -300.0f, 300.0f);
            targetPosition.z = constrain(targetPosition.z + joyBY * positionSpeed, 0.0f, 420.0f);
        } else {
            if (!orientationAnglesValid) {
                float length = sqrtf(targetOrientation.x * targetOrientation.x +
                                     targetOrientation.y * targetOrientation.y +
                                     targetOrientation.z * targetOrientation.z);
                if (length < 0.001f) {
                    targetOrientation = {0.0f, 0.0f, 1.0f};
                    length = 1.0f;
                }
                orientationPitchRad = asinf(targetOrientation.z / length);
                orientationYawRad = atan2f(targetOrientation.y, targetOrientation.x);
                orientationAnglesValid = true;
            }

            const float orientSpeed = 0.08f * adaptiveScalar;
            const float rollSpeedDeg = 12.0f * (0.5f + potValue) * precisionScalar;

            orientationPitchRad = constrain(orientationPitchRad + joyAY * orientSpeed,
                                            -PI / 2.0f + 0.1f,
                                            PI / 2.0f - 0.1f);
            orientationYawRad += joyAX * orientSpeed;
            targetToolRollDeg = constrain(targetToolRollDeg + joyBY * rollSpeedDeg, 0.0f, 180.0f);

            targetOrientation.x = cosf(orientationPitchRad) * cosf(orientationYawRad);
            targetOrientation.y = cosf(orientationPitchRad) * sinf(orientationYawRad);
            targetOrientation.z = sinf(orientationPitchRad);
        }

        IKEngine::IKSolution solution;
        if (ikSolver.solve(targetPosition, targetOrientation, solution)) {
            armCommand.extensionMillimeters = constrain(solution.joints.elbowExtensionMm, 0.0f, 250.0f);
            armCommand.shoulderDegrees = solution.joints.shoulderDeg;
            armCommand.elbowDegrees = solution.joints.elbowDeg;
            armCommand.pitchDegrees = solution.joints.gripperPitchDeg;
            armCommand.rollDegrees = targetToolRollDeg;
            armCommand.yawDegrees = solution.joints.gripperYawDeg;
            armCommand.validMask = ArmCommandMask::AllServos | ArmCommandMask::Extension;
            armCommand.flags = ArmCommandFlag::EnableOutputs;
            armCommandDirty = true;
        }

        refreshPeripheralState(0.0f, potValue);
    }

    if (requestStatusPulse) {
        latchSystemCommand(GillSystemCommand_RequestStatus);
        requestStatusPulse = false;
    }

    if (requestArmStatePending && (now - lastArmStateRequestMs) >= kArmStateRequestIntervalMs) {
        latchSystemCommand(GillSystemCommand_RequestArmState);
        lastArmStateRequestMs = now;
    }

    thegillCommand.magic = THEGILL_PACKET_MAGIC;
    thegillCommand.system = systemCommandLatch;
    systemCommandLatch = 0;

    thegillRuntime.brakeActive = (thegillCommand.flags & GillCommandFlag_Brake) != 0;
    thegillRuntime.honkActive = (thegillCommand.flags & GillCommandFlag_Honk) != 0;
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
    float shoulderLen = 158.0f;  // From IK config
    float shoulderX = shoulderLen * cosf(shoulderRad) * cosf(baseYawRad);
    float shoulderY = shoulderLen * cosf(shoulderRad) * sinf(baseYawRad);
    float shoulderZ = shoulderLen * sinf(shoulderRad);

    // Elbow joint position
    float forearmLen = 182.0f + solution.joints.elbowExtensionMm;
    float elbowAngleAbs = shoulderRad + elbowRad - PI;  // Absolute elbow angle
    float elbowX = shoulderX + forearmLen * cosf(elbowAngleAbs) * cosf(baseYawRad);
    float elbowY = shoulderY + forearmLen * cosf(elbowAngleAbs) * sinf(baseYawRad);
    float elbowZ = shoulderZ + forearmLen * sinf(elbowAngleAbs);

    // End effector (wrist center)
    float wristLen = 112.0f;  // Total wrist chain length
    float wristX = elbowX + solution.toolDirection.x * wristLen;
    float wristY = elbowY + solution.toolDirection.y * wristLen;
    float wristZ = elbowZ + solution.toolDirection.z * wristLen;

    // Project to 2D
    Point2D pBase = projectIsometric(baseX, baseY, baseZ);
    Point2D pShoulder = projectIsometric(shoulderX, shoulderY, shoulderZ);
    Point2D pElbow = projectIsometric(elbowX, elbowY, elbowZ);
    Point2D pWrist = projectIsometric(wristX, wristY, wristZ);
    Point2D pTarget = projectIsometric(targetPosition.x, targetPosition.y, targetPosition.z);

    // Draw arm links as cylinders for 3D effect
    drawCylinder(canvas, pBase, pShoulder, 2);      // Upper arm
    drawCylinder(canvas, pShoulder, pElbow, 2);     // Forearm
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
    gripperOpen = open;
    const float targetDeg = open ? gripperAngleOpen : gripperAngleClosed;
    armCommand.gripper1Degrees = targetDeg;
    armCommand.gripper2Degrees = targetDeg;
    armCommand.validMask |= ArmCommandMask::AllGrippers;
}

void toggleUnifiedGripper() {
    setUnifiedGripper(!gripperOpen);
}

bool isGripperOpen() {
    return gripperOpen;
}

void setGripperFingerPosition(uint8_t fingerIndex, float degrees) {
    const float clamped = constrain(degrees, 0.0f, 180.0f);
    if (fingerIndex == 0) {
        armCommand.gripper1Degrees = clamped;
        armCommand.validMask |= ArmCommandMask::Gripper1;
    } else {
        armCommand.gripper2Degrees = clamped;
        armCommand.validMask |= ArmCommandMask::Gripper2;
    }
}

static IKEngine::Vec3 estimateToolPosition(const ArmStatePacket& packet) {
    constexpr float shoulderLen = 158.0f;
    constexpr float elbowBaseLen = 182.0f;
    constexpr float wristLen = 112.0f;

    const float baseYawRad = packet.baseDegrees * DEG_TO_RAD;
    const float shoulderRad = packet.servoDegrees[0] * DEG_TO_RAD;
    const float elbowRad = packet.servoDegrees[1] * DEG_TO_RAD;
    const float extensionMm = packet.extensionCentimeters * 10.0f;
    const float forearmLen = elbowBaseLen + extensionMm;
    const float elbowAngleAbs = shoulderRad + elbowRad - PI;

    const float shoulderX = shoulderLen * cosf(shoulderRad) * cosf(baseYawRad);
    const float shoulderY = shoulderLen * cosf(shoulderRad) * sinf(baseYawRad);
    const float shoulderZ = shoulderLen * sinf(shoulderRad);

    const float elbowX = shoulderX + forearmLen * cosf(elbowAngleAbs) * cosf(baseYawRad);
    const float elbowY = shoulderY + forearmLen * cosf(elbowAngleAbs) * sinf(baseYawRad);
    const float elbowZ = shoulderZ + forearmLen * sinf(elbowAngleAbs);

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
    if (!armStateSynced) {
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
}

void processArmStatePacket(const ArmStatePacket& packet) {
    if (packet.magic != THEGILL_ARM_STATE_MAGIC) {
        return;
    }
    thegillArmStatePacket = packet;
    lastArmStatePacketMs = millis();
    armStateSynced = true;
    requestArmStatePending = false;

    targetPosition = estimateToolPosition(packet);

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

    armCommand.extensionMillimeters = packet.extensionCentimeters * 10.0f;
    armCommand.shoulderDegrees = packet.servoDegrees[0];
    armCommand.elbowDegrees = packet.servoDegrees[1];
    armCommand.pitchDegrees = packet.servoDegrees[2];
    armCommand.rollDegrees = packet.servoDegrees[3];
    armCommand.yawDegrees = packet.servoDegrees[4];
    armCommand.validMask = ArmCommandMask::AllServos | ArmCommandMask::Extension;
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
    peripheralDirty = true;
}

void markThegillConfigDirty() {
    configDirty = true;
}


