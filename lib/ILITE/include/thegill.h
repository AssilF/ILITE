#pragma once

#include <Arduino.h>

constexpr uint32_t THEGILL_PACKET_MAGIC = 0x54474C4C; // 'TGLL'

// Operating mode for The'gill S3 drivetrain.
enum class GillMode : uint8_t {
  Default = 0,
  Differential = 1,
};

// Available easing curves that shape motor ramp behaviour.
enum class GillEasing : uint8_t {
  None = 0,
  Linear,
  EaseIn,
  EaseOut,
  EaseInOut,
  Sine,
};

constexpr uint8_t kGillEasingCount = static_cast<uint8_t>(GillEasing::Sine) + 1;

constexpr GillEasing kDefaultGillEasing = GillEasing::None;

constexpr uint8_t GILL_FLAG_BRAKE = 0x01;
constexpr uint8_t GILL_FLAG_HONK  = 0x02;

struct ThegillCommand {
  uint32_t magic;
  int16_t leftFront;
  int16_t leftRear;
  int16_t rightFront;
  int16_t rightRear;
  float easingRate;
  GillMode mode;
  GillEasing easing;
  uint8_t flags;
  uint8_t reserved;
} __attribute__((packed));

struct ThegillConfig {
  GillMode mode;
  GillEasing easing;
};

struct ThegillRuntime {
  float targetLeftFront;
  float targetLeftRear;
  float targetRightFront;
  float targetRightRear;
  float actualLeftFront;
  float actualLeftRear;
  float actualRightFront;
  float actualRightRear;
  float easingRate;
  bool brakeActive;
  bool honkActive;
};

struct ThegillTelemetryPacket {
  uint32_t magic;
  float pitch;
  float roll;
  float yaw;
  float pitchCorrection;
  float rollCorrection;
  float yawCorrection;
  uint16_t throttle;
  int8_t pitchCommand;
  int8_t rollCommand;
  int8_t yawCommand;
  float altitude;
  float verticalAcc;
  uint32_t commandAge;
} __attribute__((packed));

extern ThegillCommand thegillCommand;
extern ThegillConfig thegillConfig;
extern ThegillRuntime thegillRuntime;
extern ThegillTelemetryPacket thegillTelemetryPacket;

float applyEasingCurve(GillEasing mode, float t);

