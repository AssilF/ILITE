#include "thegill.h"
#include "ILITEModule.h"
#include "ModuleRegistry.h"
#include "InputManager.h"
#include "DisplayCanvas.h"
#include "InverseKinematics.h"
#include "input.h"
#include <math.h>

// Helper to convert InputManager normalized values (-1.0 to +1.0) to motor range
static inline int16_t normalizedToMotor(float normalized) {
    return static_cast<int16_t>(normalized * 32767.0f);
}

// Helper to apply easing curve to normalized input (-1.0 to +1.0)
static inline float applyEasing(float input, GillEasing easing) {
    if (easing == GillEasing::None || fabsf(input) < 0.001f) {
        return input;
    }

    // Get sign and work with absolute value
    float sign = (input < 0.0f) ? -1.0f : 1.0f;
    float absValue = fabsf(input);

    // Apply easing curve (0.0 to 1.0 range)
    float eased = applyEasingCurve(easing, absValue);

    // Restore sign
    return eased * sign;
}

ThegillCommand thegillCommand{
  THEGILL_PACKET_MAGIC,
  0, 0, 0, 0,
  4.0f,
  GillMode::Default,
  kDefaultGillEasing,
  0,
  0
};

ThegillConfig thegillConfig{
  GillMode::Default,
  kDefaultGillEasing
};

ThegillRuntime thegillRuntime{
  0.f, 0.f, 0.f, 0.f,
  0.f, 0.f, 0.f, 0.f,
  4.0f,
  false,
  false
};

ThegillTelemetryPacket thegillTelemetryPacket{
  THEGILL_PACKET_MAGIC,
  0.f, 0.f, 0.f,
  0.f, 0.f, 0.f,
  0,
  0, 0, 0,
  0.f,
  0.f,
  0
};

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
static bool precisionMode = false;
static bool orientationAnglesValid = false;
static float orientationPitchRad = 0.0f;
static float orientationYawRad = 0.0f;
static float targetToolRollDeg = 90.0f;


float applyEasingCurve(GillEasing mode, float t){
  if(t <= 0.f) return 0.f;
  if(t >= 1.f) return 1.f;
  switch(mode){
    case GillEasing::None:
      return 1.f;
    case GillEasing::Linear:
      return t;
    case GillEasing::EaseIn:
      return t * t;
    case GillEasing::EaseOut:
      return 1.f - powf(1.f - t, 2.f);
    case GillEasing::EaseInOut:
      if(t < 0.5f){
        return 2.f * t * t;
      }
      return 1.f - powf(-2.f * t + 2.f, 2.f) / 2.f;
    case GillEasing::Sine:
    default:
      return sinf((t * PI) / 2.f);
  }
}

// ============================================================================
// TheGill Control Update (called at 50Hz)
// ============================================================================

