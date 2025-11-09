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
#include "connection_log.h"
#include "espnow_discovery.h"
#include <Arduino.h>

extern EspNowDiscovery discovery;

namespace DefaultActions {

// Internal state
enum class ActiveScreen {
    NONE,
    TERMINAL,
    DEVICES,
    DEVICE_INFO,
    SETTINGS
};

static ActiveScreen currentScreen = ActiveScreen::NONE;
static int scrollOffset = 0;
static int selectedItem = 0;
static int deviceDetailIndex = -1;
static int deviceDetailAction = 0;

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
        deviceDetailIndex = -1;
        deviceDetailAction = 0;
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
        deviceDetailIndex = -1;
        deviceDetailAction = 0;
        AudioRegistry::play("menu_select");
        Serial.println("[DefaultActions] Screen closed, returning to dashboard");
    }
}

bool hasActiveScreen() {
    return currentScreen != ActiveScreen::NONE;
}

const char* getActiveScreenTitle() {
    switch (currentScreen) {
        case ActiveScreen::TERMINAL:
            return "TERMINAL";
        case ActiveScreen::DEVICES:
            return "DEVICES";
        case ActiveScreen::SETTINGS:
            return "SETTINGS";
        default:
            return nullptr;
    }
}

void renderActiveScreen(DisplayCanvas& canvas) {
    switch (currentScreen) {
        case ActiveScreen::TERMINAL:
            renderTerminalScreen(canvas);
            break;
        case ActiveScreen::DEVICES:
            renderDevicesScreen(canvas);
            break;
        case ActiveScreen::DEVICE_INFO:
            renderDeviceInfoScreen(canvas);
            break;
        case ActiveScreen::SETTINGS:
            renderSettingsScreen(canvas);
            break;
        default:
            break;
    }
}

// ============================================================================
// Rendering
// ============================================================================

void renderTerminalScreen(DisplayCanvas& canvas) {
    const int startY = 14;  // Below top strip
    const int lineHeight = 7;  // Tiny font line height
    const int maxCharsPerLine = 30;  // TINY font is 4px wide, 128/4 = 32, use 30 for margin
    const int maxY = 64;  // Screen height

    // Get connection log entries
    const int logCount = connectionLogGetCount();

    canvas.setFont(DisplayCanvas::TINY);

    if (logCount == 0) {
        canvas.drawText(2, startY, "No log entries yet");
        return;
    }

    int y = startY;
    int linesDisplayed = 0;
    int linesToSkip = scrollOffset;  // Number of wrapped lines to skip

    // Show newest entries first (reverse order)
    for (int i = logCount - 1; i >= 0 && y < maxY; i--) {
        const char* entry = connectionLogGetEntry(i);
        if (!entry || entry[0] == '\0') continue;

        // Wrap this entry into multiple lines
        int entryLen = strlen(entry);
        int startPos = 0;

        while (startPos < entryLen && y < maxY) {
            // Find end of this line (word wrap)
            int lineLen = maxCharsPerLine;
            if (startPos + lineLen > entryLen) {
                lineLen = entryLen - startPos;
            } else {
                // Try to break at a space for word wrapping
                int lastSpace = -1;
                for (int j = 0; j < lineLen && (startPos + j) < entryLen; j++) {
                    if (entry[startPos + j] == ' ') {
                        lastSpace = j;
                    }
                }
                if (lastSpace > lineLen / 2) {  // Only break at space if it's not too early
                    lineLen = lastSpace + 1;  // Include the space
                }
            }

            // Skip lines for scrolling
            if (linesToSkip > 0) {
                linesToSkip--;
            } else {
                // Draw this line
                char lineBuf[32];
                int copyLen = (lineLen < 31) ? lineLen : 31;
                strncpy(lineBuf, entry + startPos, copyLen);
                lineBuf[copyLen] = '\0';

                canvas.drawText(2, y, lineBuf);
                y += lineHeight;
                linesDisplayed++;
            }

            startPos += lineLen;
        }
    }
}

