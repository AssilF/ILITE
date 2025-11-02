/**
 * @file ILITEHelpers.h
 * @brief ILITE Framework - Helper Classes
 *
 * Provides thin wrappers around existing ILITE subsystems:
 * - Logger: Connection event logging
 * - Audio: Feedback sounds
 * - Preferences: EEPROM storage
 *
 * @author ILITE Team
 * @date 2025
 */

#pragma once
#include <Arduino.h>
#include <Preferences.h>

// ============================================================================
// Logger - Connection Event Logging
// ============================================================================

/**
 * @brief Connection event logger
 *
 * Wraps connection_log system with clean API. Logs ESP-NOW events,
 * pairing status, packet counts, errors, etc.
 *
 * ## Usage
 * ```cpp
 * Logger& log = getLogger();
 * log.log("Device paired");
 * log.logf("Received %d bytes", length);
 * log.error("Timeout occurred");
 * ```
 */
class Logger {
public:
    /**
     * @brief Get singleton instance
     */
    static Logger& getInstance();

    /**
     * @brief Log a message
     * @param message Message string
     */
    void log(const char* message);

    /**
     * @brief Log formatted message (printf-style)
     * @param fmt Format string
     * @param ... Arguments
     */
    void logf(const char* fmt, ...);

    /**
     * @brief Log info message (same as log, for semantic clarity)
     * @param message Message string
     */
    void info(const char* message);

    /**
     * @brief Log warning message
     * @param message Warning string
     */
    void warning(const char* message);

    /**
     * @brief Log error message
     * @param message Error string
     */
    void error(const char* message);

    /**
     * @brief Get number of log entries
     * @return Entry count
     */
    size_t getCount() const;

    /**
     * @brief Get log entry by index (0 = oldest)
     * @param index Entry index
     * @return Log message or nullptr if invalid index
     */
    const char* getEntry(size_t index) const;

    /**
     * @brief Clear all log entries
     */
    void clear();

    /**
     * @brief Enable/disable logging
     *
     * When disabled, log() calls are ignored. Useful for performance.
     *
     * @param enabled true to enable logging
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check if logging is enabled
     * @return true if enabled
     */
    bool isEnabled() const;

private:
    Logger() = default;
};

// ============================================================================
// Audio - Feedback Sounds
// ============================================================================

/**
 * @brief Audio feedback system
 *
 * Plays tactile buzzer sounds for user actions. Wraps audio_feedback system.
 *
 * ## Usage
 * ```cpp
 * Audio& audio = getAudio();
 * audio.playSelect();           // Menu selection
 * audio.playToggle(true);       // Toggle on
 * audio.playError();            // Error occurred
 * ```
 */
class Audio {
public:
    /**
     * @brief Get singleton instance
     */
    static Audio& getInstance();

    /**
     * @brief Play scroll/navigation sound (short beep)
     */
    void playScroll();

    /**
     * @brief Play selection sound (affirming beep)
     */
    void playSelect();

    /**
     * @brief Play back/cancel sound (descending tone)
     */
    void playBack();

    /**
     * @brief Play toggle sound (on/off specific)
     * @param on true for "on" sound, false for "off" sound
     */
    void playToggle(bool on);

    /**
     * @brief Play error sound (low warning tone)
     */
    void playError();

    /**
     * @brief Play device discovered sound (ascending chirp)
     */
    void playDiscovered();

    /**
     * @brief Play pairing request sound
     */
    void playPairRequest();

    /**
     * @brief Play pairing acknowledged sound
     */
    void playPairAck();

    /**
     * @brief Play custom tone
     * @param frequencyHz Frequency in Hz
     * @param durationMs Duration in milliseconds
     */
    void playTone(uint16_t frequencyHz, uint16_t durationMs);

    /**
     * @brief Enable/disable audio
     * @param enabled true to enable audio
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check if audio is enabled
     * @return true if enabled
     */
    bool isEnabled() const;

    /**
     * @brief Update audio system (called by framework each loop)
     */
    void update();

private:
    Audio() : enabled_(true) {}
    bool enabled_;
};

// ============================================================================
// PreferencesManager - EEPROM Storage
// ============================================================================

