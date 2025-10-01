#include "thegill.h"
#include <math.h>

ThegillCommand thegillCommand{
  THEGILL_PACKET_MAGIC,
  0, 0, 0, 0,
  4.0f,
  GillMode::Default,
  GillEasing::EaseInOut,
  0,
  0
};

ThegillConfig thegillConfig{
  GillMode::Default,
  GillEasing::EaseInOut
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

