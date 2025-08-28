#include "telemetry.h"

emissionDataPacket emission{};
receptionDataPacket reception{};
telemetryPacket telemetry{};
int16_t pidPitchHistory[screen_Width];
int16_t pidRollHistory[screen_Width];
int16_t pidYawHistory[screen_Width];
