/**
 * @file HomeScreen.cpp
 * @brief ILITE Home Screen Implementation
 */

#include "HomeScreen.h"
#include "DisplayCanvas.h"
#include "ModuleBrowser.h"
#include "ModuleRegistry.h"
#include "ILITE.h"
#include "espnow_discovery.h"
#include "display.h"  // For drawConnectionLog
#include <Arduino.h>

// Static member initialization
DisplayMode HomeScreen::currentMode_ = DisplayMode::HOME;
DisplayMode HomeScreen::previousMode_ = DisplayMode::HOME;
uint32_t HomeScreen::lastUpdate_ = 0;
int HomeScreen::deviceListIndex_ = 0;
int HomeScreen::selectedDeviceIndex_ = -1;

// Menu system
static int menuIndex = 0;
static const char* menuItems[] = {"Terminal", "Device list", "Modules", "Settings"};
static const int menuItemCount = 4;

// Terminal log buffer
static String terminalLogs[8];
static int terminalLogCount = 0;

// ============================================================================
// Public Methods
// ============================================================================

void HomeScreen::begin() {
    currentMode_ = DisplayMode::HOME;
    previousMode_ = DisplayMode::HOME;
    lastUpdate_ = millis();
    menuIndex = 0;
    deviceListIndex_ = 0;
    selectedDeviceIndex_ = -1;

    // Initialize terminal with some default logs
    terminalLogs[0] = "ILITE v2.0";
    terminalLogs[1] = "Framework ready";
    terminalLogs[2] = "Searching for devices...";
    terminalLogCount = 3;

    Serial.println("[HomeScreen] Initialized");
}

void HomeScreen::draw(DisplayCanvas& canvas) {
    canvas.clear();

    switch (currentMode_) {
        case DisplayMode::HOME:
            drawHome(canvas);
            break;

        case DisplayMode::MENU:
            drawMenu(canvas);
            break;

        case DisplayMode::TERMINAL:
            drawTerminal(canvas);
            break;

        case DisplayMode::MODULE_BROWSER:
            ModuleBrowser::draw(canvas);
            break;

        case DisplayMode::SETTINGS:
            drawSettings(canvas);
            break;

        case DisplayMode::DEVICE_LIST:
            drawDeviceList(canvas);
            break;

        case DisplayMode::DEVICE_PROPERTIES:
            drawDeviceProperties(canvas);
            break;

        default:
            drawHome(canvas);
            break;
    }

    canvas.sendBuffer();
}

void HomeScreen::onEncoderButton() {
    switch (currentMode_) {
        case DisplayMode::HOME:
            // Open main menu
            previousMode_ = DisplayMode::HOME;
            currentMode_ = DisplayMode::MENU;
            menuIndex = 0;
            break;

        case DisplayMode::MENU:
            // Select menu item
            previousMode_ = DisplayMode::MENU;
            if (menuIndex == 0) {
                currentMode_ = DisplayMode::TERMINAL;
            } else if (menuIndex == 1) {
                deviceListIndex_ = 0;
                currentMode_ = DisplayMode::DEVICE_LIST;
            } else if (menuIndex == 2) {
                currentMode_ = DisplayMode::MODULE_BROWSER;
            } else if (menuIndex == 3) {
                currentMode_ = DisplayMode::SETTINGS;
            }
            break;

        case DisplayMode::MODULE_BROWSER:
            ModuleBrowser::onEncoderButton();
            break;

        case DisplayMode::DEVICE_LIST:
            // Select device from list
            selectedDeviceIndex_ = deviceListIndex_;
            previousMode_ = DisplayMode::DEVICE_LIST;
            currentMode_ = DisplayMode::DEVICE_PROPERTIES;
            break;

        case DisplayMode::DEVICE_PROPERTIES:
            // Pair with selected device
            {
                auto& discovery = ILITEFramework::getInstance().getDiscovery();
                if (selectedDeviceIndex_ >= 0 && selectedDeviceIndex_ < discovery.getPeerCount()) {
                    const uint8_t* mac = discovery.getPeer(selectedDeviceIndex_);
                    if (mac != nullptr) {
                        discovery.beginPairingWith(mac);
                        Serial.println("[HomeScreen] Initiated pairing with selected device");
                        currentMode_ = DisplayMode::HOME;
                    }
                }
            }
            break;

        default:
            break;
    }
}

void HomeScreen::onEncoderRotate(int delta) {
    switch (currentMode_) {
        case DisplayMode::MENU:
            // Navigate menu
            menuIndex += delta;
            if (menuIndex < 0) menuIndex = 0;
            if (menuIndex >= menuItemCount) menuIndex = menuItemCount - 1;
            break;

        case DisplayMode::MODULE_BROWSER:
            ModuleBrowser::onEncoderRotate(delta);
            break;

        case DisplayMode::DEVICE_LIST:
            {
                // Navigate device list
                auto& discovery = ILITEFramework::getInstance().getDiscovery();
                int peerCount = discovery.getPeerCount();
                deviceListIndex_ += delta;
                if (deviceListIndex_ < 0) deviceListIndex_ = 0;
                if (deviceListIndex_ >= peerCount) deviceListIndex_ = peerCount - 1;
            }
            break;

        case DisplayMode::TERMINAL:
            // Scroll connection log
            extern int logScrollOffset;
            logScrollOffset += delta;
            // Bounds checking is done in drawConnectionLog()
            break;

        default:
            break;
    }
}

