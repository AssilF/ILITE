/**
 * @file ScreenRegistry.h
 * @brief Screen Extension System - Custom Screen Registration
 *
 * Allows users to register custom full-screen UIs with event handlers.
 * Screens can be accessed from menus or triggered programmatically.
 *
 * @example
 * ```cpp
 * REGISTER_SCREEN("pid_tuning", {
 *     .title = "PID Tuning",
 *     .icon = ICON_TUNING,
 *     .drawFunc = drawPIDScreen,
 *     .onEncoderRotate = handlePIDRotate
 * });
 *
 * ScreenRegistry::show("pid_tuning");
 * ```
 *
 * @author ILITE Framework
 * @version 1.0.0
 */

#ifndef ILITE_SCREEN_REGISTRY_H
#define ILITE_SCREEN_REGISTRY_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "DisplayCanvas.h"
#include "IconLibrary.h"

/// Screen ID type
using ScreenID = const char*;

/**
 * @brief Screen definition
 */
struct Screen {
    ScreenID id;                    ///< Unique identifier "pid_tuning"
    const char* title;              ///< Screen title
    IconID icon;                    ///< Icon (optional)

    /// Draw function (required) - called at 10Hz
    std::function<void(DisplayCanvas&)> drawFunc;

    /// Update function (optional) - called at 10Hz before draw
    std::function<void()> updateFunc;

    // Input event handlers (all optional)
    std::function<void(int delta)> onEncoderRotate;
    std::function<void()> onEncoderPress;
    std::function<void()> onButton1;
    std::function<void()> onButton2;
    std::function<void()> onButton3;

    /// Whether this is a modal overlay (true) or full-screen (false)
    bool isModal;

    /// Condition for availability (optional)
    std::function<bool()> condition;

    /// Called when screen is shown
    std::function<void()> onShow;

    /// Called when screen is hidden
    std::function<void()> onHide;
};

/**
 * @brief Screen Registry - Manages custom screens and navigation stack
 */
class ScreenRegistry {
public:
    /**
     * @brief Register a screen
     * @param screen Screen definition
     */
    static void registerScreen(const Screen& screen);

    /**
     * @brief Get screen by ID
     * @param id Screen ID
     * @return Pointer to screen, or nullptr if not found
     */
    static const Screen* getScreen(ScreenID id);

    /**
     * @brief Check if screen exists
     * @param id Screen ID
     * @return true if registered
     */
    static bool hasScreen(ScreenID id);

    /**
     * @brief Get all registered screens
     * @return Vector of screens
     */
    static std::vector<Screen>& getAllScreens();

    /**
     * @brief Clear all screens
     */
    static void clear();

    // ========================================================================
    // Navigation
    // ========================================================================

    /**
     * @brief Show screen (push onto stack)
     * @param id Screen ID
     * @return true if screen found and shown, false otherwise
     */
    static bool show(ScreenID id);

    /**
     * @brief Go back to previous screen (pop stack)
     * @return true if went back, false if at bottom of stack
     */
    static bool back();

    /**
     * @brief Clear navigation stack and return to default
     */
    static void clearStack();

    /**
     * @brief Get currently active screen
     * @return Pointer to active screen, or nullptr if none
     */
    static const Screen* getActiveScreen();

    /**
     * @brief Check if a custom screen is active
     * @return true if screen stack is not empty
     */
    static bool hasActiveScreen();

    // ========================================================================
    // Event Handling
    // ========================================================================

    /**
     * @brief Handle encoder rotation
     * @param delta Rotation delta (-1 = CCW, +1 = CW)
     */
    static void handleEncoderRotate(int delta);

    /**
     * @brief Handle encoder button press
     */
    static void handleEncoderPress();

    /**
     * @brief Handle button press
     * @param button Button number (1-3)
     */
    static void handleButton(int button);

    // ========================================================================
    // Rendering
    // ========================================================================

    /**
     * @brief Draw active screen
     * @param canvas Drawing canvas
     */
    static void drawActiveScreen(DisplayCanvas& canvas);

    /**
     * @brief Update active screen (call before draw)
     */
    static void updateActiveScreen();

private:
    static std::vector<Screen> screens_;
    static std::vector<const Screen*> navigationStack_;
};

// ============================================================================
// Registration Macros
// ============================================================================

/**
 * @brief Macro for registering simple screens
 *
 * @example
 * ```cpp
 * REGISTER_SCREEN_SIMPLE(
 *     "my_screen",
 *     "My Screen",
 *     ICON_SETTINGS,
 *     drawMyScreen
 * );
 * ```
 */
#define REGISTER_SCREEN_SIMPLE(id, title, icon, drawFunc) \
    namespace { \
        struct ScreenRegistrar_##id { \
            ScreenRegistrar_##id() { \
                ScreenRegistry::registerScreen({ \
                    #id, \
                    title, \
                    icon, \
                    drawFunc, \
                    nullptr, \
                    nullptr, \
                    nullptr, \
                    nullptr, \
                    nullptr, \
                    nullptr, \
                    false, \
                    nullptr, \
                    nullptr, \
                    nullptr \
                }); \
            } \
        }; \
        static ScreenRegistrar_##id g_screenRegistrar_##id; \
    }

/**
 * @brief Macro for registering screens with full configuration
 *
 * Uses designated initializers for cleaner syntax.
 *
 * @example
 * ```cpp
 * REGISTER_SCREEN("pid_tuning", {
 *     .title = "PID Tuning",
 *     .icon = ICON_TUNING,
 *     .drawFunc = drawPIDScreen,
 *     .onEncoderRotate = handlePIDRotate
 * });
 * ```
 */
#define REGISTER_SCREEN(id, ...) \
    namespace { \
        struct ScreenRegistrar_##id { \
            ScreenRegistrar_##id() { \
                Screen screen = {#id, __VA_ARGS__}; \
                ScreenRegistry::registerScreen(screen); \
            } \
        }; \
        static ScreenRegistrar_##id g_screenRegistrar_##id; \
    }

#endif // ILITE_SCREEN_REGISTRY_H
