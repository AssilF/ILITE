/**
 * @file IconLibrary.cpp
 * @brief Icon Library Implementation with Built-in Icons
 */

#include "IconLibrary.h"
#include <cstring>

// Static storage
std::vector<Icon> IconLibrary::icons_;

// ============================================================================
// Built-in Icon Data (8x8 monochrome bitmaps)
// ============================================================================

// Home icon (house)
static const uint8_t icon_home_data[] = {
    0b00010000,
    0b00111000,
    0b01111100,
    0b11111110,
    0b11111110,
    0b11101110,
    0b11000110,
    0b10000010
};

// Settings icon (gear)
static const uint8_t icon_settings_data[] = {
    0b00111100,
    0b01111110,
    0b11111111,
    0b11011011,
    0b11011011,
    0b11111111,
    0b01111110,
    0b00111100
};

// Info icon (i in circle)
static const uint8_t icon_info_data[] = {
    0b00111100,
    0b01000010,
    0b10011001,
    0b10111001,
    0b10111001,
    0b10011001,
    0b01000010,
    0b00111100
};

// Warning icon (triangle with !)
static const uint8_t icon_warning_data[] = {
    0b00010000,
    0b00111000,
    0b00101000,
    0b01000100,
    0b01000100,
    0b10000010,
    0b10010010,
    0b11111110
};

// Error icon (X in circle)
static const uint8_t icon_error_data[] = {
    0b00111100,
    0b01000010,
    0b10100101,
    0b11011011,
    0b11011011,
    0b10100101,
    0b01000010,
    0b00111100
};

// Battery full (100%)
static const uint8_t icon_battery_full_data[] = {
    0b00111110,
    0b01111111,
    0b11111111,
    0b11111111,
    0b11111111,
    0b11111111,
    0b01111110,
    0b00111100
};

// Battery medium (50%)
static const uint8_t icon_battery_med_data[] = {
    0b00111110,
    0b01111111,
    0b11111111,
    0b11111111,
    0b11000011,
    0b11000011,
    0b01111110,
    0b00111100
};

// Battery low (20%)
static const uint8_t icon_battery_low_data[] = {
    0b00111110,
    0b01111111,
    0b11000011,
    0b11000011,
    0b11000011,
    0b11000011,
    0b01111110,
    0b00111100
};

// Signal full (4 bars)
static const uint8_t icon_signal_full_data[] = {
    0b00010000,
    0b00110000,
    0b01110000,
    0b11110000,
    0b11110001,
    0b11110011,
    0b11110111,
    0b11111111
};

// Signal medium (3 bars)
static const uint8_t icon_signal_med_data[] = {
    0b00000000,
    0b00000000,
    0b01110000,
    0b11110000,
    0b11110001,
    0b11110011,
    0b11110111,
    0b11111111
};

// Signal low (2 bars)
static const uint8_t icon_signal_low_data[] = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b11110001,
    0b11110011,
    0b11110111,
    0b11111111
};

// Signal none (1 bar)
static const uint8_t icon_signal_none_data[] = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000111,
    0b11111111
};

// Joystick
static const uint8_t icon_joystick_data[] = {
    0b00011000,
    0b00011000,
    0b00011000,
    0b00111100,
    0b01111110,
    0b01111110,
    0b11111111,
    0b11111111
};

// Drone (quadcopter)
static const uint8_t icon_drone_data[] = {
    0b10000001,
    0b11000011,
    0b01111110,
    0b00111100,
    0b00111100,
    0b01111110,
    0b11000011,
    0b10000001
};

// Robot
static const uint8_t icon_robot_data[] = {
    0b00111100,
    0b01111110,
    0b11011011,
    0b11111111,
    0b01111110,
    0b01111110,
    0b01011010,
    0b11000011
};

// Car
static const uint8_t icon_car_data[] = {
    0b00111100,
    0b01111110,
    0b11111111,
    0b11111111,
    0b11111111,
    0b11111111,
    0b01100110,
    0b01100110
};

// Tuning (wrench)
static const uint8_t icon_tuning_data[] = {
    0b00011100,
    0b00111110,
    0b00011100,
    0b00011000,
    0b00011000,
    0b01111000,
    0b01111000,
    0b00110000
};

// Lock (locked/disarmed)
static const uint8_t icon_lock_data[] = {
    0b00111100,
    0b01000010,
    0b01000010,
    0b11111111,
    0b11111111,
    0b11011011,
    0b11011011,
    0b11111111
};

// Unlock (unlocked/armed)
static const uint8_t icon_unlock_data[] = {
    0b00111100,
    0b01000010,
    0b01000000,
    0b11111111,
    0b11111111,
    0b11011011,
    0b11011011,
    0b11111111
};

// Play
static const uint8_t icon_play_data[] = {
    0b00010000,
    0b00110000,
    0b01110000,
    0b11110000,
    0b11110000,
    0b01110000,
    0b00110000,
    0b00010000
};

// Pause
static const uint8_t icon_pause_data[] = {
    0b01100110,
    0b01100110,
    0b01100110,
    0b01100110,
    0b01100110,
    0b01100110,
    0b01100110,
    0b01100110
};

// Stop
static const uint8_t icon_stop_data[] = {
    0b11111111,
    0b11111111,
    0b11111111,
    0b11111111,
    0b11111111,
    0b11111111,
    0b11111111,
    0b11111111
};

// Arrow up
static const uint8_t icon_up_data[] = {
    0b00011000,
    0b00111100,
    0b01111110,
    0b11111111,
    0b00111100,
    0b00111100,
    0b00111100,
    0b00111100
};