void HomeScreen::onButton(uint8_t button) {
    if (button == 3) {  // Back button
        goBack();
    }
}

DisplayMode HomeScreen::getCurrentMode() {
    return currentMode_;
}

void HomeScreen::setMode(DisplayMode mode) {
    previousMode_ = currentMode_;
    currentMode_ = mode;
}

void HomeScreen::goBack() {
    switch (currentMode_) {
        case DisplayMode::MENU:
        case DisplayMode::TERMINAL:
        case DisplayMode::SETTINGS:
            currentMode_ = DisplayMode::HOME;
            break;

        case DisplayMode::MODULE_BROWSER:
        case DisplayMode::DEVICE_LIST:
            currentMode_ = DisplayMode::MENU;
            break;

        case DisplayMode::MODULE_DASHBOARD:
            currentMode_ = DisplayMode::MODULE_BROWSER;
            break;

        case DisplayMode::DEVICE_PROPERTIES:
            currentMode_ = DisplayMode::DEVICE_LIST;
            break;

        default:
            currentMode_ = DisplayMode::HOME;
            break;
    }
}

// ============================================================================
// Private Drawing Methods
// ============================================================================

void HomeScreen::drawHome(DisplayCanvas& canvas) {
    // Title
    canvas.setFont(DisplayCanvas::NORMAL);
    canvas.drawTextCentered(8, "ILITE v2.0");

    // Status line
    canvas.setFont(DisplayCanvas::SMALL);
    canvas.drawTextCentered(22, "Controller Ready");

    // Module count
    int moduleCount = ModuleRegistry::getModuleCount();
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d modules loaded", moduleCount);
    canvas.drawTextCentered(32, buffer);

    // Pairing status - check discovery system
    auto& discovery = ILITEFramework::getInstance().getDiscovery();
    int deviceCount = discovery.getPeerCount();

    if (discovery.isPaired()) {
        canvas.drawTextCentered(42, "Device paired!");
    } else if (deviceCount > 0) {
        snprintf(buffer, sizeof(buffer), "Found %d device%s", deviceCount, deviceCount == 1 ? "" : "s");
        canvas.drawTextCentered(42, buffer);
    } else {
        canvas.drawTextCentered(42, "Searching...");
    }

    // Bottom bar with indicators
    canvas.drawHLine(0, 52, 128);

    // Device count indicator
    canvas.setFont(DisplayCanvas::TINY);
    snprintf(buffer, sizeof(buffer), "DEV:%d", deviceCount);
    canvas.drawText(2, 60, buffer);

    // Paired indicator
    if (discovery.isPaired()) {
        canvas.drawText(50, 60, "PAIRED");
    } else {
        canvas.drawText(50, 60, "UNPAIRED");
    }

    // Help text
    canvas.drawText(100, 60, "MENU");
}

void HomeScreen::drawMenu(DisplayCanvas& canvas) {
    // Title bar
    canvas.setFont(DisplayCanvas::NORMAL);
    canvas.drawText(2, 8, "Main Menu");
    canvas.drawHLine(0, 12, 128);

    // Menu items with scrolling (max 4 visible items)
    canvas.setFont(DisplayCanvas::SMALL);
    const int maxVisibleItems = 4;
    int startIdx = menuIndex;
    if (startIdx > menuItemCount - maxVisibleItems && menuItemCount > maxVisibleItems) {
        startIdx = menuItemCount - maxVisibleItems;
    }
    if (startIdx < 0) startIdx = 0;

    for (int i = 0; i < maxVisibleItems && (startIdx + i) < menuItemCount; i++) {
        int itemIdx = startIdx + i;
        int y = 24 + (i * 10);

        if (itemIdx == menuIndex) {
            // Selected item - draw selection box
            canvas.drawRect(4, y - 8, 120, 10, true);  // Filled rectangle
            canvas.setDrawColor(0);  // Invert color for text
            canvas.drawText(8, y, menuItems[itemIdx]);
            canvas.setDrawColor(1);  // Reset color
        } else {
            // Normal item
            canvas.drawText(8, y, menuItems[itemIdx]);
        }
    }

    // Bottom help
    canvas.drawHLine(0, 52, 128);
    canvas.setFont(DisplayCanvas::TINY);
    canvas.drawText(2, 60, "NAV");
    canvas.drawText(40, 60, "SEL");
    canvas.drawText(100, 60, "BACK");
}

void HomeScreen::drawTerminal(DisplayCanvas& canvas) {
    // Call the original connection log display which shows ESP-NOW TX/RX activity
    // This uses the global oled object directly
    drawConnectionLog();
}

