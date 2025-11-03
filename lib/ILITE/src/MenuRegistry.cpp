/**
 * @file MenuRegistry.cpp
 * @brief Menu Registry Implementation with Hierarchical Navigation
 */

#include "MenuRegistry.h"
#include <cstring>
#include <algorithm>

// Static storage
std::vector<MenuEntry> MenuRegistry::entries_;

// ============================================================================
// Registration
// ============================================================================

void MenuRegistry::registerEntry(const MenuEntry& entry) {
    // Check for duplicate IDs
    for (const auto& existing : entries_) {
        if (strcmp(existing.id, entry.id) == 0) {
            Serial.printf("[MenuRegistry] WARNING: Duplicate entry '%s' (ignoring)\n", entry.id);
            return;
        }
    }

    entries_.push_back(entry);
    Serial.printf("[MenuRegistry] Registered menu entry: %s (parent: %s)\n",
                  entry.id, entry.parent ? entry.parent : "root");
}

// ============================================================================
// Queries
// ============================================================================

const MenuEntry* MenuRegistry::getEntry(MenuID id) {
    if (id == nullptr) {
        return nullptr;
    }

    for (const auto& entry : entries_) {
        if (strcmp(entry.id, id) == 0) {
            return &entry;
        }
    }

    return nullptr;
}

std::vector<const MenuEntry*> MenuRegistry::getEntriesInMenu(MenuID parentId) {
    std::vector<const MenuEntry*> result;

    for (auto& entry : entries_) {
        // Check if parent matches
        bool parentMatches = false;
        if (parentId == nullptr && entry.parent == nullptr) {
            parentMatches = true;
        } else if (parentId != nullptr && entry.parent != nullptr &&
                   strcmp(entry.parent, parentId) == 0) {
            parentMatches = true;
        }

        if (parentMatches) {
            result.push_back(&entry);
        }
    }

    // Sort by priority
    std::sort(result.begin(), result.end(), compareByPriority);

    return result;
}

std::vector<const MenuEntry*> MenuRegistry::getVisibleEntries(MenuID parentId) {
    std::vector<const MenuEntry*> allEntries = getEntriesInMenu(parentId);
    std::vector<const MenuEntry*> visible;

    for (const MenuEntry* entry : allEntries) {
        // Check visibility condition
        if (entry->condition) {
            if (entry->condition()) {
                visible.push_back(entry);
            }
        } else {
            // No condition = always visible
            visible.push_back(entry);
        }
    }

    return visible;
}

bool MenuRegistry::hasChildren(MenuID id) {
    if (id == nullptr) {
        return false;
    }

    for (const auto& entry : entries_) {
        if (entry.parent != nullptr && strcmp(entry.parent, id) == 0) {
            return true;
        }
    }

    return false;
}

std::vector<MenuEntry>& MenuRegistry::getAllEntries() {
    return entries_;
}

void MenuRegistry::clear() {
    entries_.clear();
}

// ============================================================================
// Helpers
// ============================================================================

bool MenuRegistry::compareByPriority(const MenuEntry* a, const MenuEntry* b) {
    if (a == nullptr || b == nullptr) {
        return false;
    }
    return a->priority < b->priority;
}

// ============================================================================
// Built-in Menu Structure
// ============================================================================

