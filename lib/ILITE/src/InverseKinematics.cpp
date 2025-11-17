#include "InverseKinematics.h"

#include <math.h>

namespace IKEngine {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegPerRad = 180.0f / kPi;

inline float toDegrees(float radians) {
    return radians * kDegPerRad;
}

inline float clampf(float value, float minValue, float maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

inline float length(const Vec3& v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline bool isFiniteVec(const Vec3& v) {
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}

} // namespace

InverseKinematics::InverseKinematics(const IKConfiguration& config)
    : config_(config) {}

const IKConfiguration& InverseKinematics::getConfiguration() const {
    return config_;
}

void InverseKinematics::setConfiguration(const IKConfiguration& config) {
    config_ = config;
}

Vec3 InverseKinematics::normalise(const Vec3& value, float epsilon) {
    if (!isFiniteVec(value)) {
        return Vec3{1.0f, 0.0f, 0.0f};
    }
    const float magnitude = length(value);
    if (magnitude <= epsilon) {
        return Vec3{1.0f, 0.0f, 0.0f};
    }
    const float invMag = 1.0f / magnitude;
    return Vec3{value.x * invMag, value.y * invMag, value.z * invMag};
}

bool InverseKinematics::solve(const Vec3& target, IKSolution& outSolution) const {
    return solveInternal(target, nullptr, outSolution);
}

bool InverseKinematics::solve(const Vec3& target, const Vec3& toolDir, IKSolution& outSolution) const {
    return solveInternal(target, &toolDir, outSolution);
}

bool InverseKinematics::solveInternal(const Vec3& target, const Vec3* toolDir, IKSolution& outSolution) const {
    outSolution = IKSolution{};

    if (!isFiniteVec(target)) {
        outSolution.reachable = false;
        outSolution.constrained = true;
        return false;
    }

    Vec3 workingToolDir;
    if (toolDir != nullptr && isFiniteVec(*toolDir)) {
        workingToolDir = normalise(*toolDir, config_.toolDirectionEpsilonMm);
    } else {
        workingToolDir = normalise(target, config_.toolDirectionEpsilonMm);
    }

    const float wristLength = config_.dimensions.wristChainLength();
    const float targetDistance = length(target);

    float offset = wristLength;
    if (targetDistance < config_.geometryEpsilonMm) {
        offset = 0.0f;
    } else if (offset > targetDistance) {
        offset = clampf(targetDistance - config_.geometryEpsilonMm, 0.0f, wristLength);
    }

    outSolution.toolDirection = workingToolDir;

    const Vec3 wristTarget{
        target.x - workingToolDir.x * offset,
        target.y - workingToolDir.y * offset,
        target.z - workingToolDir.z * offset};

    outSolution.wristTarget = wristTarget;

    const float planarRadius = sqrtf(wristTarget.x * wristTarget.x + wristTarget.y * wristTarget.y);
    float baseYawDeg = toDegrees(atan2f(wristTarget.y, wristTarget.x));

    const float L1 = config_.dimensions.shoulderLengthMm;
    float extensionMin = config_.elbowExtension.minMm;
    float extensionMax = config_.elbowExtension.maxMm;
    if (extensionMax < extensionMin) {
        const float tmp = extensionMax;
        extensionMax = extensionMin;
        extensionMin = tmp;
    }

    const float L2Base = config_.dimensions.elbowLengthMm;
    float forearmLength = L2Base + extensionMin;

    float distance = sqrtf(planarRadius * planarRadius + wristTarget.z * wristTarget.z);
    float distanceForAngles = distance;

    bool withinWorkspace = true;
    bool clamped = false;

    if (distance <= config_.geometryEpsilonMm) {
        withinWorkspace = false;
        clamped = true;
        distanceForAngles = config_.geometryEpsilonMm;
    }

    if (distance > (L1 + forearmLength)) {
        float candidateLength = distance - L1;
        candidateLength = clampf(candidateLength, forearmLength, L2Base + extensionMax);
        if (candidateLength > (L2Base + extensionMax) - config_.geometryEpsilonMm) {
            clamped = true;
            withinWorkspace = false;
        }
        forearmLength = candidateLength;
    } else {
        forearmLength = clampf(forearmLength, L2Base + extensionMin, L2Base + extensionMax);
    }

    const float reachMin = fabsf(L1 - forearmLength);
    const float reachMax = L1 + forearmLength;

    if (distance > reachMax) {
        distanceForAngles = reachMax - config_.geometryEpsilonMm;
        clamped = true;
        withinWorkspace = false;
    } else if (distance < reachMin) {
        distanceForAngles = reachMin + config_.geometryEpsilonMm;
        clamped = true;
        withinWorkspace = false;
    }

    const float angleToTargetRad = atan2f(wristTarget.z, planarRadius);

    float cosShoulderTerm = (L1 * L1 + distanceForAngles * distanceForAngles - forearmLength * forearmLength) /
                            (2.0f * L1 * distanceForAngles);
    cosShoulderTerm = clampf(cosShoulderTerm, -1.0f, 1.0f);

    float shoulderRad = angleToTargetRad + acosf(cosShoulderTerm);
    float shoulderDeg = toDegrees(shoulderRad);

    float cosElbowTerm = (L1 * L1 + forearmLength * forearmLength - distanceForAngles * distanceForAngles) /
                         (2.0f * L1 * forearmLength);
    cosElbowTerm = clampf(cosElbowTerm, -1.0f, 1.0f);

    float elbowRad = acosf(cosElbowTerm);
    float elbowDeg = toDegrees(elbowRad) - 90.0f;

    float horizontalTool = sqrtf(workingToolDir.x * workingToolDir.x + workingToolDir.y * workingToolDir.y);
    float gripperPitch = 90.0f + toDegrees(atan2f(workingToolDir.z, horizontalTool));
    float gripperYaw = 90.0f + toDegrees(atan2f(workingToolDir.y, workingToolDir.x));
    float gripperRoll = config_.defaultGripperRollDeg;

    const JointLimits baseLimits{
        min(config_.baseYaw.minDeg, config_.baseYaw.maxDeg),
        max(config_.baseYaw.minDeg, config_.baseYaw.maxDeg)};
    const JointLimits shoulderLimits{
        min(config_.shoulder.minDeg, config_.shoulder.maxDeg),
        max(config_.shoulder.minDeg, config_.shoulder.maxDeg)};
    const JointLimits elbowLimits{
        min(config_.elbow.minDeg, config_.elbow.maxDeg),
        max(config_.elbow.minDeg, config_.elbow.maxDeg)};
    const JointLimits yawLimits{
        min(config_.gripperYaw.minDeg, config_.gripperYaw.maxDeg),
        max(config_.gripperYaw.minDeg, config_.gripperYaw.maxDeg)};
    const JointLimits pitchLimits{
        min(config_.gripperPitch.minDeg, config_.gripperPitch.maxDeg),
        max(config_.gripperPitch.minDeg, config_.gripperPitch.maxDeg)};
    const JointLimits rollLimits{
        min(config_.gripperRoll.minDeg, config_.gripperRoll.maxDeg),
        max(config_.gripperRoll.minDeg, config_.gripperRoll.maxDeg)};

    const float baseYawClamped = clampf(baseYawDeg, baseLimits.minDeg, baseLimits.maxDeg);
    const float shoulderClamped = clampf(shoulderDeg, shoulderLimits.minDeg, shoulderLimits.maxDeg);
    const float elbowClamped = clampf(elbowDeg, elbowLimits.minDeg, elbowLimits.maxDeg);
    const float pitchClamped = clampf(gripperPitch, pitchLimits.minDeg, pitchLimits.maxDeg);
    const float yawClamped = clampf(gripperYaw, yawLimits.minDeg, yawLimits.maxDeg);
    const float rollClamped = clampf(gripperRoll, rollLimits.minDeg, rollLimits.maxDeg);

    if (baseYawClamped != baseYawDeg || shoulderClamped != shoulderDeg ||
        elbowClamped != elbowDeg || pitchClamped != gripperPitch ||
        yawClamped != gripperYaw || rollClamped != gripperRoll) {
        clamped = true;
    }

    float extension = clampf(forearmLength - L2Base, extensionMin, extensionMax);
    const float extensionAppliedLength = L2Base + extension;
    if (fabsf(extensionAppliedLength - forearmLength) > config_.geometryEpsilonMm) {
        clamped = true;
        forearmLength = extensionAppliedLength;
    }

    outSolution.joints.baseYawDeg = baseYawClamped;
    outSolution.joints.shoulderDeg = shoulderClamped;
    outSolution.joints.elbowDeg = elbowClamped;
    outSolution.joints.elbowExtensionMm = extension;
    outSolution.joints.gripperPitchDeg = pitchClamped;
    outSolution.joints.gripperYawDeg = yawClamped;
    outSolution.joints.gripperRollDeg = rollClamped;

    outSolution.forearmLengthMm = forearmLength;
    outSolution.wristDistanceMm = distanceForAngles;
    outSolution.constrained = clamped;
    outSolution.reachable = withinWorkspace && !clamped;

    return outSolution.reachable;
}

} // namespace IKEngine

