/**
 * @file AudioRegistry.cpp
 * @brief Audio Registry Implementation
 */

#include "AudioRegistry.h"
#include "audio_feedback.h"
#include "ILITEHelpers.h"
#include <cstring>

// Static storage
std::vector<AudioCueDefinition> AudioRegistry::cues_;

// ============================================================================
// Registration
// ============================================================================

void AudioRegistry::registerCue(const AudioCueDefinition& cue) {
    // Check for duplicate names
    for (const auto& existing : cues_) {
        if (strcmp(existing.name, cue.name) == 0) {
            Serial.printf("[AudioRegistry] WARNING: Duplicate cue '%s' (ignoring)\n", cue.name);
            return;
        }
    }

    cues_.push_back(cue);
    Serial.printf("[AudioRegistry] Registered audio cue: %s (%uHz, %ums)\n",
                  cue.name, cue.frequencyHz, cue.durationMs);
}

// ============================================================================
// Playback
// ============================================================================

bool AudioRegistry::play(const char* name) {
    if (name == nullptr) {
        return false;
    }

    // Find cue by name
    for (const auto& cue : cues_) {
        if (strcmp(cue.name, name) == 0) {
            // Check condition if present
            if (cue.condition && !cue.condition()) {
                Logger::getInstance().logf("Audio cue '%s' condition not met (skipped)", name);
                return false;
            }

            // Play custom function if present
            if (cue.customPlayback) {
                cue.customPlayback();
                return true;
            }

            // Play simple tone using cue's frequency and duration
            // TODO: Integrate with audio_feedback.h tone system
            // For now, log the playback (actual tone generation needs hardware integration)
            Logger::getInstance().logf("Playing audio: %s (%uHz, %ums)", name, cue.frequencyHz, cue.durationMs);
            return true;
        }
    }

    Logger::getInstance().logf("Audio cue not found: %s", name);
    return false;
}

// ============================================================================
// Queries
// ============================================================================

bool AudioRegistry::hasCue(const char* name) {
    if (name == nullptr) {
        return false;
    }

    for (const auto& cue : cues_) {
        if (strcmp(cue.name, name) == 0) {
            return true;
        }
    }

    return false;
}

std::vector<AudioCueDefinition>& AudioRegistry::getCues() {
    return cues_;
}

const AudioCueDefinition* AudioRegistry::getCue(const char* name) {
    if (name == nullptr) {
        return nullptr;
    }

    for (const auto& cue : cues_) {
        if (strcmp(cue.name, name) == 0) {
            return &cue;
        }
    }

    return nullptr;
}

void AudioRegistry::clear() {
    cues_.clear();
}
