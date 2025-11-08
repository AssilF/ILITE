#pragma once

#include <Arduino.h>

// Forward declarations
namespace IKEngine {
    struct Vec3;
    class InverseKinematics;
}

constexpr uint32_t THEGILL_PACKET_MAGIC = 0x54474C4C; // 'TGLL'

// Operating mode for The'gill S3 drivetrain.
enum class GillMode : uint8_t {
  Default = 0,
  Differential = 1,
};

// Available easing curves that shape motor ramp behaviour.
enum class GillEasing : uint8_t {
  None = 0,
  Linear,
  EaseIn,
  EaseOut,
  EaseInOut,
  Sine,
};

constexpr uint8_t kGillEasingCount = static_cast<uint8_t>(GillEasing::Sine) + 1;

constexpr GillEasing kDefaultGillEasing = GillEasing::None;

constexpr uint8_t GILL_FLAG_BRAKE = 0x01;
constexpr uint8_t GILL_FLAG_HONK  = 0x02;

struct ThegillCommand {
  uint32_t magic;
  int16_t leftFront;
  int16_t leftRear;
  int16_t rightFront;
  int16_t rightRear;
  float easingRate;
  GillMode mode;
  GillEasing easing;
  uint8_t flags;
  uint8_t reserved;
} __attribute__((packed));

struct ThegillConfig {
  GillMode mode;
  GillEasing easing;
};

struct ThegillRuntime {
  float targetLeftFront;
  float targetLeftRear;
  float targetRightFront;
  float targetRightRear;
  float actualLeftFront;
  float actualLeftRear;
  float actualRightFront;
  float actualRightRear;
  float easingRate;
  bool brakeActive;
  bool honkActive;
};

struct ThegillTelemetryPacket {
  uint32_t magic;
  float pitch;
  float roll;
  float yaw;
  float pitchCorrection;
  float rollCorrection;
  float yawCorrection;
  uint16_t throttle;
  int8_t pitchCommand;
  int8_t rollCommand;
  int8_t yawCommand;
  float altitude;
  float verticalAcc;
  uint32_t commandAge;
} __attribute__((packed));

extern ThegillCommand thegillCommand;
extern ThegillConfig thegillConfig;
extern ThegillRuntime thegillRuntime;
extern ThegillTelemetryPacket thegillTelemetryPacket;

float applyEasingCurve(GillEasing mode, float t);
void updateThegillControl();

// ============================================================================
// Mech'Iane Arm Control
// ============================================================================

constexpr uint32_t ARM_COMMAND_MAGIC = 0x54474152; // 'TGAR'

namespace ArmCommandMask {
constexpr uint16_t Extension  = 1u << 0;
constexpr uint16_t Shoulder   = 1u << 1;
constexpr uint16_t Elbow      = 1u << 2;
constexpr uint16_t Pitch      = 1u << 3;
constexpr uint16_t Roll       = 1u << 4;
constexpr uint16_t Yaw        = 1u << 5;
constexpr uint16_t Gripper1   = 1u << 6;
constexpr uint16_t Gripper2   = 1u << 7;
constexpr uint16_t AllServos  = Shoulder | Elbow | Pitch | Roll | Yaw;
constexpr uint16_t AllGrippers = Gripper1 | Gripper2;
constexpr uint16_t AllOutputs = Extension | AllServos | AllGrippers;
} // namespace ArmCommandMask

namespace ArmCommandFlag {
constexpr uint8_t EnableOutputs  = 0x01;
constexpr uint8_t DisableOutputs = 0x02;
constexpr uint8_t EnableServos   = 0x04;
constexpr uint8_t DisableServos  = 0x08;
} // namespace ArmCommandFlag

struct ArmControlCommand {
    uint32_t magic;
    float extensionMillimeters;
    float shoulderDegrees;
    float elbowDegrees;
    float pitchDegrees;
    float rollDegrees;
    float yawDegrees;
    float gripper1Degrees;   // First gripper finger
    float gripper2Degrees;   // Second gripper finger
    uint16_t validMask;
    uint8_t flags;
    uint8_t reserved;
} __attribute__((packed));

// Control modes for Mech'Iane
enum class MechIaneMode : uint8_t {
    DriveMode = 0,    // Normal drivetrain control
    ArmXYZ = 1,       // XYZ position control with IK
    ArmOrientation = 2, // Pitch/Yaw/Roll orientation control
};

extern ArmControlCommand armCommand;
extern MechIaneMode mechIaneMode;

// Expose IK state for visualization
extern IKEngine::Vec3 targetPosition;
extern IKEngine::Vec3 targetOrientation;
extern IKEngine::InverseKinematics ikSolver;

