#pragma once

#include <Arduino.h>

// Forward declarations
namespace IKEngine {
    struct Vec3;
    class InverseKinematics;
}

constexpr uint32_t THEGILL_PACKET_MAGIC = 0x54474C4C; // 'TGLL'

enum GillCommandFlag : uint8_t {
    GillCommandFlag_Brake = 0x01,
    GillCommandFlag_Honk  = 0x02,
};

enum GillSystemCommand : uint8_t {
    GillSystemCommand_EnableTelemetry  = 0x01,
    GillSystemCommand_DisableTelemetry = 0x02,
    GillSystemCommand_EnableBuzzer     = 0x04,
    GillSystemCommand_DisableBuzzer    = 0x08,
    GillSystemCommand_RequestStatus    = 0x10,
    GillSystemCommand_RequestArmState  = 0x20,
};

struct ThegillCommand {
  uint32_t magic;
  int16_t leftFront;
  int16_t leftRear;
  int16_t rightFront;
  int16_t rightRear;
  uint8_t flags;    // GillCommandFlag bits
  uint8_t system;   // GillSystemCommand bits
} __attribute__((packed));

// Local drive configuration (not transmitted verbatim)
enum class GillDriveProfile : uint8_t {
  Tank = 0,
  Differential = 1,
};

enum class GillDriveEasing : uint8_t {
  None = 0,
  SlewRate,
  Exponential,
  Sine,
  EaseIn,
  EaseOut,
  EaseInOut,
};

struct ThegillConfig {
  GillDriveProfile profile;
  GillDriveEasing easing;
  float easingRate;
};

constexpr uint32_t THEGILL_PERIPHERAL_MAGIC = 0x54475043; // 'TGPC'
struct PeripheralCommand {
    uint32_t magic;
    uint8_t ledPwm[3];
    uint8_t pumpDuty;
    uint8_t userMask;
    uint8_t reserved[3];
} __attribute__((packed));

constexpr uint32_t THEGILL_CONFIG_MAGIC = 0x54474346; // 'TGCF'

enum class DriveEasingMode : uint8_t {
    None = 0,
    SlewRate = 1,
    Exponential = 2,
};

namespace ConfigFlag {
constexpr uint8_t MuteAudio      = 0x01;
constexpr uint8_t DriveEnabled   = 0x02;
constexpr uint8_t ArmOutputs     = 0x04;
constexpr uint8_t FailsafeEnable = 0x08;
} // namespace ConfigFlag

namespace SafetyFlag {
constexpr uint8_t BatterySafety = 0x01;
} // namespace SafetyFlag

struct ConfigurationPacket {
    uint32_t magic;
    uint8_t easingMode;    // DriveEasingMode
    uint8_t easingRate;    // Interpretation depends on easingMode
    uint8_t controlFlags;  // ConfigFlag bits
    uint8_t safetyFlags;   // SafetyFlag bits
    uint16_t batteryCutoffMillivolts;
    uint16_t batteryRecoverMillivolts;
} __attribute__((packed));

constexpr uint32_t THEGILL_SETTINGS_MAGIC = 0x54475350; // 'TGSP'
struct SettingsPacket {
    uint32_t magic;
    char robotName[16];
    char customId[32];
    char wifiSsid[32];
    char wifiPassword[32];
} __attribute__((packed));

constexpr uint32_t THEGILL_ARM_STATE_MAGIC = 0x54474153; // 'TGAS'
struct ArmStatePacket {
    uint32_t magic;
    float baseDegrees;
    float extensionCentimeters;
    float servoDegrees[5];      // Shoulder, Elbow, Pitch, Roll, Yaw
    uint8_t servoEnabledMask;
    uint8_t servoAttachedMask;
    uint8_t flags;              // bit0 => arm outputs enabled, bit1 => servos enabled
} __attribute__((packed));

constexpr uint32_t THEGILL_STATUS_MAGIC = 0x54475354; // 'TGST'
struct StatusPacket {
    uint32_t magic;
    int16_t wheelSpeedMmPerSec[4];
    uint16_t batteryMillivolts;
    uint8_t ledPwm[3];
    uint8_t pumpDuty;
    uint8_t userMask;
    uint8_t flags;          // StatusFlag bits
    uint16_t commandAgeMs;
} __attribute__((packed));

