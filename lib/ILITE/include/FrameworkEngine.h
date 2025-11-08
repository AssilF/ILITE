/**
 * @file FrameworkEngine.h
 * @brief ILITE Framework Core Engine
 *
 * Central coordinator for the ILITE framework. Manages:
 * - Module lifecycle and state
 * - Button event routing
 * - Display rendering coordination (top strip + dashboard)
 * - Framework menu system
 * - Communication between engines
 *
 * @author ILITE Team
 * @date 2025
 * @version 2.0.0
 */

#pragma once
#include <Arduino.h>
#include <vector>
#include "ButtonEventEngine.h"
#include "ILITEModule.h"
#include "DisplayCanvas.h"
#include "AudioRegistry.h"
#include "MenuRegistry.h"
#include "ModuleMenu.h"

// Forward declarations
class InputManager;
class Audio;

/**
 * @brief Framework system status
 */
enum class FrameworkStatus {
    IDLE,               ///< No activity, ready
    SCANNING,           ///< Searching for devices
    PAIRING,            ///< Pairing in progress
    PAIRED,             ///< Connected to device
    ERROR_COMM,         ///< Communication error
    ERROR_MODULE        ///< Module error
};

/**
 * @brief Encoder function definition for top strip
 */
struct EncoderFunction {
    const char* label;              ///< Short label (max 4 chars, e.g., "F1", "ARM")
    const char* fullName;           ///< Full name (e.g., "Arm Motors")
    std::function<void()> callback; ///< Function to execute
    bool isToggle;                  ///< If true, shows on/off state
    bool* toggleState;              ///< Pointer to toggle state variable (if isToggle)

    EncoderFunction()
        : label(nullptr), fullName(nullptr), callback(nullptr),
          isToggle(false), toggleState(nullptr) {}
};

/**
 * @brief Top strip button type (for navigation)
 */
enum class StripButton {
    MENU,       ///< Always present, opens framework menu
    FUNCTION_1, ///< Optional module function 1
    FUNCTION_2  ///< Optional module function 2
};

/**
 * @brief Framework Engine - Central coordinator for ILITE
 */
class FrameworkEngine {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to framework engine
     */
    static FrameworkEngine& getInstance();

    /**
     * @brief Initialize framework engine
     */
    void begin();

    /**
     * @brief Update framework state (call at 50Hz)
     *
     * Updates button states, processes events, routes to modules
     */
    void update();

    /**
     * @brief Render complete frame (top strip + dashboard)
     *
     * @param canvas Display canvas to render to
     */
    void render(DisplayCanvas& canvas);

    // ========================================================================
    // Module Management
    // ========================================================================

    /**
     * @brief Load and activate a module
     *
     * @param module Pointer to module to load (nullptr to unload current)
     */
    void loadModule(ILITEModule* module);

    /**
     * @brief Get currently loaded module
     *
     * @return Pointer to active module, or nullptr if none loaded
     */
    ILITEModule* getCurrentModule() const { return currentModule_; }

    /**
     * @brief Check if a module is currently loaded
     *
     * @return true if module loaded
     */
    bool hasModuleLoaded() const { return currentModule_ != nullptr; }

    // ========================================================================
    // Pairing & Status
    // ========================================================================

    /**
     * @brief Set pairing status
     *
     * @param paired true if device is paired
     */
    void setPaired(bool paired);

    /**
     * @brief Check if currently paired
     *
     * @return true if paired
     */
    bool isPaired() const { return isPaired_; }

    /**
     * @brief Set framework status
     *
     * @param status Current status
     */
    void setStatus(FrameworkStatus status) { status_ = status; }

    /**
     * @brief Get framework status
     *
     * @return Current status
     */
    FrameworkStatus getStatus() const { return status_; }

    // ========================================================================
    // Button Event System
    // ========================================================================

    /**
     * @brief Get button event engine
     *
     * @return Reference to button event engine
     */
    ButtonEventEngine& getButtonEngine() { return buttonEngine_; }

    /**
     * @brief Set button callback for default functions
     *
     * Used when no module is loaded or module doesn't define button functions
     *
     * @param buttonIndex Button index (0-2 for buttons 1-3)
     * @param callback Function to call for button events
     */
    void setDefaultButtonCallback(uint8_t buttonIndex, ButtonCallback callback);

    // ========================================================================
    // Top Strip & Encoder Functions
    // ========================================================================

    /**
     * @brief Set encoder function (module-defined)
     *
     * @param slot Function slot (0=F1, 1=F2)
     * @param func Encoder function definition
     */
    void setEncoderFunction(uint8_t slot, const EncoderFunction& func);

    /**
     * @brief Clear encoder function
     *
     * @param slot Function slot (0=F1, 1=F2)
     */
    void clearEncoderFunction(uint8_t slot);

