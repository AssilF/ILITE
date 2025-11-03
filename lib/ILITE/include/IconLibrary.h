/**
 * @file IconLibrary.h
 * @brief Icon Library - Bitmap Icon Management System
 *
 * Provides a centralized system for managing bitmap icons used throughout
 * the UI. Icons can be 8x8, 16x16, or 32x32 pixels (1-bit monochrome).
 *
 * @example
 * ```cpp
 * // Register custom icon
 * REGISTER_ICON(ICON_MY_DRONE, 16, 16, {
 *     0b00000001, 0b10000000,
 *     0b00000111, 0b11100000,
 *     // ... 14 more rows
 * });
 *
 * // Use in drawing code
 * canvas.drawIcon(0, 0, ICON_MY_DRONE);
 * ```
 *
 * @author ILITE Framework
 * @version 1.0.0
 */

#ifndef ILITE_ICON_LIBRARY_H
#define ILITE_ICON_LIBRARY_H

#include <Arduino.h>
#include <vector>

/// Icon ID type
using IconID = const char*;

/**
 * @brief Icon data structure
 */
struct Icon {
    IconID id;               ///< Unique identifier "icon_drone"
    uint8_t width;           ///< Width in pixels (8, 16, or 32)
    uint8_t height;          ///< Height in pixels (8, 16, or 32)
    const uint8_t* data;     ///< Bitmap data (1 bit per pixel, row-major)
};

/**
 * @brief Icon Library - Manages bitmap icons
 */
class IconLibrary {
public:
    /**
     * @brief Register a custom icon
     * @param icon Icon definition
     */
    static void registerIcon(const Icon& icon);

    /**
     * @brief Get icon by ID
     * @param id Icon ID (e.g., "icon_drone")
     * @return Pointer to icon, or nullptr if not found
     */
    static const Icon* getIcon(IconID id);

    /**
     * @brief Check if icon exists
     * @param id Icon ID
     * @return true if registered
     */
    static bool hasIcon(IconID id);

    /**
     * @brief Get all registered icons
     * @return Vector of icons
     */
    static std::vector<Icon>& getIcons();

    /**
     * @brief Clear all icons
     */
    static void clear();

    /**
     * @brief Initialize built-in icon set
     *
     * Registers standard icons (home, settings, battery, signal, etc.)
     * Called automatically by framework.
     */
    static void initBuiltInIcons();

private:
    static std::vector<Icon> icons_;
};

// ============================================================================
// Built-in Icon IDs (always available)
// ============================================================================

#define ICON_HOME           "icon_home"
#define ICON_SETTINGS       "icon_settings"
#define ICON_INFO           "icon_info"
#define ICON_WARNING        "icon_warning"
#define ICON_ERROR          "icon_error"
#define ICON_BATTERY_FULL   "icon_battery_full"
#define ICON_BATTERY_MED    "icon_battery_med"
#define ICON_BATTERY_LOW    "icon_battery_low"
#define ICON_SIGNAL_FULL    "icon_signal_full"
#define ICON_SIGNAL_MED     "icon_signal_med"
#define ICON_SIGNAL_LOW     "icon_signal_low"
#define ICON_SIGNAL_NONE    "icon_signal_none"
#define ICON_JOYSTICK       "icon_joystick"
#define ICON_DRONE          "icon_drone"
#define ICON_ROBOT          "icon_robot"
#define ICON_CAR            "icon_car"
#define ICON_TUNING         "icon_tuning"
#define ICON_LOCK           "icon_lock"
#define ICON_UNLOCK         "icon_unlock"
#define ICON_PLAY           "icon_play"
#define ICON_PAUSE          "icon_pause"
#define ICON_STOP           "icon_stop"
#define ICON_UP             "icon_up"
#define ICON_DOWN           "icon_down"
#define ICON_LEFT           "icon_left"
#define ICON_RIGHT          "icon_right"
#define ICON_CHECK          "icon_check"
#define ICON_CROSS          "icon_cross"
#define ICON_MENU           "icon_menu"
#define ICON_BACK           "icon_back"

// ============================================================================
// Registration Macro
// ============================================================================

/**
 * @brief Macro for registering custom icons
 *
 * @param id Icon ID constant (must be unique)
 * @param w Width in pixels
 * @param h Height in pixels
 * @param bitmap Initializer list of uint8_t bytes
 *
 * @example
 * ```cpp
 * // 8x8 icon (1 byte per row)
 * REGISTER_ICON(ICON_CUSTOM, 8, 8, {
 *     0b00011000,
 *     0b00111100,
 *     0b01111110,
 *     0b11111111,
 *     0b11111111,
 *     0b01111110,
 *     0b00111100,
 *     0b00011000
 * });
 *
 * // 16x16 icon (2 bytes per row)
 * REGISTER_ICON(ICON_BIG, 16, 16, {
 *     0b00000001, 0b10000000,
 *     0b00000111, 0b11100000,
 *     // ... 14 more rows
 * });
 * ```
 */
#define REGISTER_ICON(id, w, h, ...) \
    namespace { \
        static const uint8_t g_iconData_##id[] = __VA_ARGS__; \
        struct IconRegistrar_##id { \
            IconRegistrar_##id() { \
                IconLibrary::registerIcon({ \
                    id, \
                    w, \
                    h, \
                    g_iconData_##id \
                }); \
            } \
        }; \
        static IconRegistrar_##id g_iconRegistrar_##id; \
    }

#endif // ILITE_ICON_LIBRARY_H
