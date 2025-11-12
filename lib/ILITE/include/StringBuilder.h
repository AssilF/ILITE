#pragma once

/**
 * @file StringBuilder.h
 * @brief On-device keyboard + string input helper.
 *
 * Provides a reusable on-screen keyboard that captures textual input using
 * the encoder/buttons. Framework features (menus, settings, terminal, modules)
 * can invoke it to edit strings without reimplementing UI logic.
 */

#include <Arduino.h>
#include <functional>
#include <cstddef>

struct StringBuilderConfig {
    const char* title = "Text Input";            ///< Title shown at top of screen
    const char* subtitle = nullptr;              ///< Optional subtitle (instructions)
    const char* initialValue = "";               ///< Initial text (can be nullptr)
    size_t maxLength = 32;                       ///< Max characters (excluding null terminator)
    std::function<void(const char*)> onSubmit;   ///< Called when user presses OK/Enter
    std::function<void()> onCancel;              ///< Called when user cancels
};

class StringBuilder {
public:
    /**
     * @brief Launch the string builder screen.
     * @param config Session configuration
     * @return true if started, false if another session already active
     */
    static bool begin(const StringBuilderConfig& config);

    /**
     * @brief Check if keyboard UI is active.
     */
    static bool isActive();

    /**
     * @brief Cancel current session (if any).
     */
    static void cancel();

    /**
     * @brief Get current in-progress text.
     */
    static const char* getText();
};