void renderDevicesScreen(DisplayCanvas& canvas) {
    const int startY = 14;  // Below top strip
    int y = startY;

    // Get discovered devices
    const int deviceCount = discovery.getPeerCount();
    const int pairedIndex = discovery.getPairedPeerIndex();

    canvas.setFont(DisplayCanvas::SMALL);

    if (deviceCount == 0) {
        // No devices found
        canvas.drawText(2, y, "Scanning for devices...");
        y += 12;
        canvas.setFont(DisplayCanvas::TINY);
        canvas.drawText(2, y, "Make sure remote device");
        y += 9;
        canvas.drawText(2, y, "is powered on and in");
        y += 9;
        canvas.drawText(2, y, "discovery mode.");
    } else {
        // Display discovered devices
        for (int i = 0; i < deviceCount && y < 60; i++) {
            const char* deviceName = discovery.getPeerName(i);
            const uint8_t* deviceMac = discovery.getPeer(i);
            bool isPaired = (i == pairedIndex);
            bool isSelected = (i == selectedItem);

            if (isSelected) {
                canvas.drawRect(0, y - 8, 128, 10, 1);
                canvas.setDrawColor(0);
            }

            // Status indicator
            canvas.setFont(DisplayCanvas::NORMAL);
            if (isPaired) {
                canvas.drawText(2, y, "\x10");  // Paired indicator
            } else {
                canvas.drawText(2, y, ">");
            }

            // Device name
            canvas.drawText(12, y, deviceName ? deviceName : "Unknown");

            if (isSelected) {
                canvas.setDrawColor(1);
            }

            y += 10;
        }

        // Device count indicator
        canvas.setFont(DisplayCanvas::TINY);
        y = 60;
        canvas.drawTextF(2, y, "%d device%s found", deviceCount, (deviceCount == 1) ? "" : "s");
    }
}

void renderDeviceInfoScreen(DisplayCanvas& canvas) {
    canvas.setFont(DisplayCanvas::SMALL);
    canvas.drawText(2, 12, "Device Details");

    const int deviceCount = discovery.getPeerCount();
    if (deviceDetailIndex < 0 || deviceDetailIndex >= deviceCount) {
        canvas.setFont(DisplayCanvas::TINY);
        canvas.drawText(2, 28, "Device unavailable.");
        canvas.drawText(2, 38, "It may have timed out.");
        deviceDetailAction = 1;  // Default to Cancel
    } else {
        const Identity* identity = discovery.getPeerIdentity(deviceDetailIndex);
        const uint8_t* mac = discovery.getPeer(deviceDetailIndex);
        const int pairedIndex = discovery.getPairedPeerIndex();
        const bool isActive = (deviceDetailIndex == pairedIndex);

        canvas.setFont(DisplayCanvas::TINY);
        const char* name = (identity && identity->customId[0]) ? identity->customId : "Unknown";
        const char* deviceType = (identity && identity->deviceType[0]) ? identity->deviceType : "Unknown";
        const char* platform = (identity && identity->platform[0]) ? identity->platform : "Unknown";

        canvas.drawTextF(2, 24, "Name: %s", name);
        canvas.drawTextF(2, 34, "Type: %s", deviceType);
        canvas.drawTextF(2, 44, "Platform: %s", platform);

        char macStr[24] = "Unavailable";
        if (mac != nullptr) {
            EspNowDiscovery::macToString(mac, macStr, sizeof(macStr));
        }
        canvas.drawTextF(2, 54, "MAC: %s", macStr);
        canvas.drawTextF(2, 62, "Status: %s", isActive ? "Paired" : "Discovered");
    }

    const int buttonWidth = 56;
    const int buttonHeight = 10;
    const int buttonBaseline = 60;

    auto drawButton = [&](int index, int x, const char* label, bool enabled) {
        bool selected = (deviceDetailAction == index);
        if (selected) {
            canvas.drawRect(x, buttonBaseline - 8, buttonWidth, buttonHeight, 1);
            canvas.setDrawColor(0);
        }

        canvas.setFont(enabled ? DisplayCanvas::SMALL : DisplayCanvas::TINY);
        canvas.drawText(x + 6, buttonBaseline, label);

        if (selected) {
            canvas.setDrawColor(1);
        }
    };

    const bool isCurrentSelectionValid =
        (deviceDetailIndex >= 0 && deviceDetailIndex < discovery.getPeerCount());
    const bool isPairedDevice =
        (isCurrentSelectionValid && deviceDetailIndex == discovery.getPairedPeerIndex());

    drawButton(0, 4, isPairedDevice ? "ACTIVE" : "PAIR", !isPairedDevice && isCurrentSelectionValid);
    drawButton(1, 68, "CANCEL", true);
}

