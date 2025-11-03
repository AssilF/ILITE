/**
 * @file UIComponents.h
 * @brief Modern UI Components Library
 *
 * Provides high-level UI components for building consistent, icon-based
 * interfaces. All components use the IconLibrary and DisplayCanvas.
 *
 * @example
 * ```cpp
 * UIComponents::drawModernHeader(canvas, "DroneGaze", ICON_DRONE, 85, 4);
 * UIComponents::drawModernMenu(canvas, menuEntries, focusedIndex);
 * UIComponents::drawBreadcrumb(canvas, "Home > Settings > Audio");
 * ```
 *
 * @author ILITE Framework
 * @version 1.0.0
 */

#ifndef ILITE_UI_COMPONENTS_H
#define ILITE_UI_COMPONENTS_H

#include <Arduino.h>
#include "DisplayCanvas.h"
#include "IconLibrary.h"

/**
 * @brief Modern UI Components - High-level interface building blocks
 */
class UIComponents {
public:
    // ========================================================================
    // Layout Constants
    // ========================================================================

    static constexpr int16_t HEADER_HEIGHT = 16;
    static constexpr int16_t FOOTER_HEIGHT = 12;
    static constexpr int16_t STATUS_BAR_HEIGHT = 10;
    static constexpr int16_t MENU_ITEM_HEIGHT = 14;
    static constexpr int16_t BREADCRUMB_HEIGHT = 10;

    // ========================================================================
    // Modern Header
    // ========================================================================

    /**
     * @brief Draw modern header with icon, title, and status indicators
     *
     * Layout:
     * ```
     * [ICON] Module Name    [BAT] [SIG]
     * ────────────────────────────────
     * ```
     *
     * @param canvas Drawing canvas
     * @param title Module/screen name
     * @param icon Icon ID (8x8 or 16x16)
     * @param batteryPercent Battery level 0-100 (0 = hide)
     * @param signalStrength Signal strength 0-4 (0 = hide)
     * @param showDivider Draw divider line below header
     */
    static void drawModernHeader(DisplayCanvas& canvas,
                                 const char* title,
                                 IconID icon = nullptr,
                                 uint8_t batteryPercent = 0,
                                 uint8_t signalStrength = 0,
                                 bool showDivider = true);

    /**
     * @brief Draw battery icon with percentage
     *
     * Automatically selects icon based on charge level:
     * - 75-100%: ICON_BATTERY_FULL
     * - 30-74%:  ICON_BATTERY_MED
     * - 0-29%:   ICON_BATTERY_LOW
     *
     * @param canvas Drawing canvas
     * @param x X position (top-right of icon)
     * @param y Y position (top of icon)
     * @param percent Battery level 0-100
     * @return Width occupied by battery icon + text
     */
    static int16_t drawBatteryIndicator(DisplayCanvas& canvas,
                                        int16_t x, int16_t y,
                                        uint8_t percent);

    /**
     * @brief Draw signal strength icon
     *
     * Icons based on strength:
     * - 4: ICON_SIGNAL_FULL (all bars)
     * - 3: ICON_SIGNAL_MED (3 bars)
     * - 2: ICON_SIGNAL_LOW (2 bars)
     * - 0-1: ICON_SIGNAL_NONE (1 bar)
     *
     * @param canvas Drawing canvas
     * @param x X position (top-right of icon)
     * @param y Y position (top of icon)
     * @param strength Signal strength 0-4
     * @return Width occupied by signal icon
     */
    static int16_t drawSignalIndicator(DisplayCanvas& canvas,
                                       int16_t x, int16_t y,
                                       uint8_t strength);

    // ========================================================================
    // Modern Footer
    // ========================================================================

    /**
     * @brief Draw modern footer with button labels and stats
     *
     * Layout:
     * ```
     * ────────────────────────────────
     * [BTN1] [BTN2] [BTN3]    TX:123
     * ```
     *
     * @param canvas Drawing canvas
     * @param button1Label Button 1 label (max 6 chars)
     * @param button2Label Button 2 label (max 6 chars)
     * @param button3Label Button 3 label (max 6 chars)
     * @param statLabel Optional stat label (e.g., "TX:123")
     * @param showDivider Draw divider line above footer
     */
    static void drawModernFooter(DisplayCanvas& canvas,
                                 const char* button1Label = nullptr,
                                 const char* button2Label = nullptr,
                                 const char* button3Label = nullptr,
                                 const char* statLabel = nullptr,
                                 bool showDivider = true);

    // ========================================================================
    // Breadcrumb Navigation
    // ========================================================================

    /**
     * @brief Draw breadcrumb navigation trail
     *
     * Example: "Home > Settings > Audio"
     *
     * @param canvas Drawing canvas
     * @param breadcrumb Breadcrumb string with " > " separators
     * @param y Y position (default top of screen)
     * @param showBackIcon Show [<] back arrow on left
     */
    static void drawBreadcrumb(DisplayCanvas& canvas,
                              const char* breadcrumb,
                              int16_t y = 0,
                              bool showBackIcon = true);

    // ========================================================================
    // Menu Rendering
    // ========================================================================

    /**
     * @brief Menu item rendering options
     */
    struct MenuItem {
        IconID icon;                ///< Icon ID (8x8)
        const char* label;          ///< Item label
        const char* value;          ///< Optional value display (e.g., "On", "50%")
        bool hasSubmenu;            ///< Show > arrow
        bool isToggle;              ///< Show checkmark if enabled
        bool toggleState;           ///< Toggle state (only if isToggle)
        bool isReadOnly;            ///< Grayed out, non-selectable
    };

