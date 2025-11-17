#pragma once

#include <Arduino.h>

/**
 * @file InverseKinematics.h
 * @brief Parametric inverse kinematics solver for Mech'Iane articulated arm.
 *
 * The solver estimates joint commands (base rotation, shoulder, elbow,
 * prismatic extension, and wrist orientation) required to guide the arm's end
 * effector to a target point in 3D space. Every relevant dimension and limit is
 * configurable so the solver can be tuned without code changes once deployed to
 * hardware.
 *
 * Coordinate system assumptions:
 * - +X is forward from the base.
 * - +Y is to the left of the base (right-handed frame).
 * - +Z is upward.
 *
 * Joint conventions used by the solver:
 * - Base rotation: -180 deg (fully clockwise) to 180 deg (fully counter-clockwise).
 * - Shoulder: 0 deg straight forward, 90 deg straight up, 180 deg backwards.
 * - Elbow: 0 deg fully extended, 180 deg fully folded.
 * - Wrist Yaw: 0 deg points right, 90 deg forward, 180 deg left.
 * - Wrist Pitch: 0 deg down, 90 deg forward, 180 deg up.
 * - Wrist Roll: 0 deg right, 90 deg neutral, 180 deg left.
 *
 * These conventions map directly to the user requirements, and can be adapted
 * through configuration offsets if future hardware revisions require it.
 */

namespace IKEngine {

/**
 * Simple 3D vector expressed in millimetres.
 */
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(float xIn, float yIn, float zIn) : x(xIn), y(yIn), z(zIn) {}
};

/**
 * Range helper for rotational joints (degrees).
 */
struct JointLimits {
    float minDeg = 0.0f;
    float maxDeg = 180.0f;
    
    JointLimits() = default;
    JointLimits(float min, float max) : minDeg(min), maxDeg(max) {}
};

/**
 * Range helper for prismatic joints (millimetres).
 */
struct ExtensionLimits {
    float minMm = 0.0f;
    float maxMm = 110.0f;
    
    ExtensionLimits() = default;
    ExtensionLimits(float min, float max) : minMm(min), maxMm(max) {}
};

/**
 * Physical layout of the arm links and wrist offsets (millimetres).
 */
struct ArmDimensions {
    float shoulderLengthMm = 155.0f; ///< Shoulder pivot to elbow base.
    float elbowLengthMm = 170.0f;    ///< Elbow base length without extension.
    float yawToPitchOffsetMm = 0.0f;
    float pitchToRollOffsetMm = 0.0f;
    float rollToEffectorOffsetMm = 0.0f;

    /**
     * @return Total wrist chain length from the yaw joint to the tool tip.
     */
    float wristChainLength() const {
        return yawToPitchOffsetMm + pitchToRollOffsetMm + rollToEffectorOffsetMm;
    }
};

/**
 * Aggregated configuration for the solver.
 */
struct IKConfiguration {
    ArmDimensions dimensions{};
    ExtensionLimits elbowExtension{0.0f, 130.0f};
    JointLimits baseYaw{0.0f, 360.0f};
    JointLimits shoulder{0.0f, 180.0f};
    JointLimits elbow{0.0f, 180.0f};
    JointLimits gripperYaw{0.0f, 180.0f};
    JointLimits gripperPitch{0.0f, 180.0f};
    JointLimits gripperRoll{0.0f, 180.0f};
    float defaultGripperRollDeg = 90.0f; ///< Neutral wrist roll when not specified.
    float toolDirectionEpsilonMm = 1.0f; ///< Minimum magnitude before a direction is considered valid.
    float geometryEpsilonMm = 1e-3f;     ///< Keeps calculations numerically stable.
};

/**
 * Output joint state expressed in degrees / millimetres.
 */
struct JointAngles {
    float baseYawDeg = 0.0f;
    float shoulderDeg = 0.0f;
    float elbowDeg = 0.0f;
    float elbowExtensionMm = 0.0f;
    float gripperYawDeg = 90.0f;
    float gripperPitchDeg = 90.0f;
    float gripperRollDeg = 90.0f;
};

/**
 * Detailed result returned by the solver.
 */
struct IKSolution {
    JointAngles joints{};
    Vec3 wristTarget{};     ///< Wrist centre the 2-link solver aimed for.
    Vec3 toolDirection{};   ///< Normalised tool direction used for wrist orientation.
    float forearmLengthMm = 0.0f; ///< Effective forearm length after extension.
    float wristDistanceMm = 0.0f; ///< Distance from shoulder to wrist centre used in calculations.
    bool reachable = true;        ///< True if the requested target was within physical reach.
    bool constrained = false;     ///< True if any joint had to be clamped to limits.
};

/**
 * Inverse kinematics solver for the ILITE arm.
 */
class InverseKinematics {
public:
    explicit InverseKinematics(const IKConfiguration& config = IKConfiguration());

    const IKConfiguration& getConfiguration() const;
    void setConfiguration(const IKConfiguration& config);

    /**
     * Solve for a target position using an auto-generated tool direction that
     * points from the base toward the goal.
     */
    bool solve(const Vec3& target, IKSolution& outSolution) const;

    /**
     * Solve using an explicit tool direction hint.
     *
     * @param target     Desired end-effector position in millimetres.
     * @param toolDir    Preferred tool direction (will be normalised).
     * @param outSolution Populated joint solution.
     * @return true if the target was reachable without clamping, false otherwise.
     */
    bool solve(const Vec3& target, const Vec3& toolDir, IKSolution& outSolution) const;

    /**
     * Normalises a vector. Returns {1,0,0} when magnitude falls below epsilon.
     */
    static Vec3 normalise(const Vec3& value, float epsilon = 1e-3f);

private:
    IKConfiguration config_;

    bool solveInternal(const Vec3& target, const Vec3* toolDir, IKSolution& outSolution) const;
};

} // namespace IKEngine
