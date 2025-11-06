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

class TheGillModule : public ILITEModule {
public:
    // Module metadata
    const char* getModuleId() const override {
        return "com.ilite.thegill";
    }

    const char* getModuleName() const override {
        return "TheGill";
    }

    const char* getVersion() const override {
        return "1.0.0";
    }

    // Detection keywords for auto-pairing
    const char** getDetectionKeywords() const override {
        static const char* keywords[] = {"gill", "thegill", "tgll"};
        return keywords;
    }

    size_t getDetectionKeywordCount() const override {
        return 3;
    }

    const uint8_t* getLogo32x32() const override {
        return thegill_logo_32x32;
    }

    // Packet descriptors
    size_t getCommandPacketTypeCount() const override {
        return 1;
    }

    PacketDescriptor getCommandPacketDescriptor(size_t index) const override {
        if (index == 0) {
            return {
                "TheGill Command",
                THEGILL_PACKET_MAGIC,
                sizeof(ThegillCommand),
                sizeof(ThegillCommand),
                true,
                nullptr,
                0
            };
        }
        return {};
    }

    size_t getTelemetryPacketTypeCount() const override {
        return 1;
    }

    PacketDescriptor getTelemetryPacketDescriptor(size_t index) const override {
        if (index == 0) {
            return {
                "TheGill Telemetry",
                THEGILL_PACKET_MAGIC,
                sizeof(ThegillTelemetryPacket),
                sizeof(ThegillTelemetryPacket),
                true,
                nullptr,
                0
            };
        }
        return {};
    }

    // Lifecycle methods
    void onInit() override {
        thegillCommand.magic = THEGILL_PACKET_MAGIC;
        thegillCommand.easingRate = 4.0f;
        thegillCommand.mode = GillMode::Default;
        thegillCommand.easing = kDefaultGillEasing;
        Serial.println("[TheGillModule] Initialized");
    }

    void onPair() override {
        Serial.println("[TheGillModule] Paired");
    }

    void onUnpair() override {
        Serial.println("[TheGillModule] Unpaired");
        // Reset command
        thegillCommand.leftFront = 0;
        thegillCommand.leftRear = 0;
        thegillCommand.rightFront = 0;
        thegillCommand.rightRear = 0;
        thegillCommand.flags = 0;
    }

    // Control update - basic implementation
    void updateControl(const InputManager& inputs, float dt) override {
        // This would normally read joysticks and map to motor commands
        // For now, keep existing behavior from old system
    }

    // Packet handling
    size_t prepareCommandPacket(size_t typeIndex, uint8_t* buffer, size_t bufferSize) override {
        if (typeIndex == 0 && bufferSize >= sizeof(ThegillCommand)) {
            memcpy(buffer, &thegillCommand, sizeof(ThegillCommand));
            return sizeof(ThegillCommand);
        }
        return 0;
    }

    void handleTelemetry(size_t typeIndex, const uint8_t* data, size_t length) override {
        if (typeIndex == 0 && length >= sizeof(ThegillTelemetryPacket)) {
            memcpy(&thegillTelemetryPacket, data, sizeof(ThegillTelemetryPacket));
        }
    }

    // Display - basic implementation
    void drawDashboard(DisplayCanvas& canvas) override {
        canvas.setFont(DisplayCanvas::NORMAL);
        canvas.drawTextCentered(10, "TheGill");

        canvas.setFont(DisplayCanvas::SMALL);
        char buffer[32];

        // Show motor values
        snprintf(buffer, sizeof(buffer), "LF:%d LR:%d",
                 thegillCommand.leftFront, thegillCommand.leftRear);
        canvas.drawText(2, 30, buffer);

        snprintf(buffer, sizeof(buffer), "RF:%d RR:%d",
                 thegillCommand.rightFront, thegillCommand.rightRear);
        canvas.drawText(2, 42, buffer);

        // Show telemetry if available
        canvas.setFont(DisplayCanvas::TINY);
        snprintf(buffer, sizeof(buffer), "P:%.1f R:%.1f Y:%.1f",
                 thegillTelemetryPacket.pitch,
                 thegillTelemetryPacket.roll,
                 thegillTelemetryPacket.yaw);
        canvas.drawText(2, 56, buffer);
    }
};

// Note: Module registration is now done in src/ModuleRegistration.cpp
// to avoid linker issues with static initialization in library archives

