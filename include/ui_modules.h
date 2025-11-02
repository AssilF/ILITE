/**
 * @file ui_modules.h
 * @brief Module system for platform-specific control interfaces
 *
 * ILITE uses a modular architecture where each controlled device (robot, drone, etc.)
 * can have its own specialized interface. This file defines the core abstractions
 * for adding new platforms to the controller.
 *
 * @author ILITE Team
 * @date 2025
 */

#pragma once
#include <Arduino.h>

/**
 * @brief Platform/device type identifier
 *
 * Each type of controlled device has a unique PeerKind. This is used for:
 * - Auto-detection during pairing (via name matching)
 * - Selecting appropriate control/display logic
 * - Configuring context-specific menu options
 */
enum class PeerKind : uint8_t {
  None,       ///< Uninitialized or invalid
  Generic,    ///< Unknown/universal device (displays raw data)
  Drongaze,   ///< DroneGaze quadcopter flight controller
  Bulky,      ///< Bulky utility carrier robot
  Thegill     ///< The'gill differential drive robot
};

struct ModuleState;

/// Maximum number of programmable function buttons per module
constexpr size_t kMaxFunctionSlots = 3;

/**
 * @brief Function action definition for programmable buttons
 *
 * Each module can define a set of actions that can be assigned to
 * the three function buttons (button1-3). Examples: toggle lights,
 * switch modes, open settings screens, etc.
 */
struct FunctionActionOption {
  const char* name;         ///< Full action name (e.g., "Toggle Arm")
  const char* shortLabel;   ///< Short label for UI (e.g., "ARM", max 4 chars)
  void (*invoke)(ModuleState& state, size_t slot);  ///< Action callback function
};

/**
 * @brief Module descriptor - defines platform-specific behavior
 *
 * Each controlled platform (Drongaze, Bulky, Thegill, etc.) has a descriptor
 * that provides all platform-specific implementations. This allows ILITE to
 * hot-swap between different robot types without code changes.
 *
 * To add a new platform:
 * 1. Add a PeerKind enum value
 * 2. Create platform-specific header/implementation files
 * 3. Implement all descriptor callbacks
 * 4. Add to moduleStates array in main.cpp
 * 5. Add name detection in findModuleByName()
 */
struct ModuleDescriptor {
  PeerKind kind;                     ///< Platform type identifier
  const char* displayName;           ///< Name shown in UI (e.g., "DroneGaze")
  void (*drawDashboard)();           ///< Render main control dashboard
  void (*drawLayoutCard)(const ModuleState& state, int16_t x, int16_t y, bool focused); ///< Render home screen card
  void (*handleIncoming)(ModuleState& state, const uint8_t* data, size_t length); ///< Process telemetry packets
  void (*updateControl)();           ///< Read inputs and update command packet
  const uint8_t* (*preparePayload)(size_t& size); ///< Get command packet bytes for transmission
  const FunctionActionOption* functionOptions;   ///< Array of available function button actions
  size_t functionOptionCount;        ///< Number of elements in functionOptions array
};

/**
 * @brief Runtime state for one module instance
 *
 * Each module has persistent state including WiFi enable, assigned actions,
 * and function output states. Multiple instances of the same platform type
 * could theoretically be supported.
 */
struct ModuleState {
  const ModuleDescriptor* descriptor;               ///< Pointer to platform descriptor
  bool wifiEnabled;                                 ///< WiFi telemetry enable flag
  uint8_t assignedActions[kMaxFunctionSlots];       ///< Index into functionOptions for each button
  bool functionOutputs[kMaxFunctionSlots];          ///< Current output state of each function
};

/**
 * @brief Get total number of registered modules
 * @return Number of module instances
 */
size_t getModuleStateCount();

/**
 * @brief Get module state by index
 * @param index Module index (0 to getModuleStateCount()-1)
 * @return Pointer to module state, or nullptr if index invalid
 */
ModuleState* getModuleState(size_t index);

/**
 * @brief Find module by platform kind
 * @param kind Platform type to search for
 * @return Pointer to first matching module, or fallback to index 0 if not found
 */
ModuleState* findModuleByKind(PeerKind kind);

/**
 * @brief Find module by device name (auto-detection during pairing)
 *
 * Searches device name for platform-specific keywords:
 * - "gill" → Thegill
 * - "bulky" → Bulky
 * - "drongaze" or "drone" → Drongaze
 * - Default → Generic
 *
 * @param name Device custom ID string
 * @return Pointer to appropriate module state
 */
ModuleState* findModuleByName(const char* name);

/**
 * @brief Get currently active module
 * @return Pointer to active module state
 */
ModuleState* getActiveModule();

/**
 * @brief Switch to different module (hot-swap control layout)
 * @param state Module to activate
 */
void setActiveModule(ModuleState* state);

/**
 * @brief Get function action assigned to button slot
 * @param state Module state
 * @param slot Button slot (0-2)
 * @return Pointer to action option, or nullptr if invalid
 */
const FunctionActionOption* getAssignedAction(const ModuleState& state, size_t slot);

/**
 * @brief Cycle function button assignment
 *
 * Allows user to reconfigure which action is bound to each button.
 *
 * @param state Module state
 * @param slot Button slot (0-2)
 * @param delta Direction: +1 next, -1 previous
 */
void cycleAssignedAction(ModuleState& state, size_t slot, int delta);

/**
 * @brief Set function output state
 * @param state Module state
 * @param slot Button slot (0-2)
 * @param active Output state (true = on/active)
 */
void setFunctionOutput(ModuleState& state, size_t slot, bool active);

/**
 * @brief Get function output state
 * @param state Module state
 * @param slot Button slot (0-2)
 * @return Current output state
 */
bool getFunctionOutput(const ModuleState& state, size_t slot);

/**
 * @brief Toggle WiFi telemetry enable for module
 *
 * Sends WifiControlCommand packet to paired device to enable/disable
 * telemetry transmission (battery saving feature).
 *
 * @param state Module state
 */
void toggleModuleWifi(ModuleState& state);
