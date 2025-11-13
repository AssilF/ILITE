/**
 * @file AudioCues.cpp
 * @brief Organic Audio Cue Definitions for ILITE Framework
 *
 * Each cue has a distinct character:
 * - Navigation sounds: Quick, light chirps
 * - Actions: Confirmative tones with character
 * - Errors: Descending tones
 * - Success: Ascending, pleasant tones
 * - Edit mode: Subtle, functional sounds
 * - Status: Unique patterns for each state
 */

#include "AudioRegistry.h"
#include "AudioCues.h"
#include "audio_feedback.h"

// ============================================================================
// Menu Navigation - Light, Quick Chirps
// ============================================================================

// Menu scroll - subtle upward chirp
REGISTER_AUDIO_CUSTOM(menu_select, []() {
    audioPlayTone(800, 20);
    uint32_t start = millis();
    while (millis() - start < 35) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(1000, 25);
});

// Menu back - downward chirp
REGISTER_AUDIO_CUSTOM(menu_back, []() {
    audioPlayTone(1000, 20);
    uint32_t start = millis();
    while (millis() - start < 35) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(800, 25);
});

// ============================================================================
// Actions - Confirmative, Distinct Tones
// ============================================================================

// General action/pair - pleasant upward sweep
REGISTER_AUDIO_CUSTOM(paired, []() {
    audioPlayTone(600, 40);
    uint32_t start = millis();
    while (millis() - start < 70) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(800, 40);
    start = millis();
    while (millis() - start < 70) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(1200, 60);
});

// Toggle ON - rising tone
REGISTER_AUDIO_CUSTOM(toggle, []() {
    audioPlayTone(700, 30);
    uint32_t start = millis();
    while (millis() - start < 50) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(1100, 40);
});

// Toggle OFF - falling tone
REGISTER_AUDIO_CUSTOM(toggle_off, []() {
    audioPlayTone(1100, 30);
    uint32_t start = millis();
    while (millis() - start < 50) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(700, 40);
});

// ============================================================================
// Value Editing - Functional, Subtle Sounds
// ============================================================================

// Enter edit mode - distinctive two-tone up
REGISTER_AUDIO_CUSTOM(edit_start, []() {
    audioPlayTone(880, 35);
    uint32_t start = millis();
    while (millis() - start < 60) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(1320, 35);
});

// Adjust value - single soft tick (different for up/down could be added)
REGISTER_AUDIO(edit_adjust, 1000, 15);

// Save edited value - satisfying confirmation
REGISTER_AUDIO_CUSTOM(edit_save, []() {
    audioPlayTone(800, 30);
    uint32_t start = millis();
    while (millis() - start < 50) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(1200, 30);
    start = millis();
    while (millis() - start < 50) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(1600, 50);
});

// Cancel edit - gentle descending
REGISTER_AUDIO_CUSTOM(edit_cancel, []() {
    audioPlayTone(1200, 30);
    uint32_t start = millis();
    while (millis() - start < 50) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(900, 30);
    start = millis();
    while (millis() - start < 50) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(700, 40);
});

// ============================================================================
// Communication - Network Status Sounds
// ============================================================================

// Device discovered - curious chirp
REGISTER_AUDIO_CUSTOM(peer_discovered, []() {
    audioPlayTone(1200, 30);
    uint32_t start = millis();
    while (millis() - start < 50) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(1400, 25);
    start = millis();
    while (millis() - start < 45) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(1600, 35);
});

// Pairing in progress - pulsing tone
REGISTER_AUDIO_CUSTOM(peer_request, []() {
    audioPlayTone(1000, 50);
    uint32_t start = millis();
    while (millis() - start < 90) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(1000, 50);
});

// Pairing successful - upward cascade
REGISTER_AUDIO_CUSTOM(peer_acknowledge, []() {
    audioPlayTone(800, 30);
    uint32_t start = millis();
    while (millis() - start < 45) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(1100, 30);
    start = millis();
    while (millis() - start < 45) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(1400, 40);
    start = millis();
    while (millis() - start < 55) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(1800, 50);
});

// Unpaired/disconnected - descending tone
REGISTER_AUDIO_CUSTOM(unpaired, []() {
    audioPlayTone(1200, 30);
    uint32_t start = millis();
    while (millis() - start < 50) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(800, 40);
});

// ============================================================================
// Errors - Descending, Attention-Getting Tones
// ============================================================================

// General error - double descending beep
REGISTER_AUDIO_CUSTOM(error, []() {
    audioPlayTone(800, 50);
    uint32_t start = millis();
    while (millis() - start < 80) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(600, 50);
    start = millis();
    while (millis() - start < 130) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(800, 50);
    start = millis();
    while (millis() - start < 80) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(600, 50);
});

// Timeout warning - urgent triple beep
REGISTER_AUDIO_CUSTOM(timeout_warning, []() {
    audioPlayTone(900, 40);
    uint32_t start = millis();
    while (millis() - start < 90) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(900, 40);
    start = millis();
    while (millis() - start < 90) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(900, 40);
});

// ============================================================================
// System Startup - Welcoming Melody
// ============================================================================

// Startup melody - friendly ascending tune
REGISTER_AUDIO_CUSTOM(startup, []() {
    audioPlayTone(523, 80);   // C
    uint32_t start = millis();
    while (millis() - start < 140) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(659, 80);   // E
    start = millis();
    while (millis() - start < 140) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(784, 80);   // G
    start = millis();
    while (millis() - start < 140) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(1047, 120); // High C
});

// ============================================================================
// Module-Specific Sounds
// ============================================================================

// Arm deployed/retracted - mechanical sound
REGISTER_AUDIO_CUSTOM(arm_actuate, []() {
    for (int i = 0; i < 3; i++) {
        audioPlayTone(400 + (i * 100), 25);
        uint32_t start = millis();
        while (millis() - start < 40) {
            audioUpdate();
            delayMicroseconds(100);
        }
    }
});

// Flight mode change - whoosh
REGISTER_AUDIO_CUSTOM(mode_change, []() {
    for (int freq = 1200; freq <= 1800; freq += 100) {
        audioPlayTone(freq, 20);
        uint32_t start = millis();
        while (millis() - start < 25) {
            audioUpdate();
            delayMicroseconds(100);
        }
    }
});

// Gripper action - pinch sound
REGISTER_AUDIO_CUSTOM(gripper, []() {
    audioPlayTone(1500, 25);
    uint32_t start = millis();
    while (millis() - start < 35) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(1200, 25);
    start = millis();
    while (millis() - start < 35) {
        audioUpdate();
        delayMicroseconds(100);
    }
    audioPlayTone(1000, 35);
});

void ensureDefaultAudioCuesRegistered() {
    // Intentionally empty; referencing this symbol forces the linker to keep
    // this translation unit so that the static cue registrars run at startup.
}
