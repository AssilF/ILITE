/**
 * @file AudioRegistry.h
 * @brief Audio Extension System - Custom Sound Registration
 *
 * Allows users to register custom audio cues with friendly names.
 * Sounds are played by name, abstracting hardware details.
 *
 * @example
 * ```cpp
 * REGISTER_AUDIO("motor_armed", 1000, 100);  // 1000Hz, 100ms
 *
 * void onArm() {
 *     AudioRegistry::play("motor_armed");
 * }
 * ```
 *
 * @author ILITE Framework
 * @version 1.0.0
 */

#ifndef ILITE_AUDIO_REGISTRY_H
#define ILITE_AUDIO_REGISTRY_H

#include <Arduino.h>
#include <vector>
#include <functional>

/**
 * @brief Audio cue definition (renamed to avoid conflict with existing AudioCue enum)
 */
struct AudioCueDefinition {
    const char* name;          ///< Unique identifier "motor_armed"
    uint16_t frequencyHz;      ///< Tone frequency in Hz (0 = silence)
    uint16_t durationMs;       ///< Duration in milliseconds

    /// Advanced: Custom playback function (overrides frequency/duration)
    std::function<void()> customPlayback;

    /// Optional: Play only if condition is true
    std::function<bool()> condition;
};

/**
 * @brief Audio Registry - Manages custom sound cues
 */
class AudioRegistry {
public:
    /**
     * @brief Register a custom audio cue
     * @param cue Audio cue definition
     */
    static void registerCue(const AudioCueDefinition& cue);

    /**
     * @brief Play audio cue by name
     * @param name Cue name (e.g., "motor_armed")
     * @return true if cue found and played, false if not found
     */
    static bool play(const char* name);

    /**
     * @brief Check if cue exists
     * @param name Cue name
     * @return true if registered
     */
    static bool hasCue(const char* name);

    /**
     * @brief Get all registered cues
     * @return Vector of cue definitions
     */
    static std::vector<AudioCueDefinition>& getCues();

    /**
     * @brief Get cue by name
     * @param name Cue name
     * @return Pointer to cue, or nullptr if not found
     */
    static const AudioCueDefinition* getCue(const char* name);

    /**
     * @brief Clear all registered cues
     */
    static void clear();

private:
    static std::vector<AudioCueDefinition> cues_;
};

/**
 * @brief Macro for registering simple audio cues
 *
 * Creates a static initializer that registers the cue at startup.
 *
 * @param name Cue name (string literal)
 * @param freq Frequency in Hz
 * @param duration Duration in milliseconds
 *
 * @example
 * ```cpp
 * REGISTER_AUDIO("beep", 1000, 100);
 * REGISTER_AUDIO("long_beep", 800, 500);
 * ```
 */
#define REGISTER_AUDIO(name, freq, duration) \
    namespace { \
        struct AudioCueRegistrar_##name { \
            AudioCueRegistrar_##name() { \
                AudioRegistry::registerCue({ \
                    #name, \
                    freq, \
                    duration, \
                    nullptr, \
                    nullptr \
                }); \
            } \
        }; \
        static AudioCueRegistrar_##name g_audioCueRegistrar_##name; \
    }

/**
 * @brief Macro for registering audio cues with conditions
 *
 * @param name Cue name
 * @param freq Frequency in Hz
 * @param duration Duration in milliseconds
 * @param cond Lambda returning bool (cue plays only if true)
 *
 * @example
 * ```cpp
 * REGISTER_AUDIO_CONDITIONAL("armed_alert", 1000, 200,
 *     []() { return drongazeState.armed; }
 * );
 * ```
 */
#define REGISTER_AUDIO_CONDITIONAL(name, freq, duration, cond) \
    namespace { \
        struct AudioCueRegistrar_##name { \
            AudioCueRegistrar_##name() { \
                AudioRegistry::registerCue({ \
                    #name, \
                    freq, \
                    duration, \
                    nullptr, \
                    cond \
                }); \
            } \
        }; \
        static AudioCueRegistrar_##name g_audioCueRegistrar_##name; \
    }

/**
 * @brief Macro for registering custom audio playback
 *
 * @param name Cue name
 * @param func Lambda or function performing custom playback
 *
 * @example
 * ```cpp
 * REGISTER_AUDIO_CUSTOM("startup", []() {
 *     AudioRegistry::play("beep");
 *     delay(100);
 *     AudioRegistry::play("beep");
 *     delay(100);
 *     AudioRegistry::play("long_beep");
 * });
 * ```
 */
#define REGISTER_AUDIO_CUSTOM(name, func) \
    namespace { \
        struct AudioCueRegistrar_##name { \
            AudioCueRegistrar_##name() { \
                AudioRegistry::registerCue({ \
                    #name, \
                    0, \
                    0, \
                    func, \
                    nullptr \
                }); \
            } \
        }; \
        static AudioCueRegistrar_##name g_audioCueRegistrar_##name; \
    }

#endif // ILITE_AUDIO_REGISTRY_H