namespace StatusFlag {
constexpr uint8_t ArmOutputsEnabled = 0x01;
constexpr uint8_t FailsafeArmed     = 0x02;
constexpr uint8_t TelemetryOn       = 0x04;
constexpr uint8_t TcpClientActive   = 0x08;
constexpr uint8_t SerialActive      = 0x10;
constexpr uint8_t PairedLink        = 0x20;
constexpr uint8_t BatteryLatch      = 0x40;
constexpr uint8_t DriveArmed        = 0x80;
} // namespace StatusFlag

enum class GripperControlTarget : uint8_t;
enum class ArmCameraView : uint8_t;

struct ThegillRuntime {
  float targetLeftFront;
  float targetLeftRear;
  float targetRightFront;
  float targetRightRear;
  float actualLeftFront;
  float actualLeftRear;
  float actualRightFront;
  float actualRightRear;
  bool brakeActive;
  bool honkActive;
  bool driveEnabled;
  bool failsafeEnabled;
  bool telemetryEnabled;
  bool armOutputsEnabled;
  uint16_t batteryMillivolts;
  uint16_t commandAgeMs;
  uint8_t ledPwm[3];
  uint8_t pumpDuty;
  uint8_t userMask;
  uint8_t statusFlags;
  float driveSpeedScalar;
  float easingRate;
  GripperControlTarget gripperTarget;
  ArmCameraView cameraView;
};

extern ThegillCommand thegillCommand;
extern PeripheralCommand thegillPeripheralCommand;
extern ConfigurationPacket thegillConfigurationPacket;
extern SettingsPacket thegillSettingsPacket;
extern StatusPacket thegillStatusPacket;
extern ArmStatePacket thegillArmStatePacket;
extern ThegillConfig thegillConfig;
extern ThegillRuntime thegillRuntime;

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
constexpr uint16_t Base       = 1u << 8;
constexpr uint16_t AllServos  = Shoulder | Elbow | Pitch | Roll | Yaw;
constexpr uint16_t AllGrippers = Gripper1 | Gripper2;
constexpr uint16_t AllOutputs = Extension | Base | AllServos | AllGrippers;
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
    float baseDegrees;
    float shoulderDegrees;
    float elbowDegrees;
    float pitchDegrees;
    float rollDegrees;
    float yawDegrees;
    float gripper1Degrees;   // First gripper finger command (-1=open, +1=close)
    float gripper2Degrees;   // Second gripper finger command (-1=open, +1=close)
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

// Camera views for 3D arm visualization
enum class ArmCameraView : uint8_t {
    TopLeftCorner = 0,   ///< Original isometric
    ThirdPerson = 1,     ///< Low-height behind-the-arm view
    Overhead = 2,        ///< Top-down view
    Side = 3,            ///< Side profile view
    Front = 4,           ///< Front view
    OperatorLeft = 5,    ///< Operator standing on left
    OperatorRight = 6,   ///< Operator standing on right
    ToolTip = 7,         ///< Camera looking back from the tool tip
    OrbitHigh = 8,       ///< Elevated orbit looking down on the arm
    ShoulderClose = 9,   ///< Close-up around the shoulder joint
    RearQuarter = 10     ///< Over-the-shoulder rear-quarter view
};

enum class GripperControlTarget : uint8_t {
    Both = 0,
    Left = 1,
    Right = 2
};

extern ArmControlCommand armCommand;
extern MechIaneMode mechIaneMode;
extern ArmCameraView armCameraView;

// Expose IK state for visualization
extern IKEngine::Vec3 targetPosition;
extern IKEngine::Vec3 targetOrientation;
extern IKEngine::InverseKinematics ikSolver;

void setPrecisionMode(bool enabled);
bool isPrecisionModeEnabled();
void requestOrientationRetarget();
void setTargetToolRoll(float degrees);
float getTargetToolRoll();
void toggleUnifiedGripper();
void setUnifiedGripper(bool open);
bool isGripperOpen();
void setGripperFingerPosition(uint8_t fingerIndex, float degrees);
void setExtensionEnabled(bool enabled);
bool isExtensionEnabled();
bool acquirePeripheralCommand(PeripheralCommand& out);
bool acquireConfigurationPacket(ConfigurationPacket& out);
bool acquireSettingsPacket(SettingsPacket& out);
bool acquireArmCommand(ArmControlCommand& out);
void processStatusPacket(const StatusPacket& packet);
void processArmStatePacket(const ArmStatePacket& packet);
void queueStatusRequest();
void forceArmStateResync();
bool isArmStateSynced();
void onThegillPaired();
void onThegillUnpaired();
void markThegillConfigDirty();
void markThegillSettingsDirty();
void cycleControlMode();
void cycleArmCameraView(int delta);
