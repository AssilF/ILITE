#include "thegill.h"
#include "ILITEModule.h"
#include "ModuleRegistry.h"
#include "InputManager.h"
#include "DisplayCanvas.h"
#include "input.h"
#include <math.h>

ThegillCommand thegillCommand{
  THEGILL_PACKET_MAGIC,
  0, 0, 0, 0,
  4.0f,
  GillMode::Default,
  kDefaultGillEasing,
  0,
  0
};

ThegillConfig thegillConfig{
  GillMode::Default,
  kDefaultGillEasing
};

ThegillRuntime thegillRuntime{
  0.f, 0.f, 0.f, 0.f,
  0.f, 0.f, 0.f, 0.f,
  4.0f,
  false,
  false
};

ThegillTelemetryPacket thegillTelemetryPacket{
  THEGILL_PACKET_MAGIC,
  0.f, 0.f, 0.f,
  0.f, 0.f, 0.f,
  0,
  0, 0, 0,
  0.f,
  0.f,
  0
};


float applyEasingCurve(GillEasing mode, float t){
  if(t <= 0.f) return 0.f;
  if(t >= 1.f) return 1.f;
  switch(mode){
    case GillEasing::None:
      return 1.f;
    case GillEasing::Linear:
      return t;
    case GillEasing::EaseIn:
      return t * t;
    case GillEasing::EaseOut:
      return 1.f - powf(1.f - t, 2.f);
    case GillEasing::EaseInOut:
      if(t < 0.5f){
        return 2.f * t * t;
      }
      return 1.f - powf(-2.f * t + 2.f, 2.f) / 2.f;
    case GillEasing::Sine:
    default:
      return sinf((t * PI) / 2.f);
  }
}

// ============================================================================
// TheGill Control Update (called at 50Hz)
// ============================================================================

void updateThegillControl() {
    if (thegillConfig.mode == GillMode::Default) {
        // Tank drive mode: Left joystick Y = left motors, Right joystick Y = right motors
        int16_t leftY = analogRead(joystickA_Y);
        int16_t rightY = analogRead(joystickB_Y);

        // Map to motor range (assuming 4095 = max forward, 0 = max reverse, 2048 = center)
        int16_t leftMotor = map(leftY, 0, 4095, -32767, 32767);
        int16_t rightMotor = map(rightY, 0, 4095, -32767, 32767);

        // Apply deadzone (10%)
        if (abs(leftMotor) < 3276) leftMotor = 0;
        if (abs(rightMotor) < 3276) rightMotor = 0;

        // Update command (both front and rear get same value for each side)
        thegillCommand.leftFront = leftMotor;
        thegillCommand.leftRear = leftMotor;
        thegillCommand.rightFront = rightMotor;
        thegillCommand.rightRear = rightMotor;

    } else {
        // Differential mode: One joystick controls both forward/back and turning
        int16_t joyY = analogRead(joystickA_Y);
        int16_t joyX = analogRead(joystickA_X);

        // Map to -100 to +100 range for easier math
        int16_t throttle = map(joyY, 0, 4095, -100, 100);
        int16_t turn = map(joyX, 0, 4095, -100, 100);

        // Apply deadzone
        if (abs(throttle) < 10) throttle = 0;
        if (abs(turn) < 10) turn = 0;

        // Differential drive calculation
        int16_t leftMotor = throttle + turn;
        int16_t rightMotor = throttle - turn;

        // Clamp to range
        leftMotor = constrain(leftMotor, -100, 100);
        rightMotor = constrain(rightMotor, -100, 100);

        // Scale to int16_t range
        thegillCommand.leftFront = leftMotor * 327;
        thegillCommand.leftRear = leftMotor * 327;
        thegillCommand.rightFront = rightMotor * 327;
        thegillCommand.rightRear = rightMotor * 327;
    }

    // Update config in command packet
    thegillCommand.mode = thegillConfig.mode;
    thegillCommand.easing = thegillConfig.easing;
    thegillCommand.easingRate = thegillRuntime.easingRate;

    // Read button states for brake/honk
    thegillCommand.flags = 0;
    if (digitalRead(button1)) {
        thegillCommand.flags |= GILL_FLAG_BRAKE;
        thegillRuntime.brakeActive = true;
    } else {
        thegillRuntime.brakeActive = false;
    }

    if (digitalRead(button2)) {
        thegillCommand.flags |= GILL_FLAG_HONK;
        thegillRuntime.honkActive = true;
    } else {
        thegillRuntime.honkActive = false;
    }
}

// ============================================================================
// TheGill Module Class (ILITE Framework Integration)
// ============================================================================

// TheGill logo (32x32 XBM - horizontal stripes)
static const uint8_t thegill_logo_32x32[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


