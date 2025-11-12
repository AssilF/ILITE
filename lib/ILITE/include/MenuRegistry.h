/**
 * @file MenuRegistry.h
 * @brief Menu Extension System - Custom Menu Entry Registration
 *
 * Allows users to register custom menu entries with hierarchical structure,
 * conditional visibility, icons, and callbacks.
 *
 * @example
 * ```cpp
 * REGISTER_MENU_ENTRY(
 *     .id = "mydrone.arm",
 *     .parent = "quick_actions",
 *     .icon = ICON_UNLOCK,
 *     .label = "Arm Motors",
 *     .condition = []() { return myDrone.isPaired() && !myDrone.armed; },
 *     .onSelect = []() {
 *         myDrone.armed = true;
 *         AudioRegistry::play("motor_armed");
 *     }
 * );
 * ```
 *
 * @author ILITE Framework
 * @version 1.0.0
 */

#ifndef ILITE_MENU_REGISTRY_H
#define ILITE_MENU_REGISTRY_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include <cstddef>
#include "IconLibrary.h"

/// Menu entry ID type
using MenuID = const char*;

/**
 * @brief Menu entry definition
 */
struct MenuEntry {
    MenuID id;                         ///< Unique identifier "mydrone.arm"
    MenuID parent;                     ///< Parent menu ID (nullptr = root level)
    IconID icon;                       ///< Icon ID from IconLibrary
    const char* label;                 ///< Display label "Arm Motors"
    const char* shortLabel;            ///< Short label for compact display (optional)

    /// Callback when entry is selected
    std::function<void()> onSelect;

    /// Condition for visibility (return false to hide entry)
    std::function<bool()> condition;

    /// Display value (e.g., "Enabled", "50%")
    std::function<const char*()> getValue;

    /// Priority for sorting (lower = higher in list)
    int priority;

    /// Whether this entry opens a submenu
    bool isSubmenu;

    /// Whether this entry is a toggle (show checkmark when enabled)
    bool isToggle;

    /// Get toggle state (only valid if isToggle == true)
    std::function<bool()> getToggleState;

    /// Whether this entry is a read-only info display
    bool isReadOnly;

    /// Custom draw function (advanced - overrides default rendering)
    std::function<void(int16_t x, int16_t y, int16_t w, bool focused)> customDraw;

    // Editable value support
    bool isEditableInt = false;        ///< Whether this is an editable integer
    bool isEditableFloat = false;      ///< Whether this is an editable float
    std::function<int()> getIntValue;  ///< Get integer value
    std::function<void(int)> setIntValue; ///< Set integer value
    std::function<float()> getFloatValue; ///< Get float value
    std::function<void(float)> setFloatValue; ///< Set float value
    int minValue = 0;                  ///< Minimum value
    int maxValue = 100;                ///< Maximum value
    float minValueFloat = 0.0f;        ///< Minimum float value
    float maxValueFloat = 100.0f;      ///< Maximum float value
    float step = 1.0f;                 ///< Fine step size
    float coarseStep = 10.0f;          ///< Coarse step size (for fast rotation)

    // Editable string support
    bool isEditableString = false;      ///< Whether this entry edits a string
    size_t maxStringLength = 32;        ///< Maximum characters (excluding null)
    std::function<void(char*, size_t)> getStringValueForEdit; ///< Populate buffer with current value
    std::function<void(const char*)> setStringValue;          ///< Apply edited value
};

/**
 * @brief Menu Registry - Manages hierarchical menu structure
 */
class MenuRegistry {
public:
    /**
     * @brief Register a menu entry
     * @param entry Menu entry definition
     */
    static void registerEntry(const MenuEntry& entry);

    /**
     * @brief Get all entries at a given level
     * @param parentId Parent ID (nullptr = root level)
     * @return Vector of entries (sorted by priority)
     */
    static std::vector<const MenuEntry*> getEntriesInMenu(MenuID parentId);

    /**
     * @brief Get entry by ID
     * @param id Entry ID
     * @return Pointer to entry, or nullptr if not found
     */
    static const MenuEntry* getEntry(MenuID id);

    /**
     * @brief Check if entry has children (is a submenu)
     * @param id Entry ID
     * @return true if has children
     */
    static bool hasChildren(MenuID id);

    /**
     * @brief Get visible entries in menu (applies conditions)
     * @param parentId Parent ID
     * @return Vector of visible entries only
     */
    static std::vector<const MenuEntry*> getVisibleEntries(MenuID parentId);

    /**
     * @brief Get all registered entries
     * @return Vector of all entries
     */
    static std::vector<MenuEntry>& getAllEntries();

    /**
     * @brief Clear all entries
     */
    static void clear();

    /**
     * @brief Remove a specific entry by ID
     * @param id Entry ID to remove
     * @return true if entry was found and removed
     */
    static bool removeEntry(MenuID id);

    /**
     * @brief Remove all entries with a specific parent
     * @param parentId Parent ID
     * @return Number of entries removed
     */
    static int removeEntriesByParent(MenuID parentId);

    /**
     * @brief Find entry by ID (non-const version for internal use)
     * @param id Entry ID
     * @return Pointer to entry, or nullptr if not found
     */
    static MenuEntry* findEntry(MenuID id);

