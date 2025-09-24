#pragma once
#include <Arduino.h>

enum class AudioCue : uint8_t {
  Scroll,
  Select,
  Back,
  PeerRequest,
  PeerAcknowledge,
  PeerDiscovered,
  ToggleOn,
  ToggleOff,
  Error
};

void audioFeedback(AudioCue cue);
void audioUpdate();
void audioSetup();
void audioPlayStartup();
