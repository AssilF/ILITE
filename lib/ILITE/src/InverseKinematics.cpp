#include "InverseKinematics.h"

#include <math.h>
#include <algorithm>

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

bool InverseKinematics::solveInternal(const Vec3& target, const Vec3*, IKSolution& outSolution) const {
    outSolution = IKSolution{};

    if (!isFiniteVec(target)) {
        outSolution.reachable = false;
        outSolution.constrained = true;
        return false;
    }

    const float L1 = config_.dimensions.shoulderLengthMm;
    const float L2Base = config_.dimensions.elbowLengthMm;
const float extMin = std::min(config_.elbowExtension.minMm, config_.elbowExtension.maxMm);
const float extMax = std::max(config_.elbowExtension.minMm, config_.elbowExtension.maxMm);
    const float L2Min = L2Base + extMin;
    const float L2Max = L2Base + extMax;

    const JointLimits baseLimits{
        std::min(config_.baseYaw.minDeg, config_.baseYaw.maxDeg),
        std::max(config_.baseYaw.minDeg, config_.baseYaw.maxDeg)};
    const JointLimits shoulderLimits{
        std::min(config_.shoulder.minDeg, config_.shoulder.maxDeg),
        std::max(config_.shoulder.minDeg, config_.shoulder.maxDeg)};
    const JointLimits elbowLimits{
        std::min(config_.elbow.minDeg, config_.elbow.maxDeg),
        std::max(config_.elbow.minDeg, config_.elbow.maxDeg)};

    bool constrained = false;

    float desiredYaw = toDegrees(atan2f(target.z, target.x));
    float baseYawDeg = clampf(desiredYaw, baseLimits.minDeg, baseLimits.maxDeg);
    if (fabsf(baseYawDeg - desiredYaw) > 1e-3f) {
        constrained = true;
    }
    const float baseYawRad = baseYawDeg * DEG_TO_RAD;
    const float cosYaw = cosf(baseYawRad);
    const float sinYaw = sinf(baseYawRad);

    float planarX = cosYaw * target.x + sinYaw * target.z;
    float lateral = -sinYaw * target.x + cosYaw * target.z;
    if (planarX < 0.0f) {
        planarX = 0.0f;
        constrained = true;
    }
    if (fabsf(lateral) > 5.0f) {
        constrained = true;
    }

    const float vertical = target.y;
    float distance = sqrtf(planarX * planarX + vertical * vertical);

    float desiredL2 = (distance > config_.geometryEpsilonMm) ? distance - L1 : 0.0f;
    float L2 = clampf(L2Base + clampf(desiredL2, extMin, extMax), L2Min, L2Max);
    float extensionMm = clampf(L2 - L2Base, extMin, extMax);

    const float reachMin = fabsf(L1 - L2);
    const float reachMax = L1 + L2;
    float distanceEval = clampf(distance, reachMin, reachMax);
    if (fabsf(distanceEval - distance) > 1e-3f) {
        constrained = true;
    }

    const float angleToTarget = atan2f(vertical, planarX);
    float cosShoulder = (L1 * L1 + distanceEval * distanceEval - L2 * L2) /
                        (2.0f * L1 * distanceEval);
    cosShoulder = clampf(cosShoulder, -1.0f, 1.0f);
    float shoulderRad = angleToTarget + acosf(cosShoulder);
    float shoulderDeg = toDegrees(shoulderRad);

    float cosElbow = (L1 * L1 + L2 * L2 - distanceEval * distanceEval) /
                     (2.0f * L1 * L2);
    cosElbow = clampf(cosElbow, -1.0f, 1.0f);
    float elbowRad = acosf(cosElbow);
    float elbowDeg = toDegrees(elbowRad);

    float shoulderClamped = clampf(shoulderDeg, shoulderLimits.minDeg, shoulderLimits.maxDeg);
    float elbowClamped = clampf(elbowDeg, elbowLimits.minDeg, elbowLimits.maxDeg);
    if (fabsf(shoulderClamped - shoulderDeg) > 1e-3f || fabsf(elbowClamped - elbowDeg) > 1e-3f) {
        constrained = true;
    }

    shoulderRad = shoulderClamped * DEG_TO_RAD;
    elbowRad = elbowClamped * DEG_TO_RAD;
    float elbowAbsRad = shoulderRad - elbowRad;

    float elbowLocalX = L1 * cosf(shoulderRad);
    float elbowLocalY = L1 * sinf(shoulderRad);
    float wristLocalX = elbowLocalX + L2 * cosf(elbowAbsRad);
    float wristLocalY = elbowLocalY + L2 * sinf(elbowAbsRad);

    Vec3 wristWorld{
        cosYaw * wristLocalX,
        wristLocalY,
        sinYaw * wristLocalX
    };

    Vec3 elbowWorld{
        cosYaw * elbowLocalX,
        elbowLocalY,
        sinYaw * elbowLocalX
    };

    Vec3 forearmDir{
        wristWorld.x - elbowWorld.x,
        wristWorld.y - elbowWorld.y,
        wristWorld.z - elbowWorld.z
    };

    outSolution.joints.baseYawDeg = baseYawDeg;
    outSolution.joints.shoulderDeg = shoulderClamped;
    outSolution.joints.elbowDeg = elbowClamped;
    outSolution.joints.elbowExtensionMm = extensionMm;
    outSolution.joints.gripperPitchDeg = 90.0f;
    outSolution.joints.gripperYawDeg = 90.0f;
    outSolution.joints.gripperRollDeg = config_.defaultGripperRollDeg;
    outSolution.forearmLengthMm = L2;
    outSolution.wristDistanceMm = distanceEval;
    outSolution.wristTarget = wristWorld;
    outSolution.toolDirection = normalise(forearmDir, config_.toolDirectionEpsilonMm);
    outSolution.constrained = constrained;
    outSolution.reachable = !constrained;
    return outSolution.reachable;
}

} // namespace IKEngine


