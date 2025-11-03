/**
 * @file ILITEHelpers.cpp
 * @brief Helper Classes Implementation
 */

#include "ILITEHelpers.h"
#include "connection_log.h"
#include "audio_feedback.h"
#include <cstdarg>
#include <cstdio>

// ============================================================================
// Logger Implementation
// ============================================================================

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::log(const char* message) {
    connectionLogAdd(message);
}

void Logger::logf(const char* fmt, ...) {
    char buffer[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    connectionLogAddf("%s", buffer);
}

void Logger::info(const char* message) {
    log(message);
}

void Logger::warning(const char* message) {
    char buffer[140];
    snprintf(buffer, sizeof(buffer), "WARN: %s", message);
    log(buffer);
}

void Logger::error(const char* message) {
    char buffer[140];
    snprintf(buffer, sizeof(buffer), "ERROR: %s", message);
    log(buffer);
}

size_t Logger::getCount() const {
    return connectionLogGetCount();
}

const char* Logger::getEntry(size_t index) const {
    return connectionLogGetEntry(index);
}

void Logger::clear() {
    connectionLogClear();
}

void Logger::setEnabled(bool enabled) {
    connectionLogSetRecordingEnabled(enabled);
}

bool Logger::isEnabled() const {
    return connectionLogIsRecordingEnabled();
}

// ============================================================================
// Audio Implementation
// ============================================================================

Audio& Audio::getInstance() {
    static Audio instance;
    return instance;
}

void Audio::playScroll() {
    if (enabled_) {
        audioFeedback(AudioCue::Scroll);
    }
}

void Audio::playSelect() {
    if (enabled_) {
        audioFeedback(AudioCue::Select);
    }
}

void Audio::playBack() {
    if (enabled_) {
        audioFeedback(AudioCue::Back);
    }
}

void Audio::playToggle(bool on) {
    if (enabled_) {
        audioFeedback(on ? AudioCue::ToggleOn : AudioCue::ToggleOff);
    }
}

void Audio::playError() {
    if (enabled_) {
        audioFeedback(AudioCue::Error);
    }
}

void Audio::playDiscovered() {
    if (enabled_) {
        audioFeedback(AudioCue::PeerDiscovered);
    }
}

void Audio::playPairRequest() {
    if (enabled_) {
        audioFeedback(AudioCue::PeerRequest);
    }
}

void Audio::playPairAck() {
    if (enabled_) {
        audioFeedback(AudioCue::PeerAcknowledge);
    }
}

void Audio::playTone(uint16_t frequencyHz, uint16_t durationMs) {
    if (enabled_) {
        // Custom tone implementation would go here
        // For now, use nearest predefined cue
        audioFeedback(AudioCue::Select);
    }
}

void Audio::setEnabled(bool enabled) {
    enabled_ = enabled;
}

bool Audio::isEnabled() const {
    return enabled_;
}

void Audio::update() {
    audioUpdate();
}

// ============================================================================
// PreferencesManager Implementation
// ============================================================================

PreferencesManager& PreferencesManager::getInstance() {
    static PreferencesManager instance;
    return instance;
}

bool PreferencesManager::begin(const char* name, bool readOnly) {
    return prefs_.begin(name, readOnly);
}

void PreferencesManager::end() {
    prefs_.end();
}

bool PreferencesManager::putInt(const char* key, int32_t value) {
    return prefs_.putInt(key, value) > 0;
}

bool PreferencesManager::putUInt(const char* key, uint32_t value) {
    return prefs_.putUInt(key, value) > 0;
}

bool PreferencesManager::putFloat(const char* key, float value) {
    return prefs_.putFloat(key, value) > 0;
}

bool PreferencesManager::putBool(const char* key, bool value) {
    return prefs_.putBool(key, value) > 0;
}

bool PreferencesManager::putString(const char* key, const char* value) {
    return prefs_.putString(key, value) > 0;
}

bool PreferencesManager::putBytes(const char* key, const void* data, size_t length) {
    return prefs_.putBytes(key, data, length) > 0;
}

int32_t PreferencesManager::getInt(const char* key, int32_t defaultValue) {
    return prefs_.getInt(key, defaultValue);
}

uint32_t PreferencesManager::getUInt(const char* key, uint32_t defaultValue) {
    return prefs_.getUInt(key, defaultValue);
}

float PreferencesManager::getFloat(const char* key, float defaultValue) {
    return prefs_.getFloat(key, defaultValue);
}

bool PreferencesManager::getBool(const char* key, bool defaultValue) {
    return prefs_.getBool(key, defaultValue);
}

size_t PreferencesManager::getString(const char* key, char* buffer, size_t bufferSize, const char* defaultValue) {
    String value = prefs_.getString(key, defaultValue);
    size_t len = value.length();
    if (len >= bufferSize) {
        len = bufferSize - 1;
    }
    memcpy(buffer, value.c_str(), len);
    buffer[len] = '\0';
    return len;
}

size_t PreferencesManager::getBytes(const char* key, void* buffer, size_t bufferSize) {
    return prefs_.getBytes(key, buffer, bufferSize);
}

bool PreferencesManager::isKey(const char* key) {
    return prefs_.isKey(key);
}

bool PreferencesManager::remove(const char* key) {
    return prefs_.remove(key);
}

bool PreferencesManager::clear() {
    return prefs_.clear();
}
