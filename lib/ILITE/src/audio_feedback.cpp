#include "audio_feedback.h"
#include <DacESP32.h>

namespace {
constexpr gpio_num_t kBuzzerPin = GPIO_NUM_26;
struct Tone {
  uint16_t frequency;
  uint16_t durationMs;
};

struct QueuedTone {
  Tone tone;
  uint32_t endMs;
  bool active;
};

DacESP32 buzzer(kBuzzerPin);
QueuedTone currentTone{{0,0},0,false};

Tone cueToTone(AudioCue cue){
  switch(cue){
    case AudioCue::Scroll:        return {520, 80};
    case AudioCue::Select:        return {720, 140};
    case AudioCue::Back:          return {360, 120};
    case AudioCue::PeerRequest:   return {880, 180};
    case AudioCue::PeerAcknowledge:return {620, 120};
    case AudioCue::PeerDiscovered:return {940, 160};
    case AudioCue::ToggleOn:      return {780, 120};
    case AudioCue::ToggleOff:     return {300, 120};
    case AudioCue::Error:         return {220, 200};
    default:                      return {400, 120};
  }
}

void startTone(const Tone& tone){
  buzzer.enable();
  buzzer.outputCW(tone.frequency);
  currentTone = {tone, millis() + tone.durationMs, true};
}

void stopTone(){
  buzzer.outputCW(0);
  buzzer.disable();
  currentTone = {{0,0},0,false};
}

} // namespace

void audioSetup(){
  buzzer.enable();
  buzzer.outputCW(0);
  buzzer.disable();
}

void audioPlayStartup(){
  const Tone startup[] = {{320,120}, {480,120}, {640,160}, {0,60}};
  for(const Tone& tone : startup){
    if(tone.frequency == 0){
      stopTone();
      delay(tone.durationMs);
    }else{
      startTone(tone);
      delay(tone.durationMs);
    }
  }
  stopTone();
}

void audioFeedback(AudioCue cue){
  startTone(cueToTone(cue));
}

void audioUpdate(){
  if(currentTone.active && millis() >= currentTone.endMs){
    stopTone();
  }
}

void audioPlayTone(uint16_t frequencyHz, uint16_t durationMs){
  startTone({frequencyHz, durationMs});
}
