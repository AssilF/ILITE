#include "telemetry.h"
#include <string.h>
#include <math.h>

ThrustCommand emission{PACKET_MAGIC, 1000, 0, 0, 0, false};
receptionDataPacket reception{};
TelemetryPacket telemetry{};
BulkyCommand bulkyCommand{0, 0, 0, {0, 0, 0}};
uint8_t droneStabilizationMask = 0;
bool droneStabilizationGlobal = false;
int16_t pidCorrectionHistory[PID_AXIS_COUNT][screen_Width];
int16_t pidActualHistory[PID_AXIS_COUNT][screen_Width];
int16_t pidTargetHistory[PID_AXIS_COUNT][screen_Width];
int16_t pidErrorHistory[PID_AXIS_COUNT][screen_Width];

static inline int16_t clampToInt16(float value) {
  if (value > 32767.0f) return 32767;
  if (value < -32768.0f) return -32768;
  return static_cast<int16_t>(value);
}

void appendPidSample() {
  constexpr float kAngleScale = 100.0f;

  for (int axis = 0; axis < PID_AXIS_COUNT; ++axis) {
    memmove(pidCorrectionHistory[axis], pidCorrectionHistory[axis] + 1, (screen_Width - 1) * sizeof(int16_t));
    memmove(pidActualHistory[axis],     pidActualHistory[axis]     + 1, (screen_Width - 1) * sizeof(int16_t));
    memmove(pidTargetHistory[axis],     pidTargetHistory[axis]     + 1, (screen_Width - 1) * sizeof(int16_t));
    memmove(pidErrorHistory[axis],      pidErrorHistory[axis]      + 1, (screen_Width - 1) * sizeof(int16_t));
  }

  const float actual[PID_AXIS_COUNT] = { telemetry.pitch, telemetry.roll, telemetry.yaw };
  const float target[PID_AXIS_COUNT] = { static_cast<float>(telemetry.pitchAngle),
                                         static_cast<float>(telemetry.rollAngle),
                                         static_cast<float>(telemetry.yawAngle) };
  const float correction[PID_AXIS_COUNT] = { telemetry.pitchCorrection, telemetry.rollCorrection, telemetry.yawCorrection };

  for (int axis = 0; axis < PID_AXIS_COUNT; ++axis) {
    int16_t actualSample = clampToInt16(roundf(actual[axis] * kAngleScale));
    int16_t targetSample = clampToInt16(roundf(target[axis] * kAngleScale));
    int16_t errorSample = clampToInt16(static_cast<float>(targetSample - actualSample));
    int16_t correctionSample = clampToInt16(roundf(correction[axis]));

    pidActualHistory[axis][screen_Width - 1] = actualSample;
    pidTargetHistory[axis][screen_Width - 1] = targetSample;
    pidErrorHistory[axis][screen_Width - 1] = errorSample;
    pidCorrectionHistory[axis][screen_Width - 1] = correctionSample;
  }
}