// Arrow down
static const uint8_t icon_down_data[] = {
    0b00111100,
    0b00111100,
    0b00111100,
    0b00111100,
    0b11111111,
    0b01111110,
    0b00111100,
    0b00011000
};

// Arrow left
static const uint8_t icon_left_data[] = {
    0b00010000,
    0b00110000,
    0b01110000,
    0b11111111,
    0b11111111,
    0b01110000,
    0b00110000,
    0b00010000
};

// Arrow right
static const uint8_t icon_right_data[] = {
    0b00001000,
    0b00001100,
    0b00001110,
    0b11111111,
    0b11111111,
    0b00001110,
    0b00001100,
    0b00001000
};

// Check mark
static const uint8_t icon_check_data[] = {
    0b00000000,
    0b00000001,
    0b00000011,
    0b10000110,
    0b11001100,
    0b01111000,
    0b00110000,
    0b00000000
};

// X mark (cross)
static const uint8_t icon_cross_data[] = {
    0b10000001,
    0b11000011,
    0b01100110,
    0b00111100,
    0b00111100,
    0b01100110,
    0b11000011,
    0b10000001
};

// Menu (hamburger)
static const uint8_t icon_menu_data[] = {
    0b11111111,
    0b00000000,
    0b11111111,
    0b00000000,
    0b11111111,
    0b00000000,
    0b11111111,
    0b00000000
};

// Back (arrow left with bar)
static const uint8_t icon_back_data[] = {
    0b00010000,
    0b00110000,
    0b01110000,
    0b11111111,
    0b11111111,
    0b01110000,
    0b00110000,
    0b00010000
};

// ============================================================================
// Registration
// ============================================================================

void IconLibrary::registerIcon(const Icon& icon) {
    // Check for duplicate IDs
    for (const auto& existing : icons_) {
        if (strcmp(existing.id, icon.id) == 0) {
            Serial.printf("[IconLibrary] WARNING: Duplicate icon '%s' (ignoring)\n", icon.id);
            return;
        }
    }

    icons_.push_back(icon);
    Serial.printf("[IconLibrary] Registered icon: %s (%ux%u)\n",
                  icon.id, icon.width, icon.height);
}

// ============================================================================
// Queries
// ============================================================================

const Icon* IconLibrary::getIcon(IconID id) {
    if (id == nullptr) {
        return nullptr;
    }

    for (const auto& icon : icons_) {
        if (strcmp(icon.id, id) == 0) {
            return &icon;
        }
    }

    return nullptr;
}

bool IconLibrary::hasIcon(IconID id) {
    return getIcon(id) != nullptr;
}

std::vector<Icon>& IconLibrary::getIcons() {
    return icons_;
}

void IconLibrary::clear() {
    icons_.clear();
}

// ============================================================================
// Built-in Icon Initialization
// ============================================================================

void IconLibrary::initBuiltInIcons() {
    // Register all built-in 8x8 icons
    registerIcon({ICON_HOME, 8, 8, icon_home_data});
    registerIcon({ICON_SETTINGS, 8, 8, icon_settings_data});
    registerIcon({ICON_INFO, 8, 8, icon_info_data});
    registerIcon({ICON_WARNING, 8, 8, icon_warning_data});
    registerIcon({ICON_ERROR, 8, 8, icon_error_data});
    registerIcon({ICON_BATTERY_FULL, 8, 8, icon_battery_full_data});
    registerIcon({ICON_BATTERY_MED, 8, 8, icon_battery_med_data});
    registerIcon({ICON_BATTERY_LOW, 8, 8, icon_battery_low_data});
    registerIcon({ICON_SIGNAL_FULL, 8, 8, icon_signal_full_data});
    registerIcon({ICON_SIGNAL_MED, 8, 8, icon_signal_med_data});
    registerIcon({ICON_SIGNAL_LOW, 8, 8, icon_signal_low_data});
    registerIcon({ICON_SIGNAL_NONE, 8, 8, icon_signal_none_data});
    registerIcon({ICON_JOYSTICK, 8, 8, icon_joystick_data});
    registerIcon({ICON_DRONE, 8, 8, icon_drone_data});
    registerIcon({ICON_ROBOT, 8, 8, icon_robot_data});
    registerIcon({ICON_CAR, 8, 8, icon_car_data});
    registerIcon({ICON_TUNING, 8, 8, icon_tuning_data});
    registerIcon({ICON_LOCK, 8, 8, icon_lock_data});
    registerIcon({ICON_UNLOCK, 8, 8, icon_unlock_data});
    registerIcon({ICON_PLAY, 8, 8, icon_play_data});
    registerIcon({ICON_PAUSE, 8, 8, icon_pause_data});
    registerIcon({ICON_STOP, 8, 8, icon_stop_data});
    registerIcon({ICON_UP, 8, 8, icon_up_data});
    registerIcon({ICON_DOWN, 8, 8, icon_down_data});
    registerIcon({ICON_LEFT, 8, 8, icon_left_data});
    registerIcon({ICON_RIGHT, 8, 8, icon_right_data});
    registerIcon({ICON_CHECK, 8, 8, icon_check_data});
    registerIcon({ICON_CROSS, 8, 8, icon_cross_data});
    registerIcon({ICON_MENU, 8, 8, icon_menu_data});
    registerIcon({ICON_BACK, 8, 8, icon_back_data});

    Serial.printf("[IconLibrary] Initialized %zu built-in icons\n", icons_.size());
}