void MenuRegistry::initBuiltInMenus() {
    Serial.println("[MenuRegistry] Initializing built-in menus...");

    // ========================================================================
    // Root Level Menus
    // ========================================================================

    registerEntry({
        MENU_HOME,
        MENU_ROOT,
        ICON_HOME,
        "Home",
        nullptr,
        nullptr,  // No callback - opens submenu
        nullptr,
        nullptr,
        0,
        true,  // Is submenu
        false,
        nullptr,
        false,
        nullptr
    });

    registerEntry({
        MENU_MODULES,
        MENU_ROOT,
        ICON_ROBOT,
        "Modules",
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        10,
        true,
        false,
        nullptr,
        false,
        nullptr
    });

    registerEntry({
        MENU_SETTINGS,
        MENU_ROOT,
        ICON_SETTINGS,
        "Settings",
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        20,
        true,
        false,
        nullptr,
        false,
        nullptr
    });

    registerEntry({
        MENU_QUICK_ACTIONS,
        MENU_ROOT,
        ICON_PLAY,
        "Quick Actions",
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        30,
        true,
        false,
        nullptr,
        false,
        nullptr
    });

    registerEntry({
        MENU_LOGS,
        MENU_ROOT,
        ICON_INFO,
        "Logs",
        nullptr,
        nullptr,  // Screen will be opened by framework
        nullptr,
        nullptr,
        40,
        false,
        false,
        nullptr,
        false,
        nullptr
    });

    registerEntry({
        MENU_ABOUT,
        MENU_ROOT,
        ICON_INFO,
        "About",
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        50,
        false,
        false,
        nullptr,
        false,
        nullptr
    });

    // ========================================================================
    // Settings Submenus
    // ========================================================================

    registerEntry({
        MENU_DISPLAY,
        MENU_SETTINGS,
        ICON_HOME,
        "Display",
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0,
        true,
        false,
        nullptr,
        false,
        nullptr
    });

    registerEntry({
        MENU_AUDIO,
        MENU_SETTINGS,
        ICON_SETTINGS,
        "Audio",
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        10,
        true,
        false,
        nullptr,
        false,
        nullptr
    });

    registerEntry({
        MENU_CONTROLS,
        MENU_SETTINGS,
        ICON_JOYSTICK,
        "Controls",
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        20,
        true,
        false,
        nullptr,
        false,
        nullptr
    });

    registerEntry({
        MENU_NETWORK,
        MENU_SETTINGS,
        ICON_SIGNAL_FULL,
        "Network",
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        30,
        true,
        false,
        nullptr,
        false,
        nullptr
    });

    // ========================================================================
    // Display Settings
    // ========================================================================

    registerEntry({
        "display.brightness",
        MENU_DISPLAY,
        ICON_SETTINGS,
        "Brightness",
        nullptr,
        nullptr,
        nullptr,
        []() { return "Auto"; },  // Placeholder value getter
        0,
        false,
        false,
        nullptr,
        false,
        nullptr
    });

    registerEntry({
        "display.contrast",
        MENU_DISPLAY,
        ICON_SETTINGS,
        "Contrast",
        nullptr,
        nullptr,
        nullptr,
        []() { return "128"; },
        10,
        false,
        false,
        nullptr,
        false,
        nullptr
    });

    // ========================================================================
    // Audio Settings
    // ========================================================================

    registerEntry({
        "audio.enable",
        MENU_AUDIO,
        ICON_SETTINGS,
        "Enable Audio",
        nullptr,
        nullptr,  // Callback set by framework
        nullptr,
        nullptr,
        0,
        false,
        true,  // Is toggle
        []() { return true; },  // Placeholder state getter
        false,
        nullptr
    });

    registerEntry({
        "audio.volume",
        MENU_AUDIO,
        ICON_SETTINGS,
        "Volume",
        nullptr,
        nullptr,
        nullptr,
        []() { return "50%"; },
        10,
        false,
        false,
        nullptr,
        false,
        nullptr
    });

    // ========================================================================
    // Control Settings
    // ========================================================================

    registerEntry({
        "controls.deadzone",
        MENU_CONTROLS,
        ICON_JOYSTICK,
        "Joystick Deadzone",
        nullptr,
        nullptr,
        nullptr,
        []() { return "5%"; },
        0,
        false,
        false,
        nullptr,
        false,
        nullptr
    });

    registerEntry({
        "controls.sensitivity",
        MENU_CONTROLS,
        ICON_JOYSTICK,
        "Sensitivity",
        nullptr,
        nullptr,
        nullptr,
        []() { return "100%"; },
        10,
        false,
        false,
        nullptr,
        false,
        nullptr
    });

    registerEntry({
        "controls.filtering",
        MENU_CONTROLS,
        ICON_SETTINGS,
        "Input Filtering",
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        20,
        false,
        true,
        []() { return true; },
        false,
        nullptr
    });

    Serial.printf("[MenuRegistry] Initialized %zu built-in menu entries\n", entries_.size());
}
