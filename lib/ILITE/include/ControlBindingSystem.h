/**
 * @file ControlBindingSystem.h
 * @brief Control Binding System - Flexible Input Event Handling
 *
 * Allows users to bind input events (press, hold, double-click) to actions
 * with conditions and screen-specific contexts.
 *
 * @example
 * ```cpp
 * REGISTER_CONTROL_BINDING(
 *     .input = INPUT_BUTTON1,
 *     .event = EVENT_PRESS,
 *     .action = []() { toggleArm(); }
 * );
 * ```
 *
 * @author ILITE Framework
 * @version 1.0.0
 */

#ifndef ILITE_CONTROL_BINDING_SYSTEM_H
#define ILITE_CONTROL_BINDING_SYSTEM_H

#include <Arduino.h>
#include <vector>
#include <functional>

/**
 * @brief Input event types
 */
enum ControlEvent {
    EVENT_PRESS,         ///< Button pressed (rising edge)
    EVENT_RELEASE,       ///< Button released (falling edge)
    EVENT_CLICK,         ///< Quick press + release (< 300ms)
    EVENT_HOLD,          ///< Held for specified duration
    EVENT_DOUBLE_CLICK,  ///< Two clicks within 300ms
    EVENT_LONG_PRESS     ///< Held for 2+ seconds
};

/**
 * @brief Input identifiers
 */
enum ControlInput {
    INPUT_BUTTON1,
    INPUT_BUTTON2,
    INPUT_BUTTON3,
    INPUT_JOYSTICK_A_BUTTON,
    INPUT_JOYSTICK_B_BUTTON,
    INPUT_ENCODER_BUTTON,
    INPUT_ENCODER_ROTATE
};

/**
 * @brief Control binding definition
 */
struct ControlBinding {
    ControlInput input;                     ///< Input to bind
    ControlEvent event;                     ///< Event type
    std::function<void()> action;           ///< Action callback
    std::function<void(int)> actionWithValue; ///< Action with value (for encoder)
    std::function<bool()> condition;        ///< Condition (optional)
    const char* screenId;                   ///< Screen-specific binding (optional)
    uint32_t duration;                      ///< Duration for HOLD event (ms)
    int priority;                           ///< Priority (higher = executed first)
    bool moduleOwned = false;               ///< true if binding belongs to currently active module
};

/**
 * @brief Control Binding System - Manages input event bindings
 */
class ControlBindingSystem {
public:
    /**
     * @brief Initialize the control binding system
     */
    static void begin();

    /**
     * @brief Register a control binding
     * @param binding Binding definition
     */
    static void registerBinding(const ControlBinding& binding);

    /**
     * @brief Update control bindings (call in loop)
     *
     * Polls inputs, detects events, and triggers bound actions.
     */
    static void update();

    /**
     * @brief Get all registered bindings
     * @return Vector of bindings
     */
    static std::vector<ControlBinding>& getAllBindings();

    /**
     * @brief Clear all bindings
     */
    static void clear();

    /**
     * @brief Clear only module-owned bindings
     */
    static void clearModuleBindings();

    /**
     * @brief Enter module binding capture mode
     *
     * While active, all new bindings are marked as module-owned.
     */
    static void beginModuleCapture();

    /**
     * @brief Exit module binding capture mode
     */
    static void endModuleCapture();

    /**
     * @brief Enable/disable the binding system
     * @param enabled true to enable, false to disable
     */
    static void setEnabled(bool enabled);

    /**
     * @brief Check if binding system is enabled
     * @return true if enabled
     */
    static bool isEnabled();

private:
    /**
     * @brief Button state tracking
     */
    struct ButtonState {
        bool current;
        bool previous;
        uint32_t pressTime;
        uint32_t releaseTime;
        bool holdFired;
        int clickCount;
        uint32_t lastClickTime;
    };

    /**
     * @brief Encoder state tracking
     */
    struct EncoderState {
        int lastCount;
    };

    static std::vector<ControlBinding> bindings_;
    static bool enabled_;
    static bool capturingModuleBindings_;

    static ButtonState buttonStates_[7];  // 6 buttons + encoder button
    static EncoderState encoderState_;

    /**
     * @brief Update button state and detect events
     * @param input Input identifier
     * @param currentValue Current button value (true = pressed)
     */
    static void updateButton(ControlInput input, bool currentValue);

    /**
     * @brief Update encoder and detect rotation
     * @param currentCount Current encoder count
     */
    static void updateEncoder(int currentCount);

    /**
     * @brief Trigger bindings for an event
     * @param input Input that triggered event
     * @param event Event type
     * @param value Optional value (for encoder)
     */
    static void triggerBindings(ControlInput input, ControlEvent event, int value = 0);

    /**
     * @brief Get button state index
     * @param input Input identifier
     * @return State index, or -1 if not a button
     */
    static int getButtonStateIndex(ControlInput input);

    /**
     * @brief Check if binding condition is met
     * @param binding Binding to check
     * @return true if binding should execute
     */
    static bool checkCondition(const ControlBinding& binding);
};

// ============================================================================
// Registration Macros
// ============================================================================

/**
 * @brief Macro for registering simple control bindings
 *
 * @example
 * ```cpp
 * REGISTER_CONTROL_BINDING_SIMPLE(
 *     INPUT_BUTTON1,
 *     EVENT_PRESS,
 *     []() { toggleArm(); }
 * );
 * ```
 */
#define REGISTER_CONTROL_BINDING_SIMPLE(input, event, action) \
    namespace { \
        struct ControlBindingRegistrar_##input##_##event { \
            ControlBindingRegistrar_##input##_##event() { \
                ControlBindingSystem::registerBinding({ \
                    input, \
                    event, \
                    action, \
                    nullptr, \
                    nullptr, \
                    nullptr, \
                    0, \
                    0 \
                }); \
            } \
        }; \
        static ControlBindingRegistrar_##input##_##event g_controlBindingRegistrar_##input##_##event; \
    }

/**
 * @brief Macro for registering control bindings with full configuration
 *
 * Uses designated initializers for cleaner syntax.
 *
 * @example
 * ```cpp
 * REGISTER_CONTROL_BINDING({
 *     .input = INPUT_BUTTON1,
 *     .event = EVENT_PRESS,
 *     .condition = []() { return isPaired(); },
 *     .action = []() { toggleArm(); }
 * });
 * ```
 */
#define REGISTER_CONTROL_BINDING(...) \
    namespace { \
        static int g_bindingCounter_##__LINE__ = []() { \
            ControlBinding binding = {__VA_ARGS__}; \
            ControlBindingSystem::registerBinding(binding); \
            return 0; \
        }(); \
    }

#endif // ILITE_CONTROL_BINDING_SYSTEM_H
