/**
 * @file ButtonEventEngine.h
 * @brief Button Event State Machine for ILITE Framework
 *
 * Provides comprehensive button event detection including:
 * - Press, Hold, Release, Long Press events
 * - Configurable timing thresholds
 * - Module function mapping
 *
 * @author ILITE Team
 * @date 2025
 * @version 2.0.0
 */

#pragma once
#include <Arduino.h>
#include <functional>

/**
 * @brief Button event types
 */
enum class ButtonEvent {
    NONE,           ///< No event
    PRESSED,        ///< Button just pressed (rising edge)
    HELD,           ///< Button is being held
    RELEASED,       ///< Button just released (falling edge)
    LONG_PRESS      ///< Button held for long press duration
};

/**
 * @brief Button state for state machine
 */
enum class ButtonState {
    IDLE,           ///< Not pressed
    PRESSED,        ///< Just pressed, waiting
    HELD,           ///< Being held
    LONG_HELD       ///< Held past long press threshold
};

/**
 * @brief Button function callback type
 *
 * @param event The button event that triggered this callback
 */
using ButtonCallback = std::function<void(ButtonEvent event)>;

/**
 * @brief Single button event tracker with state machine
 */
class ButtonEventTracker {
public:
    /**
     * @brief Constructor
     *
     * @param pin GPIO pin number
     * @param longPressMs Long press threshold in milliseconds (default: 800ms)
     */
    ButtonEventTracker(uint8_t pin, uint32_t longPressMs = 800);

    /**
     * @brief Update button state machine (call frequently, e.g., 50Hz)
     *
     * @return Current event detected this frame
     */
    ButtonEvent update();

    /**
     * @brief Check if button is currently pressed
     *
     * @return true if pressed (debounced)
     */
    bool isPressed() const;

    /**
     * @brief Get current button state
     *
     * @return Current state in state machine
     */
    ButtonState getState() const { return state_; }

    /**
     * @brief Get time button has been in current state (ms)
     *
     * @return Milliseconds in current state
     */
    uint32_t getTimeInState() const;

    /**
     * @brief Set callback for button events
     *
     * @param callback Function to call when events occur
     */
    void setCallback(ButtonCallback callback) { callback_ = callback; }

    /**
     * @brief Execute callback if set
     *
     * @param event Event to pass to callback
     */
    void triggerCallback(ButtonEvent event);

    /**
     * @brief Reset state machine to IDLE
     */
    void reset();

private:
    uint8_t pin_;                       ///< GPIO pin
    uint32_t longPressThreshold_;       ///< Long press time in ms
    ButtonState state_;                 ///< Current state
    ButtonState previousState_;         ///< Previous state for edge detection
    uint32_t stateStartTime_;           ///< When current state started
    bool lastReading_;                  ///< Last debounced reading
    bool longPressFired_;               ///< Whether long press event has fired
    ButtonCallback callback_;           ///< User callback function

    // Debouncing
    static constexpr uint32_t DEBOUNCE_MS = 50;
    uint32_t lastDebounceTime_;
    bool lastRawReading_;
};

/**
 * @brief Button Event Engine - manages all button state machines
 */
class ButtonEventEngine {
public:
    /**
     * @brief Constructor
     *
     * @param longPressMs Long press threshold (default: 800ms)
     */
    ButtonEventEngine(uint32_t longPressMs = 800);

    /**
     * @brief Initialize all buttons
     */
    void begin();

    /**
     * @brief Update all button state machines (call at 50Hz+)
     */
    void update();

    /**
     * @brief Get button tracker by index
     *
     * @param index Button index (0=button1, 1=button2, 2=button3, 3=joystickBtnA, 4=encoderBtn)
     * @return Reference to button tracker
     */
    ButtonEventTracker& getButton(uint8_t index);

    /**
     * @brief Set callback for specific button
     *
     * @param index Button index (0-4)
     * @param callback Function to call for button events
     */
    void setButtonCallback(uint8_t index, ButtonCallback callback);

    /**
     * @brief Get last event for specific button
     *
     * @param index Button index (0-4)
     * @return Last event detected this frame
     */
    ButtonEvent getLastEvent(uint8_t index) const;

    // Button indices
    static constexpr uint8_t BUTTON_1 = 0;
    static constexpr uint8_t BUTTON_2 = 1;
    static constexpr uint8_t BUTTON_3 = 2;
    static constexpr uint8_t JOYSTICK_BTN = 3;
    static constexpr uint8_t ENCODER_BTN = 4;
    static constexpr uint8_t BUTTON_COUNT = 5;

private:
    ButtonEventTracker buttons_[BUTTON_COUNT];
    ButtonEvent lastEvents_[BUTTON_COUNT];
    uint32_t longPressThreshold_;
};
