/**
 * @file HomeScreen.h
 * @brief ILITE Home Screen - Unpaired State UI
 *
 * Displays controller status, battery, signal, registered modules,
 * and provides menu navigation to terminal and module browser.
 *
 * @author ILITE Framework
 * @date 2025
 */

#pragma once

#include <Arduino.h>

// Forward declarations
class DisplayCanvas;

/**
 * @brief Display mode enumeration
 */
enum class DisplayMode : uint8_t {
    HOME,              ///< Home screen (unpaired state)
    MENU,              ///< Main menu
    TERMINAL,          ///< Terminal view
    MODULE_BROWSER,    ///< Module card browser
    MODULE_DASHBOARD,  ///< Active module dashboard
    SETTINGS,          ///< Settings screen
    DEVICE_LIST,       ///< Device discovery list
    DEVICE_PROPERTIES  ///< Selected device properties
};

/**
 * @brief Home Screen Manager
 *
 * Handles the unpaired state UI showing controller status,
 * registered modules, and navigation to menus.
 */
class HomeScreen {
public:
    /**
     * @brief Initialize home screen
     */
    static void begin();

    /**
     * @brief Draw home screen
     * @param canvas Display canvas to draw on
     */
    static void draw(DisplayCanvas& canvas);

    /**
     * @brief Handle encoder button press (open menu)
     */
    static void onEncoderButton();

    /**
     * @brief Handle encoder rotation
     * @param delta Rotation delta (-1 or +1)
     */
    static void onEncoderRotate(int delta);

    /**
     * @brief Handle other button presses
     * @param button Button number (1-3)
     */
    static void onButton(uint8_t button);

    /**
     * @brief Get current display mode
     * @return Current display mode
     */
    static DisplayMode getCurrentMode();

    /**
     * @brief Set display mode
     * @param mode Mode to set
     */
    static void setMode(DisplayMode mode);

    /**
     * @brief Go back to previous screen
     */
    static void goBack();

private:
    static void drawHome(DisplayCanvas& canvas);
    static void drawMenu(DisplayCanvas& canvas);
    static void drawTerminal(DisplayCanvas& canvas);
    static void drawSettings(DisplayCanvas& canvas);
    static void drawDeviceList(DisplayCanvas& canvas);
    static void drawDeviceProperties(DisplayCanvas& canvas);

    static DisplayMode currentMode_;
    static DisplayMode previousMode_;
    static uint32_t lastUpdate_;
    static int deviceListIndex_;
    static int selectedDeviceIndex_;
    static int selectedButtonIndex_;
};
