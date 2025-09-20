#include "telemetry.h"
#include <string.h>

ThrustCommand emission{PACKET_MAGIC, 1000, 0, 0, 0, false};
receptionDataPacket reception{};
TelemetryPacket telemetry{};
BulkyCommand bulkyCommand{0, 0, 0, {0, 0, 0}};
int16_t pidPitchHistory[screen_Width];
int16_t pidRollHistory[screen_Width];
int16_t pidYawHistory[screen_Width];

void appendPidSample() {
  memmove(pidPitchHistory, pidPitchHistory + 1, (screen_Width - 1) * sizeof(int16_t));
  memmove(pidRollHistory,  pidRollHistory  + 1, (screen_Width - 1) * sizeof(int16_t));
  memmove(pidYawHistory,   pidYawHistory   + 1, (screen_Width - 1) * sizeof(int16_t));
  pidPitchHistory[screen_Width - 1] = static_cast<int16_t>(telemetry.pitchCorrection);
  pidRollHistory[screen_Width - 1]  = static_cast<int16_t>(telemetry.rollCorrection);
  pidYawHistory[screen_Width - 1]   = static_cast<int16_t>(telemetry.yawCorrection);
}
