/**
 * @file MenuRegistry.cpp
 * @brief Menu Registry Implementation with Hierarchical Navigation
 */

#include "MenuRegistry.h"
#include "FrameworkEngine.h"
#include <cstring>
#include <algorithm>

// Helper to create MenuEntry with editable value defaults
static MenuEntry makeMenuEntry(
    MenuID id, MenuID parent, IconID icon, const char* label, const char* shortLabel,
    std::function<void()> onSelect, std::function<bool()> condition, std::function<const char*()> getValue,
    int priority, bool isSubmenu, bool isToggle, std::function<bool()> getToggleState,
    bool isReadOnly, std::function<void(int16_t, int16_t, int16_t, bool)> customDraw
) {
    MenuEntry entry;
    entry.id = id;
    entry.parent = parent;
    entry.icon = icon;
    entry.label = label;
    entry.shortLabel = shortLabel;
    entry.onSelect = onSelect;
    entry.condition = condition;
    entry.getValue = getValue;
    entry.priority = priority;
    entry.isSubmenu = isSubmenu;
    entry.isToggle = isToggle;
    entry.getToggleState = getToggleState;
    entry.isReadOnly = isReadOnly;
    entry.customDraw = customDraw;
    // Editable value fields use defaults from struct definition
    return entry;
}

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

bool MenuRegistry::removeEntry(MenuID id) {
    if (id == nullptr) {
        return false;
    }

    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (strcmp(it->id, id) == 0) {
            Serial.printf("[MenuRegistry] Removed entry: %s\n", id);
            entries_.erase(it);
            return true;
        }
    }

    return false;
}

int MenuRegistry::removeEntriesByParent(MenuID parentId) {
    int removed = 0;

    // Iterate backwards to safely erase while iterating
    for (auto it = entries_.begin(); it != entries_.end(); ) {
        bool shouldRemove = false;

        // Check if parent matches
        if (parentId == nullptr && it->parent == nullptr) {
            shouldRemove = true;
        } else if (parentId != nullptr && it->parent != nullptr &&
                   strcmp(it->parent, parentId) == 0) {
            shouldRemove = true;
        }

        if (shouldRemove) {
            Serial.printf("[MenuRegistry] Removed entry: %s (parent: %s)\n",
                          it->id, it->parent ? it->parent : "root");
            it = entries_.erase(it);
            removed++;
        } else {
            ++it;
        }
    }

    return removed;
}

MenuEntry* MenuRegistry::findEntry(MenuID id) {
    if (id == nullptr) {
        return nullptr;
    }

    for (auto& entry : entries_) {
        if (strcmp(entry.id, id) == 0) {
            return &entry;
        }
    }

    return nullptr;
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

    registerEntry(makeMenuEntry(
        MENU_HOME,
        MENU_ROOT,
        ICON_HOME,
        "Home",
        nullptr,
        []() {
            // Close menu to show dashboard
            FrameworkEngine::getInstance().closeMenu();
        },
        nullptr,
        nullptr,
        0,
        false,  // Not a submenu - directly shows dashboard
        false,
        nullptr,
        false,
        nullptr
    ));

    registerEntry(makeMenuEntry(
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
    ));

    registerEntry(makeMenuEntry(
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
    ));

    registerEntry(makeMenuEntry(
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
    ));

    registerEntry(makeMenuEntry(
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
    ));

    registerEntry(makeMenuEntry(
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
    ));

    // ========================================================================
    // Settings Submenus
    // ========================================================================

    registerEntry(makeMenuEntry(
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
    ));

    registerEntry(makeMenuEntry(
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
    ));

    registerEntry(makeMenuEntry(
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
    ));

    registerEntry(makeMenuEntry(
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
    ));

    // ========================================================================
    // Display Settings
    // ========================================================================

    registerEntry(makeMenuEntry(
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
    ));

    registerEntry(makeMenuEntry(
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
    ));

    // ========================================================================
    // Audio Settings
    // ========================================================================

    registerEntry(makeMenuEntry(
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
    ));

    registerEntry(makeMenuEntry(
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
    ));

    // ========================================================================
    // Control Settings
    // ========================================================================

    registerEntry(makeMenuEntry(
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
    ));

    registerEntry(makeMenuEntry(
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
    ));

    registerEntry(makeMenuEntry(
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
    ));

    Serial.printf("[MenuRegistry] Initialized %zu built-in menu entries\n", entries_.size());
}