void renderSettingsScreen(DisplayCanvas& canvas) {
    const int startY = 14;  // Below top strip
    int y = startY;

    // Settings menu items
    const char* settings[] = {
        "Display Brightness",
        "Audio Volume",
        "WiFi Settings",
        "System Info",
        "About ILITE"
    };
    const int settingCount = 5;

    canvas.setFont(DisplayCanvas::SMALL);
    for (int i = 0; i < settingCount && y < 60; i++) {
        if (i == selectedItem) {
            canvas.drawRect(0, y - 8, 128, 10, 1);
            canvas.setDrawColor(0);
        }

        canvas.drawText(2, y, ">");
        canvas.drawText(12, y, settings[i]);

        if (i == selectedItem) {
            canvas.setDrawColor(1);
        }
        y += 10;
    }
}

// ============================================================================
// Input Handling
// ============================================================================

void handleEncoderRotate(int delta) {
    // Only handle rotation if a screen is active
    if (currentScreen == ActiveScreen::NONE) {
        return;
    }

    // For terminal, scroll through wrapped lines
    if (currentScreen == ActiveScreen::TERMINAL) {
        scrollOffset += delta;

        // Allow generous scrolling (assume average 2 wrapped lines per log entry)
        const int logCount = connectionLogGetCount();
        const int maxScroll = logCount * 3;  // Allow scrolling through ~3x wrapped lines

        if (scrollOffset < 0) {
            scrollOffset = 0;
        } else if (scrollOffset > maxScroll) {
            scrollOffset = maxScroll;
        }

        AudioRegistry::play("menu_select");
        return;
    }

    // Update selected item based on active screen
    int maxItems = 0;
    switch (currentScreen) {
        case ActiveScreen::DEVICES:
            maxItems = discovery.getPeerCount();
            if (maxItems == 0) maxItems = 1;  // At least 1 for "no devices"
            break;
        case ActiveScreen::DEVICE_INFO: {
            if (discovery.getPeerCount() == 0) {
                currentScreen = ActiveScreen::DEVICES;
                deviceDetailIndex = -1;
                deviceDetailAction = 0;
                return;
            }
            deviceDetailAction += delta;
            if (deviceDetailAction < 0) deviceDetailAction = 0;
            if (deviceDetailAction > 1) deviceDetailAction = 1;
            AudioRegistry::play("menu_select");
            return;
        }
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

    // Update scroll offset if needed
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
            {
                const int deviceCount = discovery.getPeerCount();
                if (deviceCount == 0) {
                    AudioRegistry::play("error");
                    return;
                }
                if (selectedItem >= deviceCount) {
                    selectedItem = deviceCount - 1;
                }
                deviceDetailIndex = selectedItem;
                deviceDetailAction = (deviceDetailIndex == discovery.getPairedPeerIndex()) ? 1 : 0;
                currentScreen = ActiveScreen::DEVICE_INFO;
                AudioRegistry::play("menu_select");
            }
            break;

        case ActiveScreen::DEVICE_INFO:
            if (deviceDetailAction == 0) {
                const int deviceCount = discovery.getPeerCount();
                if (deviceDetailIndex < 0 || deviceDetailIndex >= deviceCount) {
                    AudioRegistry::play("error");
                    currentScreen = ActiveScreen::DEVICES;
                    break;
                }

                const bool alreadyPaired = (deviceDetailIndex == discovery.getPairedPeerIndex());
                const uint8_t* mac = discovery.getPeer(deviceDetailIndex);
                if (!alreadyPaired && mac != nullptr && discovery.beginPairingWith(mac)) {
                    Serial.printf("[DefaultActions] Pairing with device %d\n", deviceDetailIndex);
                    AudioRegistry::play("paired");
                } else if (alreadyPaired) {
                    AudioRegistry::play("menu_select");
                } else {
                    AudioRegistry::play("error");
                }
                selectedItem = deviceDetailIndex;
                currentScreen = ActiveScreen::DEVICES;
                deviceDetailIndex = -1;
                deviceDetailAction = 0;
            } else {
                AudioRegistry::play("menu_back");
                currentScreen = ActiveScreen::DEVICES;
                deviceDetailIndex = -1;
                deviceDetailAction = 0;
            }
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