void updateThegillControl() {
    // Mode switching is now handled by ControlBindingSystem in ModuleRegistration.cpp
    const InputManager& inputs = InputManager::getInstance();
    const float potValue = inputs.getPotentiometer();
    const float precisionScalar = precisionMode ? 0.4f : 1.0f;
    const float adaptiveScalar = (0.35f + potValue * 0.65f) * precisionScalar;

    // ========================================================================
    // DRIVE MODE - Normal drivetrain control
    // ========================================================================
    if (mechIaneMode == MechIaneMode::DriveMode) {

        if (thegillConfig.mode == GillMode::Default) {
            // Tank drive mode: Left joystick Y = left motors, Right joystick Y = right motors
            // InputManager provides -1.0 to +1.0 with deadzone and filtering applied
            float leftY = inputs.getJoystickA_Y();
            float rightY = inputs.getJoystickB_Y();

            // Apply easing curve on controller side
            leftY = applyEasing(leftY, thegillConfig.easing);
            rightY = applyEasing(rightY, thegillConfig.easing);
            leftY = constrain(leftY * adaptiveScalar, -1.0f, 1.0f);
            rightY = constrain(rightY * adaptiveScalar, -1.0f, 1.0f);

            // Convert to motor range (-32767 to +32767)
            int16_t leftMotor = normalizedToMotor(leftY);
            int16_t rightMotor = normalizedToMotor(rightY);

            // Update command (both front and rear get same value for each side)
            thegillCommand.leftFront = leftMotor;
            thegillCommand.leftRear = leftMotor;
            thegillCommand.rightFront = rightMotor;
            thegillCommand.rightRear = rightMotor;

        } else {
            // Differential mode: One joystick controls both forward/back and turning
            // InputManager provides -1.0 to +1.0 with deadzone and filtering applied
            float throttle = inputs.getJoystickA_Y();
            float turn = inputs.getJoystickA_X();

            // Apply easing curve on controller side
            throttle = applyEasing(throttle, thegillConfig.easing);
            turn = applyEasing(turn, thegillConfig.easing);
            throttle *= adaptiveScalar;
            turn *= adaptiveScalar;

            // Differential drive calculation
            float leftMotor = constrain(throttle + turn, -1.0f, 1.0f);
            float rightMotor = constrain(throttle - turn, -1.0f, 1.0f);

            // Convert to motor range
            thegillCommand.leftFront = normalizedToMotor(leftMotor);
            thegillCommand.leftRear = normalizedToMotor(leftMotor);
            thegillCommand.rightFront = normalizedToMotor(rightMotor);
            thegillCommand.rightRear = normalizedToMotor(rightMotor);
        }

        // Update config in command packet
        thegillCommand.mode = thegillConfig.mode;
        thegillCommand.easing = thegillConfig.easing;
        thegillCommand.easingRate = thegillRuntime.easingRate;

        // Button states are handled by ControlBindingSystem
    }

    // ========================================================================
    // ARM CONTROL MODE - Mech'Iane control with IK
    // ========================================================================
    else {
        // Stop drivetrain motors when in arm mode
        thegillCommand.leftFront = 0;
        thegillCommand.leftRear = 0;
        thegillCommand.rightFront = 0;
        thegillCommand.rightRear = 0;

        // XYZ/Orientation toggle is now handled by ControlBindingSystem in ModuleRegistration.cpp

        // Read joysticks using InputManager (provides -1.0 to +1.0 with filtering and deadzone)
        float joyAX = inputs.getJoystickA_X();
        float joyAY = inputs.getJoystickA_Y();
        float joyBY = inputs.getJoystickB_Y();

        if (mechIaneMode == MechIaneMode::ArmXYZ) {
            // XYZ Position Control
            // Joystick A X -> X axis (left/right)
            // Joystick A Y -> Y axis (forward/back)
            // Joystick B Y -> Z axis (up/down)

            const float positionSpeed = 8.0f * adaptiveScalar; // mm per frame at max joystick
            targetPosition.x += joyAX * positionSpeed;
            targetPosition.y += joyAY * positionSpeed;
            targetPosition.z += joyBY * positionSpeed;

            // Clamp target position to reasonable workspace
            targetPosition.x = constrain(targetPosition.x, 50.0f, 450.0f);
            targetPosition.y = constrain(targetPosition.y, -300.0f, 300.0f);
            targetPosition.z = constrain(targetPosition.z, 0.0f, 400.0f);

        } else {
            // Orientation Control (Pitch/Yaw/Roll)
            // Joystick A X -> Yaw
            // Joystick A Y -> Pitch
            // Joystick B Y -> Roll

            if (!orientationAnglesValid) {
                float length = sqrtf(targetOrientation.x * targetOrientation.x +
                                   targetOrientation.y * targetOrientation.y +
                                   targetOrientation.z * targetOrientation.z);
                if (length < 0.001f) {
                    length = 1.0f;
                    targetOrientation = {0.0f, 0.0f, 1.0f};
                }
                orientationPitchRad = asinf(targetOrientation.z / length);
                orientationYawRad = atan2f(targetOrientation.y, targetOrientation.x);
                orientationAnglesValid = true;
            }

            const float orientSpeed = 0.08f * adaptiveScalar; // radians per frame at max joystick
            const float rollSpeedDeg = 10.0f * (0.5f + potValue) * precisionScalar;

            orientationPitchRad += joyAY * orientSpeed;
            orientationYawRad += joyAX * orientSpeed;
            orientationPitchRad = constrain(orientationPitchRad, -PI / 2.0f + 0.1f, PI / 2.0f - 0.1f);

            targetToolRollDeg = constrain(targetToolRollDeg + joyBY * rollSpeedDeg, 0.0f, 180.0f);

            targetOrientation.x = cosf(orientationPitchRad) * cosf(orientationYawRad);
            targetOrientation.y = cosf(orientationPitchRad) * sinf(orientationYawRad);
            targetOrientation.z = sinf(orientationPitchRad);
        }

        // Solve IK for current target
        IKEngine::IKSolution solution;
        ikSolver.solve(targetPosition, targetOrientation, solution);

        // Update arm command with IK solution
        armCommand.extensionMillimeters = solution.joints.elbowExtensionMm;
        armCommand.shoulderDegrees = solution.joints.shoulderDeg;
        armCommand.elbowDegrees = solution.joints.elbowDeg;
        armCommand.pitchDegrees = solution.joints.gripperPitchDeg;
        armCommand.rollDegrees = targetToolRollDeg;
        armCommand.yawDegrees = solution.joints.gripperYawDeg;

        armCommand.validMask = ArmCommandMask::AllServos | ArmCommandMask::Extension;

        // Gripper control is now handled by ControlBindingSystem in ModuleRegistration.cpp
        // TODO: Implement gripper control bindings in ModuleRegistration.cpp
    }

    thegillRuntime.brakeActive = (thegillCommand.flags & GILL_FLAG_BRAKE) != 0;
    thegillRuntime.honkActive = (thegillCommand.flags & GILL_FLAG_HONK) != 0;
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