void HomeScreen::drawSettings(DisplayCanvas& canvas) {
    // Title bar
    canvas.setFont(DisplayCanvas::SMALL);
    canvas.drawText(2, 8, "Settings");
    canvas.drawHLine(0, 10, 128);

    // Settings display
    canvas.setFont(DisplayCanvas::TINY);

    char buffer[32];

    // Free heap
    snprintf(buffer, sizeof(buffer), "Free Heap: %u", ESP.getFreeHeap());
    canvas.drawText(2, 20, buffer);

    // Uptime
    unsigned long uptime = millis() / 1000;
    snprintf(buffer, sizeof(buffer), "Uptime: %lus", uptime);
    canvas.drawText(2, 28, buffer);

    // Module count
    int moduleCount = ModuleRegistry::getModuleCount();
    snprintf(buffer, sizeof(buffer), "Modules: %d", moduleCount);
    canvas.drawText(2, 36, buffer);

    // Control loop frequency
    canvas.drawText(2, 44, "Ctrl: 50Hz");

    // Display frequency
    canvas.drawText(2, 52, "Disp: 10Hz");

    // Bottom help
    canvas.drawHLine(0, 56, 128);
    canvas.drawText(100, 63, "BACK");
}

void HomeScreen::drawDeviceList(DisplayCanvas& canvas) {
    // Title bar
    canvas.setFont(DisplayCanvas::SMALL);
    canvas.drawText(2, 8, "Device List");
    canvas.drawHLine(0, 10, 128);

    // Get discovered devices
    auto& discovery = ILITEFramework::getInstance().getDiscovery();
    int deviceCount = discovery.getPeerCount();

    if (deviceCount == 0) {
        canvas.setFont(DisplayCanvas::SMALL);
        canvas.drawTextCentered(32, "No devices found");
        canvas.setFont(DisplayCanvas::TINY);
        canvas.drawTextCentered(42, "Searching...");
    } else {
        // Display devices (up to 5 visible)
        canvas.setFont(DisplayCanvas::TINY);
        int startIdx = deviceListIndex_;
        if (startIdx > deviceCount - 5 && deviceCount > 5) {
            startIdx = deviceCount - 5;
        }
        if (startIdx < 0) startIdx = 0;

        for (int i = 0; i < 5 && (startIdx + i) < deviceCount; i++) {
            int deviceIdx = startIdx + i;
            int y = 18 + (i * 9);

            const char* deviceName = discovery.getPeerName(deviceIdx);
            bool selected = (deviceIdx == deviceListIndex_);

            if (selected) {
                // Highlight selected device
                canvas.drawRect(2, y - 7, 124, 9, true);
                canvas.setDrawColor(0);  // Invert for text
            }

            // Draw device name (truncate if needed)
            char nameBuffer[22];
            strncpy(nameBuffer, deviceName != nullptr ? deviceName : "Unknown", 21);
            nameBuffer[21] = '\0';
            canvas.drawText(4, y, nameBuffer);

            if (selected) {
                canvas.setDrawColor(1);  // Reset
            }
        }
    }

    // Bottom help
    canvas.drawHLine(0, 56, 128);
    canvas.setFont(DisplayCanvas::TINY);
    canvas.drawText(2, 63, "NAV");
    canvas.drawText(50, 63, "SELECT");
    canvas.drawText(100, 63, "BACK");
}

void HomeScreen::drawDeviceProperties(DisplayCanvas& canvas) {
    // Title bar
    canvas.setFont(DisplayCanvas::SMALL);
    canvas.drawText(2, 8, "Device Info");
    canvas.drawHLine(0, 10, 128);

    // Get device info
    auto& discovery = ILITEFramework::getInstance().getDiscovery();

    if (selectedDeviceIndex_ < 0 || selectedDeviceIndex_ >= discovery.getPeerCount()) {
        canvas.setFont(DisplayCanvas::SMALL);
        canvas.drawTextCentered(32, "Invalid device");
        return;
    }

    const Identity* identity = nullptr;
    const uint8_t* mac = discovery.getPeer(selectedDeviceIndex_);

    // Get identity from peer entry
    int peerIdx = discovery.findPeerIndex(mac);
    if (peerIdx >= 0) {
        // We need to access the peer entry - for now, show basic info
        canvas.setFont(DisplayCanvas::TINY);

        char buffer[32];

        // Device name
        const char* name = discovery.getPeerName(selectedDeviceIndex_);
        canvas.drawText(2, 20, "Name:");
        canvas.drawText(2, 28, name != nullptr ? name : "Unknown");

        // MAC address
        canvas.drawText(2, 38, "MAC:");
        char macStr[18];
        if (mac != nullptr) {
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            canvas.drawText(2, 46, macStr);
        }
    }

    // Bottom help
    canvas.drawHLine(0, 56, 128);
    canvas.setFont(DisplayCanvas::TINY);
    canvas.drawText(2, 63, "PAIR");
    canvas.drawText(100, 63, "BACK");
}