    /**
     * @brief Get currently selected strip button
     *
     * @return Selected button
     */
    StripButton getSelectedStripButton() const { return selectedStripButton_; }

    /**
     * @brief Handle encoder rotation (for strip navigation)
     *
     * @param delta Rotation delta (+1 = clockwise, -1 = counterclockwise)
     */
    void onEncoderRotate(int delta);

    /**
     * @brief Handle encoder button press
     *
     * Menu behavior:
     * - When NOT in dashboard (no module loaded or not paired):
     *   Encoder press ALWAYS opens the menu
     * - When in dashboard (module loaded and paired):
     *   Encoder press activates the currently selected strip button
     *   (Menu, F1, or F2)
     */
    void onEncoderPress();

    // ========================================================================
    // Menu System
    // ========================================================================

    /**
     * @brief Check if framework menu is open
     *
     * @return true if menu is open
     */
    bool isMenuOpen() const { return menuOpen_; }

    /**
     * @brief Open framework menu
     */
    void openMenu();

    /**
     * @brief Close framework menu
     */
    void closeMenu();

    /**
     * @brief Toggle menu open/closed
     */
    void toggleMenu();

    // ========================================================================
    // Battery & Status
    // ========================================================================

    /**
     * @brief Set battery percentage
     *
     * @param percent Battery level 0-100
     */
    void setBatteryPercent(uint8_t percent) { batteryPercent_ = percent; }

    /**
     * @brief Get battery percentage
     *
     * @return Battery level 0-100
     */
    uint8_t getBatteryPercent() const { return batteryPercent_; }

private:
    FrameworkEngine(); // Singleton pattern
    FrameworkEngine(const FrameworkEngine&) = delete;
    FrameworkEngine& operator=(const FrameworkEngine&) = delete;

    /**
     * @brief Render top strip (10px height)
     *
     * @param canvas Display canvas
     */
    void renderTopStrip(DisplayCanvas& canvas);

    /**
     * @brief Render dashboard area (54px height)
     *
     * @param canvas Display canvas
     */
    void renderDashboard(DisplayCanvas& canvas);

    /**
     * @brief Render framework menu when open
     */
    void renderMenu(DisplayCanvas& canvas);

    /**
     * @brief Register default menu entries (Terminal, Devices, Settings)
     */
    void registerDefaultMenuEntries();

    /**
     * @brief Handle menu navigation (encoder)
     */
    void navigateMenu(int delta);

    /**
     * @brief Activate currently selected menu entry
     */
    void activateMenuSelection();

    /**
     * @brief Return to previous menu level or close menu
     */
    void menuGoBack();

    /**
     * @brief Get current menu identifier (nullptr = root)
     */
    MenuID currentMenuId() const;

    /**
     * @brief Render generic dashboard (no module loaded)
     *
     * @param canvas Display canvas
     */
    void renderGenericDashboard(DisplayCanvas& canvas);

    /**
     * @brief Route button event to appropriate handler
     *
     * @param buttonIndex Button index (0-4)
     * @param event Button event
     */
    void routeButtonEvent(uint8_t buttonIndex, ButtonEvent event);

    /**
     * @brief Update strip button count and availability
     */
    void updateStripButtons();

    /**
     * @brief Register module menu entries into framework MenuRegistry
     */
    void registerModuleMenuEntries();

    /**
     * @brief Clear all module menu entries from MenuRegistry
     */
    void clearModuleMenuEntries();

    /**
     * @brief Recursively convert and register ModuleMenuItem tree
     */
    void convertModuleMenuItems(const ModuleMenuItem& parent, MenuID parentMenuId);

    // Core engines
    ButtonEventEngine buttonEngine_;

    // Module state
    ILITEModule* currentModule_;
    bool isPaired_;
    FrameworkStatus status_;

    // Module menu integration
    ModuleMenuItem moduleMenuRoot_;
    ModuleMenuBuilder moduleMenuBuilder_;
    std::vector<std::string> moduleMenuIds_;

    // Top strip state
    StripButton selectedStripButton_;
    EncoderFunction encoderFunctions_[2];  // F1, F2
    bool hasEncoderFunction_[2];
    uint8_t stripButtonCount_;

    // Menu state
    bool menuOpen_;
    std::vector<MenuID> menuStack_;
    int menuSelection_;
    int menuScrollOffset_;

    // Status
    uint8_t batteryPercent_;
    uint32_t statusAnimFrame_;

    // Default button callbacks (when no module loaded)
    ButtonCallback defaultButtonCallbacks_[3];

    // Constants
    static constexpr uint8_t STRIP_HEIGHT = 10;
    static constexpr uint8_t DASHBOARD_Y = STRIP_HEIGHT;
    static constexpr uint8_t DASHBOARD_HEIGHT = 54;
};
