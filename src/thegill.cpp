#include "thegill.h"
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

  float eased;
  switch(mode){
    case GillEasing::None:
    case GillEasing::Linear:
      eased = t;
      break;
    case GillEasing::EaseIn:
      eased = t * t;
      break;
    case GillEasing::EaseOut:
      eased = 1.f - powf(1.f - t, 2.f);
      break;
    case GillEasing::EaseInOut:
      if(t < 0.5f){
        eased = 2.f * t * t;
      }else{
        eased = 1.f - powf(-2.f * t + 2.f, 2.f) / 2.f;
      }
      break;
    case GillEasing::Sine:
    default:
      eased = sinf((t * PI) / 2.f);
      break;
  }

  if(eased > t){
    const float overshoot = eased - t;
    return t + overshoot * t;
  }
  return eased;
}

