/**
 * @file AudioCues.h
 * @brief Helper to ensure built-in audio cues are linked into the binary.
 *
 * The default cue definitions live inside AudioCues.cpp and rely on static
 * initializers. Calling this function forces the translation unit to be linked,
 * guaranteeing those initializers execute at boot.
 */
#pragma once

/**
 * @brief Force-link the built-in audio cues translation unit.
 */
void ensureDefaultAudioCuesRegistered();

