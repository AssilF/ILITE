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

void InverseKinematics::setExtensionEnabled(bool enabled) {
    extensionEnabled_ = enabled;
}

bool InverseKinematics::isExtensionEnabled() const {
    return extensionEnabled_;
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

bool InverseKinematics::solvePlanar(float horizontalMm,
                                    float verticalMm,
                                    float extensionOverrideMm,
                                    IKSolution& outSolution) const {
    outSolution = IKSolution{};

    if (!isfinite(horizontalMm) || !isfinite(verticalMm)) {
        outSolution.reachable = false;
        outSolution.constrained = true;
        return false;
    }

    const float L1 = config_.dimensions.shoulderLengthMm;
    const float L2Base = config_.dimensions.elbowLengthMm;
    const float configuredExtMin = std::min(config_.elbowExtension.minMm, config_.elbowExtension.maxMm);
    const float configuredExtMax = std::max(config_.elbowExtension.minMm, config_.elbowExtension.maxMm);
    const float extMin = extensionEnabled_ ? configuredExtMin : 0.0f;
    const float extMax = extensionEnabled_ ? configuredExtMax : 0.0f;
    const float shoulderMin = std::min(config_.shoulder.minDeg, config_.shoulder.maxDeg);
    const float shoulderMax = std::max(config_.shoulder.minDeg, config_.shoulder.maxDeg);
    const float elbowMin = std::min(config_.elbow.minDeg, config_.elbow.maxDeg);
    const float elbowMax = std::max(config_.elbow.minDeg, config_.elbow.maxDeg);

    bool constrained = false;

    float planarX = horizontalMm;
    if (planarX < 0.0f) {
        planarX = 0.0f;
        constrained = true;
    }
    float vertical = verticalMm;
    if (!isfinite(planarX) || !isfinite(vertical)) {
        outSolution.reachable = false;
        outSolution.constrained = true;
        return false;
    }
    float extensionMm = clampf(extensionOverrideMm, extMin, extMax);
    const float L2 = L2Base + extensionMm;

    float distance = sqrtf(planarX * planarX + vertical * vertical);
    const float reachMin = fabsf(L1 - L2);
    const float reachMax = L1 + L2;
    float distanceEval = clampf(distance, reachMin, reachMax);
    if (fabsf(distanceEval - distance) > 1e-3f) {
        constrained = true;
    }
    distanceEval = fmaxf(distanceEval, config_.geometryEpsilonMm);

    const float angleToTarget = (distanceEval > config_.geometryEpsilonMm)
                                    ? atan2f(vertical, planarX)
                                    : 0.0f;
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

    float shoulderClamped = clampf(shoulderDeg, shoulderMin, shoulderMax);
    float elbowClamped = clampf(elbowDeg, elbowMin, elbowMax);
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

    Vec3 elbowWorld{elbowLocalX, elbowLocalY, 0.0f};
    Vec3 wristWorld{wristLocalX, wristLocalY, 0.0f};
    Vec3 forearmDir{
        wristWorld.x - elbowWorld.x,
        wristWorld.y - elbowWorld.y,
        wristWorld.z - elbowWorld.z
    };

    outSolution.joints.baseYawDeg = 0.0f;
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


