#include "telemetry.h"
#include <string.h>

emissionDataPacket emission{};
receptionDataPacket reception{};
telemetryPacket telemetry{};
int16_t pidPitchHistory[screen_Width];
int16_t pidRollHistory[screen_Width];
int16_t pidYawHistory[screen_Width];

void appendPidSample() {
  memmove(pidPitchHistory, pidPitchHistory + 1, (screen_Width - 1) * sizeof(int16_t));
  memmove(pidRollHistory,  pidRollHistory  + 1, (screen_Width - 1) * sizeof(int16_t));
  memmove(pidYawHistory,   pidYawHistory   + 1, (screen_Width - 1) * sizeof(int16_t));
  pidPitchHistory[screen_Width - 1] = telemetry.pidPitch;
  pidRollHistory[screen_Width - 1]  = telemetry.pidRoll;
  pidYawHistory[screen_Width - 1]   = telemetry.pidYaw;
}