    /**
     * @brief Initialize built-in menu structure
     *
     * Creates default menus: Home, Settings, Quick Actions, etc.
     * Called automatically by framework.
     */
    static void initBuiltInMenus();

private:
    static std::vector<MenuEntry> entries_;

    /// Helper: Compare entries by priority
    static bool compareByPriority(const MenuEntry* a, const MenuEntry* b);
};

// ============================================================================
// Built-in Menu IDs
// ============================================================================

#define MENU_ROOT          nullptr
#define MENU_HOME          "home"
#define MENU_SETTINGS      "settings"
#define MENU_QUICK_ACTIONS "quick_actions"
#define MENU_MODULES       "modules"
#define MENU_DISPLAY       "display"
#define MENU_AUDIO         "audio"
#define MENU_CONTROLS      "controls"
#define MENU_NETWORK       "network"
#define MENU_ABOUT         "about"
#define MENU_LOGS          "logs"

// ============================================================================
// Registration Macros
// ============================================================================

/**
 * @brief Macro for registering simple menu entries
 *
 * @example
 * ```cpp
 * REGISTER_MENU_ENTRY_SIMPLE(
 *     "drone.arm",           // ID
 *     "quick_actions",       // Parent
 *     ICON_UNLOCK,           // Icon
 *     "Arm Motors",          // Label
 *     []() { arm(); }        // Callback
 * );
 * ```
 */
#define REGISTER_MENU_ENTRY_SIMPLE(id, parent, icon, label, callback) \
    namespace { \
        struct MenuEntryRegistrar_##id { \
            MenuEntryRegistrar_##id() { \
                MenuRegistry::registerEntry({ \
                    #id, \
                    parent, \
                    icon, \
                    label, \
                    nullptr, \
                    callback, \
                    nullptr, \
                    nullptr, \
                    0, \
                    false, \
                    false, \
                    nullptr, \
                    false, \
                    nullptr \
                }); \
            } \
        }; \
        static MenuEntryRegistrar_##id g_menuEntryRegistrar_##id; \
    }

/**
 * @brief Macro for registering conditional menu entries
 *
 * @example
 * ```cpp
 * REGISTER_MENU_ENTRY_CONDITIONAL(
 *     "drone.disarm",        // ID
 *     "quick_actions",       // Parent
 *     ICON_LOCK,             // Icon
 *     "Disarm Motors",       // Label
 *     []() { return drone.armed; },  // Condition
 *     []() { drone.armed = false; }  // Callback
 * );
 * ```
 */
#define REGISTER_MENU_ENTRY_CONDITIONAL(id, parent, icon, label, cond, callback) \
    namespace { \
        struct MenuEntryRegistrar_##id { \
            MenuEntryRegistrar_##id() { \
                MenuRegistry::registerEntry({ \
                    #id, \
                    parent, \
                    icon, \
                    label, \
                    nullptr, \
                    callback, \
                    cond, \
                    nullptr, \
                    0, \
                    false, \
                    false, \
                    nullptr, \
                    false, \
                    nullptr \
                }); \
            } \
        }; \
        static MenuEntryRegistrar_##id g_menuEntryRegistrar_##id; \
    }

/**
 * @brief Macro for registering submenu entries
 *
 * @example
 * ```cpp
 * REGISTER_MENU_SUBMENU(
 *     "drone_settings",      // ID
 *     "settings",            // Parent
 *     ICON_DRONE,            // Icon
 *     "Drone Settings"       // Label
 * );
 * ```
 */
#define REGISTER_MENU_SUBMENU(id, parent, icon, label) \
    namespace { \
        struct MenuEntryRegistrar_##id { \
            MenuEntryRegistrar_##id() { \
                MenuRegistry::registerEntry({ \
                    #id, \
                    parent, \
                    icon, \
                    label, \
                    nullptr, \
                    nullptr, \
                    nullptr, \
                    nullptr, \
                    0, \
                    true, \
                    false, \
                    nullptr, \
                    false, \
                    nullptr \
                }); \
            } \
        }; \
        static MenuEntryRegistrar_##id g_menuEntryRegistrar_##id; \
    }

/**
 * @brief Macro for registering toggle menu entries
 *
 * @example
 * ```cpp
 * REGISTER_MENU_TOGGLE(
 *     "audio_enabled",       // ID
 *     "audio",               // Parent
 *     ICON_SETTINGS,         // Icon
 *     "Enable Audio",        // Label
 *     []() { return audio.isEnabled(); },    // Get state
 *     []() { audio.setEnabled(!audio.isEnabled()); }  // Toggle callback
 * );
 * ```
 */
#define REGISTER_MENU_TOGGLE(id, parent, icon, label, getState, callback) \
    namespace { \
        struct MenuEntryRegistrar_##id { \
            MenuEntryRegistrar_##id() { \
                MenuRegistry::registerEntry({ \
                    #id, \
                    parent, \
                    icon, \
                    label, \
                    nullptr, \
                    callback, \
                    nullptr, \
                    nullptr, \
                    0, \
                    false, \
                    true, \
                    getState, \
                    false, \
                    nullptr \
                }); \
            } \
        }; \
        static MenuEntryRegistrar_##id g_menuEntryRegistrar_##id; \
    }

#endif // ILITE_MENU_REGISTRY_H