/**
 * @brief Persistent storage manager (EEPROM/NVS)
 *
 * Wraps ESP32 Preferences library for module-specific settings.
 * Each module gets its own namespace to avoid conflicts.
 *
 * ## Usage
 * ```cpp
 * PreferencesManager& prefs = getPreferences();
 *
 * // Save settings
 * prefs.begin("myrobot");  // Open namespace
 * prefs.putFloat("kp", 1.5f);
 * prefs.putInt("mode", 2);
 * prefs.end();
 *
 * // Load settings
 * prefs.begin("myrobot", true);  // true = read-only
 * float kp = prefs.getFloat("kp", 1.0f);  // default = 1.0
 * int mode = prefs.getInt("mode", 0);
 * prefs.end();
 * ```
 *
 * **Important**: Always call begin() before use and end() when done!
 */
class PreferencesManager {
public:
    /**
     * @brief Get singleton instance
     */
    static PreferencesManager& getInstance();

    /**
     * @brief Open preferences namespace
     *
     * @param name Namespace name (use module ID for uniqueness)
     * @param readOnly true for read-only access, false for read-write
     * @return true if successful
     */
    bool begin(const char* name, bool readOnly = false);

    /**
     * @brief Close preferences namespace
     *
     * Must be called after begin() to free resources.
     */
    void end();

    // ========================================================================
    // Write Methods (require begin() with readOnly=false)
    // ========================================================================

    /**
     * @brief Store int value
     * @param key Key name (max 15 chars)
     * @param value Value to store
     * @return true if successful
     */
    bool putInt(const char* key, int32_t value);

    /**
     * @brief Store unsigned int value
     * @param key Key name
     * @param value Value to store
     * @return true if successful
     */
    bool putUInt(const char* key, uint32_t value);

    /**
     * @brief Store float value
     * @param key Key name
     * @param value Value to store
     * @return true if successful
     */
    bool putFloat(const char* key, float value);

    /**
     * @brief Store bool value
     * @param key Key name
     * @param value Value to store
     * @return true if successful
     */
    bool putBool(const char* key, bool value);

    /**
     * @brief Store string value
     * @param key Key name
     * @param value String to store (max 1984 chars)
     * @return true if successful
     */
    bool putString(const char* key, const char* value);

    /**
     * @brief Store byte array
     * @param key Key name
     * @param data Data pointer
     * @param length Data length
     * @return true if successful
     */
    bool putBytes(const char* key, const void* data, size_t length);

    // ========================================================================
    // Read Methods
    // ========================================================================

    /**
     * @brief Get int value
     * @param key Key name
     * @param defaultValue Default if key doesn't exist
     * @return Stored value or default
     */
    int32_t getInt(const char* key, int32_t defaultValue = 0);

    /**
     * @brief Get unsigned int value
     * @param key Key name
     * @param defaultValue Default if key doesn't exist
     * @return Stored value or default
     */
    uint32_t getUInt(const char* key, uint32_t defaultValue = 0);

    /**
     * @brief Get float value
     * @param key Key name
     * @param defaultValue Default if key doesn't exist
     * @return Stored value or default
     */
    float getFloat(const char* key, float defaultValue = 0.0f);

    /**
     * @brief Get bool value
     * @param key Key name
     * @param defaultValue Default if key doesn't exist
     * @return Stored value or default
     */
    bool getBool(const char* key, bool defaultValue = false);

    /**
     * @brief Get string value
     * @param key Key name
     * @param buffer Output buffer
     * @param bufferSize Buffer size
     * @param defaultValue Default if key doesn't exist
     * @return Number of characters read (0 if not found)
     */
    size_t getString(const char* key, char* buffer, size_t bufferSize, const char* defaultValue = "");

    /**
     * @brief Get byte array
     * @param key Key name
     * @param buffer Output buffer
     * @param bufferSize Buffer size
     * @return Number of bytes read (0 if not found)
     */
    size_t getBytes(const char* key, void* buffer, size_t bufferSize);

    // ========================================================================
    // Utility Methods
    // ========================================================================

    /**
     * @brief Check if key exists
     * @param key Key name
     * @return true if key exists in current namespace
     */
    bool isKey(const char* key);

    /**
     * @brief Remove key
     * @param key Key name
     * @return true if successful
     */
    bool remove(const char* key);

    /**
     * @brief Clear all keys in current namespace
     * @return true if successful
     */
    bool clear();

private:
    PreferencesManager() = default;
    Preferences prefs_;  ///< Underlying ESP32 Preferences instance
};