    /**
     * @brief Draw modern menu list
     *
     * Features:
     * - Icon on left (8x8)
     * - Label in center
     * - Value/indicator on right
     * - Submenu arrow (>)
     * - Toggle checkmark (✓)
     * - Focus highlight
     * - Smooth scrolling
     *
     * Layout:
     * ```
     * ┌────────────────────────────┐
     * │ [ICON] Label          Val│ ← Item 0
     * │ [ICON] Label            >│ ← Item 1 (submenu)
     * │▶[ICON] Label           ✓│ ← Item 2 (focused, toggled)
     * │ [ICON] Label              │ ← Item 3
     * └────────────────────────────┘
     * ```
     *
     * @param canvas Drawing canvas
     * @param items Array of menu items
     * @param itemCount Number of items
     * @param focusedIndex Currently focused item
     * @param scrollOffset Vertical scroll offset (items)
     * @param y Top Y position (default after breadcrumb)
     * @param maxVisible Maximum visible items (default 4)
     */
    static void drawModernMenu(DisplayCanvas& canvas,
                              const MenuItem* items,
                              size_t itemCount,
                              int focusedIndex,
                              int scrollOffset = 0,
                              int16_t y = BREADCRUMB_HEIGHT + 2,
                              int maxVisible = 4);

    /**
     * @brief Draw scroll indicator (e.g., "3 / 12")
     *
     * @param canvas Drawing canvas
     * @param currentIndex Current item (1-indexed)
     * @param totalCount Total items
     * @param y Y position (default bottom of screen)
     */
    static void drawScrollIndicator(DisplayCanvas& canvas,
                                    int currentIndex,
                                    int totalCount,
                                    int16_t y = 64 - 8);

    // ========================================================================
    // Module Selector
    // ========================================================================

    /**
     * @brief Module card data
     */
    struct ModuleCard {
        IconID icon;                ///< Large icon (16x16 recommended)
        const char* name;           ///< Module name
        const char* type;           ///< Module type (e.g., "Quadcopter")
        const char* version;        ///< Version string
        bool isActive;              ///< Currently active module
    };

    /**
     * @brief Draw large module selection card
     *
     * Layout:
     * ```
     * ┌──────────────────────────┐
     * │ [ICON]  DroneGaze        │ ← Large 32x32 icon + name
     * │          Quadcopter       │ ← Type
     * │          v2.1.0           │ ← Version
     * │          ✓ ACTIVE         │ ← Status (if active)
     * └──────────────────────────┘
     * ```
     *
     * @param canvas Drawing canvas
     * @param card Module card data
     * @param x X position
     * @param y Y position
     * @param width Card width
     * @param height Card height
     * @param focused Focused (double border)
     */
    static void drawModuleCard(DisplayCanvas& canvas,
                              const ModuleCard& card,
                              int16_t x, int16_t y,
                              int16_t width, int16_t height,
                              bool focused);

    // ========================================================================
    // Dashboard Widgets (Enhanced)
    // ========================================================================

    /**
     * @brief Draw labeled value with icon
     *
     * Layout: [ICON] Label: Value
     *
     * @param canvas Drawing canvas
     * @param x X position
     * @param y Y position
     * @param icon Icon ID (8x8)
     * @param label Label text
     * @param value Value text
     */
    static void drawLabeledValue(DisplayCanvas& canvas,
                                 int16_t x, int16_t y,
                                 IconID icon,
                                 const char* label,
                                 const char* value);

    /**
     * @brief Draw mini gauge with icon and label
     *
     * Compact circular gauge for dashboards.
     *
     * @param canvas Drawing canvas
     * @param x Center X
     * @param y Center Y
     * @param radius Gauge radius
     * @param value Current value
     * @param min Minimum value
     * @param max Maximum value
     * @param icon Icon ID (8x8)
     * @param label Label below gauge
     */
    static void drawMiniGauge(DisplayCanvas& canvas,
                             int16_t x, int16_t y,
                             int16_t radius,
                             float value, float min, float max,
                             IconID icon,
                             const char* label);

    /**
     * @brief Draw connection status indicator
     *
     * Shows connection state with icon + label
     * - Paired: Green check + "Connected"
     * - Searching: Animated dots + "Searching..."
     * - Failed: X + "No Connection"
     *
     * @param canvas Drawing canvas
     * @param x X position
     * @param y Y position
     * @param isPaired Connection state
     * @param isSearching Searching animation
     */
    static void drawConnectionStatus(DisplayCanvas& canvas,
                                     int16_t x, int16_t y,
                                     bool isPaired,
                                     bool isSearching = false);

    // ========================================================================
    // Modal/Dialog
    // ========================================================================

    /**
     * @brief Draw modal dialog with icon, message, and buttons
     *
     * Layout:
     * ```
     * ┌────────────────────────┐
     * │     [ICON]             │
     * │   Confirm Action?      │
     * │                        │
     * │  [Cancel]   [OK]       │
     * └────────────────────────┘
     * ```
     *
     * @param canvas Drawing canvas
     * @param icon Icon ID (16x16 recommended)
     * @param message Message text (auto-wraps)
     * @param button1Label Left button label
     * @param button2Label Right button label
     * @param focusedButton Focused button (0 = left, 1 = right)
     */
    static void drawModal(DisplayCanvas& canvas,
                         IconID icon,
                         const char* message,
                         const char* button1Label = "Cancel",
                         const char* button2Label = "OK",
                         int focusedButton = 1);

    // ========================================================================
    // Helpers
    // ========================================================================

    /**
     * @brief Calculate scroll offset to keep focused item visible
     *
     * @param focusedIndex Currently focused item
     * @param scrollOffset Current scroll offset
     * @param maxVisible Maximum visible items
     * @return New scroll offset
     */
    static int calculateScrollOffset(int focusedIndex,
                                     int scrollOffset,
                                     int maxVisible);
};

#endif // ILITE_UI_COMPONENTS_H
