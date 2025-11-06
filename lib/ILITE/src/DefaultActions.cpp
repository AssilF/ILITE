/**
 * @file DefaultActions.cpp
 * @brief Default button actions implementation
 *
 * @author ILITE Team
 * @date 2025
 */

#include "DefaultActions.h"
#include "DisplayCanvas.h"
#include "ModuleRegistry.h"
#include "AudioRegistry.h"
#include <Arduino.h>

namespace DefaultActions {

// Internal state
enum class ActiveScreen {
    NONE,
    TERMINAL,
    DEVICES,
    SETTINGS
};

static ActiveScreen currentScreen = ActiveScreen::NONE;
static int scrollOffset = 0;
static int selectedItem = 0;

// ============================================================================
// Public API - Screen Management
// ============================================================================

void showTerminal() {
    if (currentScreen != ActiveScreen::TERMINAL) {
        currentScreen = ActiveScreen::TERMINAL;
        scrollOffset = 0;
        selectedItem = 0;
        AudioRegistry::play("menu_select");
        Serial.println("[DefaultActions] Terminal screen opened");
    }
}

void openDevices() {
    if (currentScreen != ActiveScreen::DEVICES) {
        currentScreen = ActiveScreen::DEVICES;
        scrollOffset = 0;
        selectedItem = 0;
        AudioRegistry::play("menu_select");
        Serial.println("[DefaultActions] Devices screen opened");
    }
}

void openSettings() {
    if (currentScreen != ActiveScreen::SETTINGS) {
        currentScreen = ActiveScreen::SETTINGS;
        scrollOffset = 0;
        selectedItem = 0;
        AudioRegistry::play("menu_select");
        Serial.println("[DefaultActions] Settings screen opened");
    }
}

bool isTerminalActive() {
    return currentScreen == ActiveScreen::TERMINAL;
}

bool isDevicesActive() {
    return currentScreen == ActiveScreen::DEVICES;
}

bool isSettingsActive() {
    return currentScreen == ActiveScreen::SETTINGS;
}

void closeActiveScreen() {
    if (currentScreen != ActiveScreen::NONE) {
        currentScreen = ActiveScreen::NONE;
        scrollOffset = 0;
        selectedItem = 0;
        AudioRegistry::play("menu_select");
        Serial.println("[DefaultActions] Screen closed, returning to dashboard");
    }
}

// ============================================================================
// Rendering
// ============================================================================

void renderTerminalScreen(DisplayCanvas& canvas) {
    canvas.setFont(DisplayCanvas::SMALL);

    // Title
    canvas.drawText(2, 10, "=== TERMINAL ===");
    canvas.drawLine(0, 12, 127, 12);

    // Terminal content (mock data for now)
    // TODO: Integrate with actual Logger system
    const char* logLines[] = {
        "[INFO] System started",
        "[INFO] Modules loaded: 4",
        "[INFO] ESP-NOW initialized",
        "[INFO] Scanning for devices...",
        "[WARN] No devices found",
        "[INFO] Display ready",
        "[INFO] Audio ready"
    };
    const int lineCount = sizeof(logLines) / sizeof(logLines[0]);

    int y = 22;
    int displayLines = 5;
    for (int i = scrollOffset; i < scrollOffset + displayLines && i < lineCount; i++) {
        if (i == selectedItem) {
            canvas.drawRect(0, y - 8, 128, 9,1);
            canvas.setDrawColor(0);
        }
        canvas.drawText(2, y, logLines[i]);
        if (i == selectedItem) {
            canvas.setDrawColor(1);
        }
        y += 10;
    }

    // Footer
    canvas.setFont(DisplayCanvas::TINY);
    canvas.drawText(2, 62, "Encoder:Scroll | Btn:Close");
}

void renderDevicesScreen(DisplayCanvas& canvas) {
    canvas.setFont(DisplayCanvas::SMALL);

    // Title
    canvas.drawText(2, 10, "=== DEVICES ===");
    canvas.drawLine(0, 12, 127, 12);

    // Device list (mock data for now)
    // TODO: Integrate with actual ESP-NOW discovery
    const char* devices[] = {
        "DroneGaze-01",
        "TheGill-Alpha",
        "Bulky-Test",
        "No devices found..."
    };
    const int deviceCount = 4;

    int y = 22;
    canvas.setFont(DisplayCanvas::NORMAL);
    for (int i = 0; i < deviceCount && i < 4; i++) {
        if (i == selectedItem) {
            canvas.drawRect(0, y - 8, 128, 9,1);
            canvas.setDrawColor(0);
        }

        // Device icon
        canvas.drawText(2, y, ">");
        canvas.drawText(12, y, devices[i]);

        if (i == selectedItem) {
            canvas.setDrawColor(1);
        }
        y += 11;
    }

    // Footer
    canvas.setFont(DisplayCanvas::TINY);
    canvas.drawText(2, 62, "Encoder:Select | Btn:Pair");
}

void renderSettingsScreen(DisplayCanvas& canvas) {
    canvas.setFont(DisplayCanvas::SMALL);

    // Title
    canvas.drawText(2, 10, "=== SETTINGS ===");
    canvas.drawLine(0, 12, 127, 12);

    // Settings menu items
    const char* settings[] = {
        "Display Brightness",
        "Audio Volume",
        "WiFi Settings",
        "System Info",
        "Back"
    };
    const int settingCount = 5;

    int y = 22;
    canvas.setFont(DisplayCanvas::NORMAL);
    for (int i = 0; i < settingCount && i < 4; i++) {
        if (i == selectedItem) {
            canvas.drawRect(0, y - 8, 128, 9,1);
            canvas.setDrawColor(0);
        }

        canvas.drawText(2, y, ">");
        canvas.drawText(12, y, settings[i]);

        if (i == selectedItem) {
            canvas.setDrawColor(1);
        }
        y += 11;
    }

    // Footer
    canvas.setFont(DisplayCanvas::TINY);
    canvas.drawText(2, 62, "Encoder:Select | Btn:Enter");
}

// ============================================================================
// Input Handling
// ============================================================================

void handleEncoderRotate(int delta) {
    // Only handle rotation if a screen is active
    if (currentScreen == ActiveScreen::NONE) {
        return;
    }

    // Update selected item based on active screen
    int maxItems = 0;
    switch (currentScreen) {
        case ActiveScreen::TERMINAL:
            maxItems = 7; // Mock log line count
            break;
        case ActiveScreen::DEVICES:
            maxItems = 4; // Mock device count
            break;
        case ActiveScreen::SETTINGS:
            maxItems = 5; // Settings menu count
            break;
        default:
            return;
    }

    selectedItem += delta;

    // Wrap around
    if (selectedItem < 0) {
        selectedItem = maxItems - 1;
    } else if (selectedItem >= maxItems) {
        selectedItem = 0;
    }

    // Update scroll offset if needed (for terminal)
    if (currentScreen == ActiveScreen::TERMINAL) {
        const int displayLines = 5;
        if (selectedItem < scrollOffset) {
            scrollOffset = selectedItem;
        } else if (selectedItem >= scrollOffset + displayLines) {
            scrollOffset = selectedItem - displayLines + 1;
        }
    }
}

void handleEncoderPress() {
    // Handle encoder press based on active screen
    switch (currentScreen) {
        case ActiveScreen::TERMINAL:
            // Close terminal on encoder press
            closeActiveScreen();
            break;

        case ActiveScreen::DEVICES:
            // TODO: Pair with selected device
            Serial.printf("[DefaultActions] Pairing with device %d\n", selectedItem);
            AudioRegistry::play("paired");
            closeActiveScreen();
            break;

        case ActiveScreen::SETTINGS:
            // Handle settings selection
            if (selectedItem == 4) {
                // "Back" option
                closeActiveScreen();
            } else {
                // TODO: Open specific setting screen
                Serial.printf("[DefaultActions] Opening setting %d\n", selectedItem);
            }
            break;

        default:
            break;
    }
}

} // namespace DefaultActions
