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

/**
 * @brief Play a custom tone with specified frequency and duration
 * @param frequencyHz Tone frequency in Hz
 * @param durationMs Duration in milliseconds
 */
void audioPlayTone(uint16_t frequencyHz, uint16_t durationMs);
