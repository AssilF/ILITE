# Mech-iane IK Module

This module translates the Processing Mech'iane inverse kinematics sketch into Arduino-friendly C++. It provides
length/constraint definitions for Thegill's arm, solves for joint angles, and outputs servo-compatible commands.

## Key Types
- `mech::Vec3` – lightweight 3D vector used by the solver.
- `mech::ArmDimensions` – physical link lengths (mm) and base geometry.
- `mech::ArmLimits` – joint constraints, minimum reach, and gripper angle bounds.
- `mech::MechArmIK` – solver instance. Maintains state, gripper orientation, and reach status.
- `mech::ArmServoMap` / `mech::ArmServoSetpoints` – convert solved angles and extension into microsecond pulses for
  standard hobby servos or linear actuators.

## Default Configuration
```cpp
mech::ArmDimensions dims;
// baseHeight = 90, baseRadius = 60, shoulderLength = 140, elbowLength = 110, extensionMax = 110
mech::ArmLimits limits;
// baseYawLimit = 135deg, shoulder: -10..175deg, elbow: 0..175deg, min radial 10mm, planar 30mm
```
Adjust these values to match the real hardware before solving.

## Basic Usage
```cpp
#include "mech_arm_ik.h"
#include <Servo.h>

mech::MechArmIK arm;
mech::ArmServoMap servoMap = {
  .base = {.angleMinDeg = 20, .angleMaxDeg = 160, .pulseMinUs = 600, .pulseMaxUs = 2400},
  .shoulder = {.angleMinDeg = 5, .angleMaxDeg = 175, .pulseMinUs = 500, .pulseMaxUs = 2500},
  .elbow = {.angleMinDeg = 0, .angleMaxDeg = 170, .pulseMinUs = 500, .pulseMaxUs = 2500},
  .wristPitch = {.angleMinDeg = -90, .angleMaxDeg = 90, .pulseMinUs = 600, .pulseMaxUs = 2400},
  .wristYaw = {.angleMinDeg = -90, .angleMaxDeg = 90, .pulseMinUs = 600, .pulseMaxUs = 2400},
  .wristRoll = {.angleMinDeg = -90, .angleMaxDeg = 90, .pulseMinUs = 600, .pulseMaxUs = 2400},
  .gripper = {.angleMinDeg = 0, .angleMaxDeg = 70, .pulseMinUs = 900, .pulseMaxUs = 2100},
  .extension = {.lengthMin = 0, .lengthMax = 110, .pulseMinUs = 1000, .pulseMaxUs = 2000}
};

Servo baseServo, shoulderServo; // ... attach the rest

void loop() {
  mech::Vec3 goal(180.f, 210.f, 90.f); // mm in world frame
  bool elbowUp = false;
  if (arm.solve(goal, elbowUp)) {
    auto pulses = arm.servoSetpoints(servoMap);
    baseServo.writeMicroseconds(pulses.baseUs);
    shoulderServo.writeMicroseconds(pulses.shoulderUs);
    // ... drive the remaining servos & actuator
  }
}
```

Call `arm.updateGripperOpen(dtSeconds)` inside your control loop to chase the gripper open/close target smoothly.
Adjust yaw/pitch/roll offsets with `arm.adjustGripper(...)` or `arm.setGripperAngles(...)` before calling `solve`
if you require wrist orientation.

## Status and Safety
After each solve, inspect `arm.solution().status`:
- `reachLimited`, `minDistanceApplied`, `radialAdjusted` flag when the requested pose was clamped.
- `shoulderLimited`, `elbowLimited`, `baseLimited` indicate joint limit enforcement.
- `distanceError` is the remaining mm error between requested and achieved positions.
- `reachable` becomes `false` whenever the solver had to saturate the pose; use this to trigger fallbacks.

## Integration Tips
1. Keep target coordinates in the same mech frame (X right, Y up, Z forward) as the Processing sketch.
2. Update `ArmDimensions` live if telescoping lengths change; the solver uses the latest values each cycle.
3. Map servo pulses carefully—start with conservative ranges, then tune per joint to avoid mechanical crashes.
4. For smoothing, apply your preferred trajectory generator before feeding positions into the solver.
5. Persist calibration values in NVS or EEPROM so the arm boots with trusted geometry.
