#pragma once
#include <Arduino.h>

constexpr int screen_Width = 128;
constexpr int screen_Height = 64;

constexpr uint32_t PACKET_MAGIC = 0xA1B2C3D4;

// Command packet sent from ILITE to the drone.
struct ThrustCommand {
    uint32_t magic;       // Must be PACKET_MAGIC
    uint16_t throttle;    // Base throttle command
    int8_t pitchAngle;    // Desired pitch angle in degrees
    int8_t rollAngle;     // Desired roll angle in degrees
    int8_t yawAngle;      // Desired yaw heading in degrees
    bool arm_motors;      // Arm/disarm motors
} __attribute__((packed));

struct receptionDataPacket {
  byte INDEX;
  byte statusByte;
  int dataByte[8];
  byte okIndex;
};

// Telemetry packet sent from the drone to ILITE.
struct TelemetryPacket {
  uint32_t magic;                // Should be PACKET_MAGIC
  float pitch, roll, yaw;        // Orientation in degrees
  float pitchCorrection, rollCorrection, yawCorrection; // PID outputs
  uint16_t throttle;             // Current throttle command
  int8_t pitchAngle, rollAngle, yawAngle; // Commanded angles
  float altitude;                // Estimated altitude
  float verticalAcc;             // Vertical acceleration in m/s^2
  uint32_t commandAge;           // Age of last command in ms
} __attribute__((packed));

struct BulkyCommand {
  uint8_t replyIndex;
  int8_t speed;
  uint8_t motionState;
  uint8_t buttonStates[3];
} __attribute__((packed));

extern ThrustCommand emission;
extern receptionDataPacket reception;
extern TelemetryPacket telemetry;
extern BulkyCommand bulkyCommand;
extern int16_t pidPitchHistory[screen_Width];
extern int16_t pidRollHistory[screen_Width];
extern int16_t pidYawHistory[screen_Width];
void appendPidSample();
