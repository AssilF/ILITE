#pragma once
#include <Arduino.h>

constexpr int screen_Width = 128;
constexpr int screen_Height = 64;

struct emissionDataPacket {
    uint16_t altitude;
    int8_t pitchAngle;
    int8_t rollAngle;
    int8_t yawAngle;
    bool arm_motors;
};

struct receptionDataPacket {
  byte INDEX;
  byte statusByte;
  int dataByte[8];
  byte okIndex;
};

struct telemetryPacket {
  int16_t pitch;
  int16_t roll;
  int16_t yaw;
  int16_t altitude;
  int16_t pidPitch;
  int16_t pidRoll;
  int16_t pidYaw;
  int16_t accelZ;
};

extern emissionDataPacket emission;
extern receptionDataPacket reception;
extern telemetryPacket telemetry;
extern int16_t pidPitchHistory[screen_Width];
extern int16_t pidRollHistory[screen_Width];
extern int16_t pidYawHistory[screen_Width];
