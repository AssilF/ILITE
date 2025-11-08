#include "mech_arm_ik.h"

#include <math.h>

namespace mech {

namespace {
constexpr float kEpsilon = 1e-6f;

inline float sqr(float v) {
  return v * v;
}

Vec3 rotateAroundAxis(const Vec3& v, const Vec3& axis, float angle) {
  if (fabsf(angle) < kEpsilon) {
    return v;
  }
  Vec3 k = axis;
  float magSq = k.magSq();
  if (magSq < kEpsilon) {
    return v;
  }
  float invMag = 1.0f / sqrtf(magSq);
  k *= invMag;
  float cosA = cosf(angle);
  float sinA = sinf(angle);
  Vec3 term1 = v * cosA;
  Vec3 term2 = k.cross(v) * sinA;
  float dotKV = v.dot(k);
  Vec3 term3 = k * (dotKV * (1.0f - cosA));
  return term1 + term2 + term3;
}

} // namespace

Vec3 Vec3::operator+(const Vec3& other) const {
  return Vec3(x + other.x, y + other.y, z + other.z);
}

Vec3 Vec3::operator-(const Vec3& other) const {
  return Vec3(x - other.x, y - other.y, z - other.z);
}

Vec3 Vec3::operator*(float scalar) const {
  return Vec3(x * scalar, y * scalar, z * scalar);
}

Vec3 Vec3::operator/(float scalar) const {
  if (fabsf(scalar) < kEpsilon) {
    return Vec3();
  }
  float inv = 1.0f / scalar;
  return Vec3(x * inv, y * inv, z * inv);
}

Vec3& Vec3::operator+=(const Vec3& other) {
  x += other.x;
  y += other.y;
  z += other.z;
  return *this;
}

Vec3& Vec3::operator-=(const Vec3& other) {
  x -= other.x;
  y -= other.y;
  z -= other.z;
  return *this;
}

Vec3& Vec3::operator*=(float scalar) {
  x *= scalar;
  y *= scalar;
  z *= scalar;
  return *this;
}

float Vec3::dot(const Vec3& other) const {
  return x * other.x + y * other.y + z * other.z;
}

Vec3 Vec3::cross(const Vec3& other) const {
  return Vec3(
    y * other.z - z * other.y,
    z * other.x - x * other.z,
    x * other.y - y * other.x
  );
}

float Vec3::magSq() const {
  return x * x + y * y + z * z;
}

float Vec3::magnitude() const {
  return sqrtf(magSq());
}

void Vec3::normalize() {
  float magSqValue = magSq();
  if (magSqValue < kEpsilon) {
    x = 0.f;
    y = 0.f;
    z = 0.f;
    return;
  }
  float inv = 1.0f / sqrtf(magSqValue);
  x *= inv;
  y *= inv;
  z *= inv;
}

float Vec3::distance(const Vec3& a, const Vec3& b) {
  return (a - b).magnitude();
}

float ServoRange::clampAngleDeg(float angleDeg) const {
  if (angleDeg < angleMinDeg) {
    return angleMinDeg;
  }
  if (angleDeg > angleMaxDeg) {
    return angleMaxDeg;
  }
  return angleDeg;
}

uint16_t ServoRange::pulseFromDegrees(float angleDeg) const {
  float clamped = clampAngleDeg(angleDeg);
  float span = angleMaxDeg - angleMinDeg;
  if (fabsf(span) < kEpsilon) {
    return static_cast<uint16_t>(roundf((pulseMinUs + pulseMaxUs) * 0.5f));
  }
  float t = (clamped - angleMinDeg) / span;
  if (t < 0.f) {
    t = 0.f;
  } else if (t > 1.f) {
    t = 1.f;
  }
  float pulse = pulseMinUs + t * (pulseMaxUs - pulseMinUs);
  if (pulse < 0.f) {
    pulse = 0.f;
  } else if (pulse > 65535.f) {
    pulse = 65535.f;
  }
  return static_cast<uint16_t>(roundf(pulse));
}

uint16_t ServoRange::pulseFromRadians(float angleRad) const {
  return pulseFromDegrees(angleRad * RAD_TO_DEG);
}

float LinearServoRange::clampLength(float length) const {
  if (length < lengthMin) {
    return lengthMin;
  }
  if (length > lengthMax) {
    return lengthMax;
  }
  return length;
}

uint16_t LinearServoRange::pulseFromLength(float length) const {
  float clamped = clampLength(length);
  float span = lengthMax - lengthMin;
  if (fabsf(span) < kEpsilon) {
    return static_cast<uint16_t>(roundf((pulseMinUs + pulseMaxUs) * 0.5f));
  }
  float t = (clamped - lengthMin) / span;
  if (t < 0.f) {
    t = 0.f;
  } else if (t > 1.f) {
    t = 1.f;
  }
  float pulse = pulseMinUs + t * (pulseMaxUs - pulseMinUs);
  if (pulse < 0.f) {
    pulse = 0.f;
  } else if (pulse > 65535.f) {
    pulse = 65535.f;
  }
  return static_cast<uint16_t>(roundf(pulse));
}

void IKStatus::reset() {
  reachLimited = false;
  baseLimited = false;
  radialAdjusted = false;
  minDistanceApplied = false;
  shoulderLimited = false;
  elbowLimited = false;
  reachable = true;
  lateralError = 0.0f;
  distanceError = 0.0f;
}

float ArmSolution::baseAngleDeg() const {
  return baseAngle * RAD_TO_DEG;
}

float ArmSolution::shoulderAngleDeg() const {
  return shoulderAngle * RAD_TO_DEG;
}

float ArmSolution::elbowAngleDeg() const {
  return elbowAngle * RAD_TO_DEG;
}

float ArmSolution::wristPitchDeg() const {
  return wristPitch * RAD_TO_DEG;
}

float ArmSolution::wristYawDeg() const {
  return wristYaw * RAD_TO_DEG;
}

float ArmSolution::wristRollDeg() const {
  return wristRoll * RAD_TO_DEG;
}

float ArmSolution::gripperYawDeg() const {
  return gripperYaw * RAD_TO_DEG;
}

float ArmSolution::gripperPitchDeg() const {
  return gripperPitch * RAD_TO_DEG;
}

float ArmSolution::gripperRollDeg() const {
  return gripperRoll * RAD_TO_DEG;
}

float MechArmIK::clampf(float value, float minValue, float maxValue) {
  if (minValue > maxValue) {
    float tmp = minValue;
    minValue = maxValue;
    maxValue = tmp;
  }
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

float MechArmIK::lerpf(float a, float b, float t) {
  return a + (b - a) * clampf(t, 0.0f, 1.0f);
}

MechArmIK::MechArmIK() {
  setDimensions(ArmDimensions());
  setLimits(ArmLimits());
  gripperRight_ = Vec3(1.f, 0.f, 0.f);
  gripperUp_ = Vec3(0.f, 1.f, 0.f);
  gripperForward_ = Vec3(0.f, 0.f, 1.f);
  target_ = homeTarget();
  reached_ = target_;
  for (int i = 0; i < 5; ++i) {
    joints_[i] = basePos_;
  }
  currentSolution_.gripperRight = gripperRight_;
  currentSolution_.gripperUp = gripperUp_;
  currentSolution_.gripperForward = gripperForward_;
  solve(target_, false);
}

MechArmIK::MechArmIK(const ArmDimensions& dims, const ArmLimits& limits) {
  setDimensions(dims);
  setLimits(limits);
  gripperRight_ = Vec3(1.f, 0.f, 0.f);
  gripperUp_ = Vec3(0.f, 1.f, 0.f);
  gripperForward_ = Vec3(0.f, 0.f, 1.f);
  target_ = homeTarget();
  reached_ = target_;
  for (int i = 0; i < 5; ++i) {
    joints_[i] = basePos_;
  }
  currentSolution_.gripperRight = gripperRight_;
  currentSolution_.gripperUp = gripperUp_;
  currentSolution_.gripperForward = gripperForward_;
  solve(target_, false);
}

void MechArmIK::setDimensions(const ArmDimensions& dims) {
  dims_.baseHeight = clampf(dims.baseHeight, 10.0f, 600.0f);
  dims_.baseRadius = clampf(dims.baseRadius, 5.0f, 300.0f);
  dims_.shoulderLength = clampf(dims.shoulderLength, 30.0f, 600.0f);
  dims_.elbowLength = clampf(dims.elbowLength, 30.0f, 600.0f);
  dims_.extensionMax = clampf(dims.extensionMax, 0.0f, 400.0f);
  updateBasePosition();
}

void MechArmIK::setLimits(const ArmLimits& limits) {
  limits_ = limits;
  if (limits_.baseYawLimit < 0.f) {
    limits_.baseYawLimit = -limits_.baseYawLimit;
  }
  if (limits_.shoulderMin > limits_.shoulderMax) {
    float tmp = limits_.shoulderMin;
    limits_.shoulderMin = limits_.shoulderMax;
    limits_.shoulderMax = tmp;
  }
  if (limits_.elbowMin > limits_.elbowMax) {
    float tmp = limits_.elbowMin;
    limits_.elbowMin = limits_.elbowMax;
    limits_.elbowMax = tmp;
  }
  if (limits_.gripperAngleMin > limits_.gripperAngleMax) {
    float tmp = limits_.gripperAngleMin;
    limits_.gripperAngleMin = limits_.gripperAngleMax;
    limits_.gripperAngleMax = tmp;
  }
}

const ArmDimensions& MechArmIK::dimensions() const {
  return dims_;
}

const ArmLimits& MechArmIK::limits() const {
  return limits_;
}

Vec3 MechArmIK::homeTarget() const {
  float planar = (dims_.shoulderLength + dims_.elbowLength) * 0.6f;
  float shoulderRise = dims_.shoulderLength * 0.55f;
  if (shoulderRise < 70.0f) {
    shoulderRise = 70.0f;
  }
  float height = basePos_.y + shoulderRise;
  return Vec3(basePos_.x + planar, height, 140.0f);
}

float MechArmIK::maxPlanarReach() const {
  return dims_.shoulderLength + dims_.elbowLength + dims_.extensionMax;
}

bool MechArmIK::solve(const Vec3& desiredTarget, bool elbowUp) {
  status_.reset();
  target_ = desiredTarget;

  Vec3 relative = Vec3(
    desiredTarget.x - basePos_.x,
    desiredTarget.y - basePos_.y,
    desiredTarget.z - basePos_.z
  );

  float rawBaseAngle = atan2f(relative.x, relative.z);
  float baseAngle = clampf(rawBaseAngle, -limits_.baseYawLimit, limits_.baseYawLimit);
  if (fabsf(baseAngle - rawBaseAngle) > 0.001f) {
    status_.baseLimited = true;
  }

  Vec3 radialDir(sinf(baseAngle), 0.f, cosf(baseAngle));
  float radial = relative.x * radialDir.x + relative.z * radialDir.z;
  float lateral = relative.x * cosf(baseAngle) - relative.z * sinf(baseAngle);
  status_.lateralError = lateral;

  if (radial < limits_.minRadial) {
    radial = limits_.minRadial;
    status_.radialAdjusted = true;
  }

  float vertical = relative.y;
  float planar = sqrtf(radial * radial + vertical * vertical);

  if (planar < limits_.minPlanar) {
    if (planar < 1e-3f) {
      radial = limits_.minPlanar;
      vertical = 0.f;
    } else {
      float scale = limits_.minPlanar / planar;
      radial *= scale;
      vertical *= scale;
    }
    planar = limits_.minPlanar;
    status_.minDistanceApplied = true;
  }

  float maxReach = dims_.shoulderLength + dims_.elbowLength + dims_.extensionMax;
  if (planar > maxReach) {
    float scale = maxReach / planar;
    radial *= scale;
    vertical *= scale;
    planar = maxReach;
    status_.reachLimited = true;
  }

  float effectiveL2 = dims_.elbowLength;
  if (planar > dims_.shoulderLength + dims_.elbowLength) {
    effectiveL2 = planar - dims_.shoulderLength;
  }
  effectiveL2 = clampf(effectiveL2, dims_.elbowLength, dims_.elbowLength + dims_.extensionMax);
  extension_ = effectiveL2 - dims_.elbowLength;

  float cosElbow = (sqr(dims_.shoulderLength) + sqr(effectiveL2) - sqr(planar)) / (2.f * dims_.shoulderLength * effectiveL2);
  cosElbow = clampf(cosElbow, -1.0f, 1.0f);
  float elbowInternal = acosf(cosElbow);
  float elbowAngle = PI - elbowInternal;

  float cosShoulder = (sqr(dims_.shoulderLength) + sqr(planar) - sqr(effectiveL2)) / (2.f * dims_.shoulderLength * planar);
  cosShoulder = clampf(cosShoulder, -1.0f, 1.0f);
  float shoulderOffset = acosf(cosShoulder);
  float angleToTarget = atan2f(vertical, radial);
  float shoulderAngle = elbowUp ? angleToTarget + shoulderOffset : angleToTarget - shoulderOffset;

  float clampedShoulder = clampf(shoulderAngle, limits_.shoulderMin, limits_.shoulderMax);
  if (fabsf(clampedShoulder - shoulderAngle) > 1e-4f) {
    status_.shoulderLimited = true;
  }
  shoulderAngle = clampedShoulder;

  float clampedElbow = clampf(elbowAngle, limits_.elbowMin, limits_.elbowMax);
  if (fabsf(clampedElbow - elbowAngle) > 1e-4f) {
    status_.elbowLimited = true;
  }
  elbowAngle = clampedElbow;

  float link2Angle = shoulderAngle + elbowAngle;

  joints_[0] = Vec3(basePos_.x, 0.f, basePos_.z);
  joints_[1] = basePos_;

  float l1Radial = dims_.shoulderLength * cosf(shoulderAngle);
  float l1Vertical = dims_.shoulderLength * sinf(shoulderAngle);
  joints_[2] = Vec3(
    basePos_.x + radialDir.x * l1Radial,
    basePos_.y + l1Vertical,
    basePos_.z + radialDir.z * l1Radial
  );

  float baseL2Radial = dims_.elbowLength * cosf(link2Angle);
  float baseL2Vertical = dims_.elbowLength * sinf(link2Angle);
  joints_[3] = Vec3(
    joints_[2].x + radialDir.x * baseL2Radial,
    joints_[2].y + baseL2Vertical,
    joints_[2].z + radialDir.z * baseL2Radial
  );

  float totalL2Radial = (dims_.elbowLength + extension_) * cosf(link2Angle);
  float totalL2Vertical = (dims_.elbowLength + extension_) * sinf(link2Angle);
  joints_[4] = Vec3(
    joints_[2].x + radialDir.x * totalL2Radial,
    joints_[2].y + totalL2Vertical,
    joints_[2].z + radialDir.z * totalL2Radial
  );

  reached_ = joints_[4];

  float wristPitch = -(shoulderAngle + elbowAngle);
  float wristYaw = 0.f;
  float wristRoll = gripperRoll_;

  updateGripperFrame(radialDir, link2Angle);

  status_.distanceError = Vec3::distance(desiredTarget, reached_);
  status_.reachable = status_.distanceError <= 1.5f;

  currentSolution_.basePosition = basePos_;
  currentSolution_.desiredTarget = target_;
  currentSolution_.reachedTarget = reached_;
  for (int i = 0; i < 5; ++i) {
    currentSolution_.joints[i] = joints_[i];
  }
  currentSolution_.gripperRight = gripperRight_;
  currentSolution_.gripperUp = gripperUp_;
  currentSolution_.gripperForward = gripperForward_;
  currentSolution_.baseAngle = baseAngle;
  currentSolution_.shoulderAngle = shoulderAngle;
  currentSolution_.elbowAngle = elbowAngle;
  currentSolution_.wristPitch = wristPitch;
  currentSolution_.wristYaw = wristYaw;
  currentSolution_.wristRoll = wristRoll;
  currentSolution_.extension = extension_;
  currentSolution_.gripperYaw = gripperYaw_;
  currentSolution_.gripperPitch = gripperPitch_;
  currentSolution_.gripperRoll = gripperRoll_;
  currentSolution_.gripperOpen = gripperOpenAmount_;
  currentSolution_.elbowUp = elbowUp;
  currentSolution_.status = status_;

  return status_.reachable;
}

void MechArmIK::adjustGripper(float yawDelta, float pitchDelta, float rollDelta) {
  gripperYaw_ = clampf(gripperYaw_ + yawDelta, limits_.gripperAngleMin, limits_.gripperAngleMax);
  gripperPitch_ = clampf(gripperPitch_ + pitchDelta, limits_.gripperAngleMin, limits_.gripperAngleMax);
  gripperRoll_ = clampf(gripperRoll_ + rollDelta, limits_.gripperAngleMin, limits_.gripperAngleMax);
}

void MechArmIK::setGripperAngles(float yawRad, float pitchRad, float rollRad) {
  gripperYaw_ = clampf(yawRad, limits_.gripperAngleMin, limits_.gripperAngleMax);
  gripperPitch_ = clampf(pitchRad, limits_.gripperAngleMin, limits_.gripperAngleMax);
  gripperRoll_ = clampf(rollRad, limits_.gripperAngleMin, limits_.gripperAngleMax);
}

void MechArmIK::resetGripperOrientation() {
  gripperYaw_ = 0.f;
  gripperPitch_ = 0.f;
  gripperRoll_ = 0.f;
}

void MechArmIK::setGripperOpenTarget(float open) {
  gripperOpenTarget_ = clampf(open, 0.0f, 1.0f);
}

void MechArmIK::nudgeGripperOpen(float delta) {
  setGripperOpenTarget(gripperOpenTarget_ + delta);
}

void MechArmIK::updateGripperOpen(float dt) {
  float chaseSpeed = 6.0f;
  float diff = gripperOpenTarget_ - gripperOpenAmount_;
  if (fabsf(diff) > 1e-4f) {
    float step = clampf(diff, -chaseSpeed * dt, chaseSpeed * dt);
    gripperOpenAmount_ += step;
  }
}

void MechArmIK::adjustShoulderLength(float delta) {
  dims_.shoulderLength = clampf(dims_.shoulderLength + delta, 80.0f, 260.0f);
}

void MechArmIK::adjustElbowLength(float delta) {
  dims_.elbowLength = clampf(dims_.elbowLength + delta, 60.0f, 220.0f);
}

void MechArmIK::adjustExtensionMax(float delta) {
  dims_.extensionMax = clampf(dims_.extensionMax + delta, 30.0f, 220.0f);
  extension_ = clampf(extension_, 0.0f, dims_.extensionMax);
}

const ArmSolution& MechArmIK::solution() const {
  return currentSolution_;
}

ArmServoPose MechArmIK::servoPose() const {
  ArmServoPose pose;
  pose.baseDeg = currentSolution_.baseAngleDeg();
  pose.shoulderDeg = currentSolution_.shoulderAngleDeg();
  pose.elbowDeg = currentSolution_.elbowAngleDeg();
  pose.wristPitchDeg = currentSolution_.wristPitchDeg();
  pose.wristYawDeg = currentSolution_.gripperYawDeg();
  pose.wristRollDeg = currentSolution_.gripperRollDeg();
  pose.gripperPitchDeg = currentSolution_.gripperPitchDeg();
  pose.extensionMm = currentSolution_.extension;
  pose.gripperOpen = currentSolution_.gripperOpen;
  return pose;
}

ArmServoSetpoints MechArmIK::servoSetpoints(const ArmServoMap& map) const {
  ArmServoPose pose = servoPose();
  ArmServoSetpoints setpoints;
  setpoints.baseUs = map.base.pulseFromDegrees(pose.baseDeg);
  setpoints.shoulderUs = map.shoulder.pulseFromDegrees(pose.shoulderDeg);
  setpoints.elbowUs = map.elbow.pulseFromDegrees(pose.elbowDeg);
  setpoints.wristPitchUs = map.wristPitch.pulseFromDegrees(pose.wristPitchDeg);
  setpoints.wristYawUs = map.wristYaw.pulseFromDegrees(pose.wristYawDeg);
  setpoints.wristRollUs = map.wristRoll.pulseFromDegrees(pose.wristRollDeg);
  setpoints.gripperUs = map.gripper.pulseFromDegrees(pose.gripperPitchDeg);
  setpoints.extensionUs = map.extension.pulseFromLength(pose.extensionMm);
  setpoints.gripperOpen = pose.gripperOpen;
  return setpoints;
}

void MechArmIK::updateBasePosition() {
  basePos_ = Vec3(0.f, dims_.baseHeight, 0.f);
}

void MechArmIK::updateGripperFrame(const Vec3& radialDir, float link2Angle) {
  Vec3 forward = joints_[4] - joints_[3];
  if (forward.magSq() < 1e-5f) {
    forward = joints_[4] - joints_[2];
  }
  if (forward.magSq() < 1e-5f) {
    float forwardRadial = cosf(link2Angle);
    float forwardVertical = sinf(link2Angle);
    forward = Vec3(radialDir.x * forwardRadial, forwardVertical, radialDir.z * forwardRadial);
  }
  forward.normalize();

  Vec3 worldUp(0.f, 1.f, 0.f);
  Vec3 right = worldUp.cross(forward);
  if (right.magSq() < 1e-5f) {
    right = radialDir.cross(forward);
  }
  if (right.magSq() < 1e-5f) {
    right = Vec3(1.f, 0.f, 0.f);
  } else {
    right.normalize();
  }
  Vec3 up = forward.cross(right);
  if (up.magSq() < 1e-5f) {
    up = Vec3(0.f, 1.f, 0.f);
  } else {
    up.normalize();
  }

  gripperForward_ = forward;
  gripperRight_ = right;
  gripperUp_ = up;

  applyGripperServoRotations();
}

void MechArmIK::applyGripperServoRotations() {
  Vec3 newRight = rotateAroundAxis(gripperRight_, gripperUp_, gripperYaw_);
  Vec3 newForward = rotateAroundAxis(gripperForward_, gripperUp_, gripperYaw_);
  gripperRight_ = newRight;
  gripperForward_ = newForward;

  Vec3 newUp = rotateAroundAxis(gripperUp_, gripperRight_, gripperPitch_);
  newForward = rotateAroundAxis(gripperForward_, gripperRight_, gripperPitch_);
  gripperUp_ = newUp;
  gripperForward_ = newForward;

  newRight = rotateAroundAxis(gripperRight_, gripperForward_, gripperRoll_);
  newUp = rotateAroundAxis(gripperUp_, gripperForward_, gripperRoll_);
  gripperRight_ = newRight;
  gripperUp_ = newUp;

  ensureGripperOrthonormal();
}

void MechArmIK::ensureGripperOrthonormal() {
  gripperForward_.normalize();
  gripperRight_.normalize();

  Vec3 up = gripperForward_.cross(gripperRight_);
  if (up.magSq() < kEpsilon) {
    up = gripperForward_.cross(Vec3(0.f, 1.f, 0.f));
  }
  if (up.magSq() < kEpsilon) {
    up = Vec3(0.f, 1.f, 0.f);
  }
  up.normalize();
  gripperUp_ = up;

  Vec3 right = gripperUp_.cross(gripperForward_);
  if (right.magSq() < kEpsilon) {
    right = Vec3(1.f, 0.f, 0.f);
  }
  right.normalize();
  gripperRight_ = right;
}

} // namespace mech

