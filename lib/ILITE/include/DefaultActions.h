/**
 * @file DefaultActions.h
 * @brief Default button actions for ILITE Framework
 *
 * Provides default functions for physical buttons when no module is loaded.
 * These functions allow basic framework interaction without a module.
 *
 * @author ILITE Team
 * @date 2025
 * @version 2.0.0
 */

#pragma once
#include <Arduino.h>

// Forward declarations
class DisplayCanvas;

/**
 * @brief Default button actions namespace
 */
namespace DefaultActions {

    /**
     * @brief Show terminal/log screen
     *
     * Displays recent connection log events and system messages.
     * Useful for debugging and monitoring system state.
     */
    void showTerminal();

    /**
     * @brief Open devices screen
     *
     * Shows discovered ESP-NOW devices and allows manual pairing.
     * Displays device names, signal strength, and connection status.
     */
    void openDevices();

    /**
     * @brief Open settings screen
     *
     * Framework-level settings including:
     * - Display brightness
     * - Audio volume
     * - WiFi/OTA settings
     * - System information
     */
    void openSettings();

    /**
     * @brief Check if terminal screen is active
     * @return true if terminal is showing
     */
    bool isTerminalActive();

    /**
     * @brief Check if devices screen is active
     * @return true if devices screen is showing
     */
    bool isDevicesActive();

    /**
     * @brief Check if settings screen is active
     * @return true if settings screen is showing
     */
    bool isSettingsActive();

    /**
     * @brief Close any active default screen
     *
     * Returns to home screen or module dashboard
     */
    void closeActiveScreen();

    /**
     * @brief Check if any default screen is active
     * @return true if any default screen is showing
     */
    bool hasActiveScreen();

    /**
     * @brief Get the title of the currently active screen
     * @return Screen title string, or nullptr if no screen is active
     */
    const char* getActiveScreenTitle();

    /**
     * @brief Render the currently active default screen
     *
     * Automatically calls the appropriate render function based on active screen
     *
     * @param canvas Display canvas to render to
     */
    void renderActiveScreen(DisplayCanvas& canvas);

    /**
     * @brief Render terminal screen
     *
     * Internal function called by display engine when terminal is active
     *
     * @param canvas Display canvas to render to
     */
    void renderTerminalScreen(DisplayCanvas& canvas);

    /**
     * @brief Render devices screen
     *
     * Internal function called by display engine when devices screen is active
     *
     * @param canvas Display canvas to render to
     */
    void renderDevicesScreen(DisplayCanvas& canvas);

    /**
     * @brief Render settings screen
     *
     * Internal function called by display engine when settings screen is active
     *
     * @param canvas Display canvas to render to
     */
    void renderSettingsScreen(DisplayCanvas& canvas);

    /**
     * @brief Handle encoder rotation for active screen
     *
     * Called when encoder is rotated and a default screen is active
     *
     * @param delta Rotation delta (+1 = clockwise, -1 = counterclockwise)
     */
    void handleEncoderRotate(int delta);

    /**
     * @brief Handle encoder button press for active screen
     *
     * Called when encoder button is pressed and a default screen is active
     */
    void handleEncoderPress();

} // namespace DefaultActions
