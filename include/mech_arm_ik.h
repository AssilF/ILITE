#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <math.h>

namespace mech {

struct Vec3 {
  float x;
  float y;
  float z;
  Vec3() : x(0.f), y(0.f), z(0.f) {}
  Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
  Vec3 operator+(const Vec3& other) const;
  Vec3 operator-(const Vec3& other) const;
  Vec3 operator*(float scalar) const;
  Vec3 operator/(float scalar) const;
  Vec3& operator+=(const Vec3& other);
  Vec3& operator-=(const Vec3& other);
  Vec3& operator*=(float scalar);
  float dot(const Vec3& other) const;
  Vec3 cross(const Vec3& other) const;
  float magSq() const;
  float magnitude() const;
  void normalize();
  static float distance(const Vec3& a, const Vec3& b);
};

struct ArmDimensions {
  float baseHeight = 90.0f;
  float baseRadius = 60.0f;
  float shoulderLength = 140.0f;
  float elbowLength = 110.0f;
  float extensionMax = 110.0f;
};

struct ArmLimits {
  float baseYawLimit = radians(135.0f);
  float shoulderMin = radians(-10.0f);
  float shoulderMax = radians(175.0f);
  float elbowMin = radians(0.0f);
  float elbowMax = radians(175.0f);
  float minRadial = 10.0f;
  float minPlanar = 30.0f;
  float gripperAngleMin = radians(-90.0f);
  float gripperAngleMax = radians(90.0f);
};

struct ServoRange {
  float angleMinDeg = 0.0f;
  float angleMaxDeg = 180.0f;
  uint16_t pulseMinUs = 500;
  uint16_t pulseMaxUs = 2500;

  float clampAngleDeg(float angleDeg) const;
  uint16_t pulseFromDegrees(float angleDeg) const;
  uint16_t pulseFromRadians(float angleRad) const;
};

struct LinearServoRange {
  float lengthMin = 0.0f;
  float lengthMax = 110.0f;
  uint16_t pulseMinUs = 1000;
  uint16_t pulseMaxUs = 2000;

  float clampLength(float length) const;
  uint16_t pulseFromLength(float length) const;
};

struct IKStatus {
  bool reachLimited = false;
  bool baseLimited = false;
  bool radialAdjusted = false;
  bool minDistanceApplied = false;
  bool shoulderLimited = false;
  bool elbowLimited = false;
  bool reachable = true;
  float lateralError = 0.0f;
  float distanceError = 0.0f;

  void reset();
};

struct ArmSolution {
  Vec3 basePosition;
  Vec3 desiredTarget;
  Vec3 reachedTarget;
  Vec3 joints[5];
  Vec3 gripperRight;
  Vec3 gripperUp;
  Vec3 gripperForward;
  float baseAngle = 0.0f;
  float shoulderAngle = 0.0f;
  float elbowAngle = 0.0f;
  float wristPitch = 0.0f;
  float wristYaw = 0.0f;
  float wristRoll = 0.0f;
  float extension = 0.0f;
  float gripperYaw = 0.0f;
  float gripperPitch = 0.0f;
  float gripperRoll = 0.0f;
  float gripperOpen = 1.0f;
  bool elbowUp = false;
  IKStatus status;

  float baseAngleDeg() const;
  float shoulderAngleDeg() const;
  float elbowAngleDeg() const;
  float wristPitchDeg() const;
  float wristYawDeg() const;
  float wristRollDeg() const;
  float gripperYawDeg() const;
  float gripperPitchDeg() const;
  float gripperRollDeg() const;
};

struct ArmServoPose {
  float baseDeg = 0.0f;
  float shoulderDeg = 0.0f;
  float elbowDeg = 0.0f;
  float wristPitchDeg = 0.0f;
  float wristYawDeg = 0.0f;
  float wristRollDeg = 0.0f;
  float gripperPitchDeg = 0.0f;
  float extensionMm = 0.0f;
  float gripperOpen = 1.0f;
};

struct ArmServoSetpoints {
  uint16_t baseUs = 1500;
  uint16_t shoulderUs = 1500;
  uint16_t elbowUs = 1500;
  uint16_t wristPitchUs = 1500;
  uint16_t wristYawUs = 1500;
  uint16_t wristRollUs = 1500;
  uint16_t gripperUs = 1500;
  uint16_t extensionUs = 1500;
  float gripperOpen = 1.0f;
};

struct ArmServoMap {
  ServoRange base;
  ServoRange shoulder;
  ServoRange elbow;
  ServoRange wristPitch;
  ServoRange wristYaw;
  ServoRange wristRoll;
  ServoRange gripper;
  LinearServoRange extension;
};

class MechArmIK {
public:
  MechArmIK();
  MechArmIK(const ArmDimensions& dims, const ArmLimits& limits);

  void setDimensions(const ArmDimensions& dims);
  void setLimits(const ArmLimits& limits);

  const ArmDimensions& dimensions() const;
  const ArmLimits& limits() const;

  Vec3 homeTarget() const;
  float maxPlanarReach() const;

  bool solve(const Vec3& desiredTarget, bool elbowUp);

  void adjustGripper(float yawDelta, float pitchDelta, float rollDelta);
  void setGripperAngles(float yawRad, float pitchRad, float rollRad);
  void resetGripperOrientation();

  void setGripperOpenTarget(float open);
  void nudgeGripperOpen(float delta);
  void updateGripperOpen(float dt);

  void adjustShoulderLength(float delta);
  void adjustElbowLength(float delta);
  void adjustExtensionMax(float delta);

  const ArmSolution& solution() const;
  ArmServoPose servoPose() const;
  ArmServoSetpoints servoSetpoints(const ArmServoMap& map) const;

private:
  static float clampf(float value, float minValue, float maxValue);
  static float lerpf(float a, float b, float t);

  ArmDimensions dims_;
  ArmLimits limits_;
  Vec3 basePos_;
  Vec3 target_;
  Vec3 reached_;
  Vec3 joints_[5];
  IKStatus status_;
  ArmSolution currentSolution_;
  float extension_ = 0.0f;
  float gripperYaw_ = 0.0f;
  float gripperPitch_ = 0.0f;
  float gripperRoll_ = 0.0f;
  float gripperOpenAmount_ = 1.0f;
  float gripperOpenTarget_ = 1.0f;
  Vec3 gripperRight_;
  Vec3 gripperUp_;
  Vec3 gripperForward_;

  void updateBasePosition();
  void updateGripperFrame(const Vec3& radialDir, float link2Angle);
  void applyGripperServoRotations();
  void ensureGripperOrthonormal();
};

} // namespace mech

