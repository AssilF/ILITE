/**
 * @file ButtonEventEngine.cpp
 * @brief Button Event State Machine Implementation
 *
 * @author ILITE Team
 * @date 2025
 */

#include "ButtonEventEngine.h"
#include "input.h"

// ============================================================================
// ButtonEventTracker Implementation
// ============================================================================

ButtonEventTracker::ButtonEventTracker(uint8_t pin, uint32_t longPressMs)
    : pin_(pin)
    , longPressThreshold_(longPressMs)
    , state_(ButtonState::IDLE)
    , previousState_(ButtonState::IDLE)
    , stateStartTime_(0)
    , lastReading_(false)
    , longPressFired_(false)
    , callback_(nullptr)
    , lastDebounceTime_(0)
    , lastRawReading_(false)
{
}

ButtonEvent ButtonEventTracker::update() {
    // Read raw pin state (buttons are active LOW with INPUT_PULLUP)
    bool rawReading = (digitalRead(pin_) == LOW);

    // TEMP DEBUG: Show encoder button state
    if (pin_ == 18) {  // encoderBtn
        static bool lastDebugReading = false;
        if (rawReading != lastDebugReading) {
            Serial.printf("[ButtonEventTracker] Pin 18 raw state: %s\n", rawReading ? "LOW (PRESSED)" : "HIGH (NOT PRESSED)");
            lastDebugReading = rawReading;
        }
    }

    // Debouncing logic
    if (rawReading != lastRawReading_) {
        lastDebounceTime_ = millis();
    }
    lastRawReading_ = rawReading;

    // Only update if stable for DEBOUNCE_MS
    if ((millis() - lastDebounceTime_) > DEBOUNCE_MS) {
        lastReading_ = rawReading;
    }

    // State machine
    previousState_ = state_;
    ButtonEvent event = ButtonEvent::NONE;
    uint32_t timeInState = millis() - stateStartTime_;

    switch (state_) {
        case ButtonState::IDLE:
            if (lastReading_) {
                // Button pressed
                state_ = ButtonState::PRESSED;
                stateStartTime_ = millis();
                longPressFired_ = false;
                event = ButtonEvent::PRESSED;
            }
            break;

        case ButtonState::PRESSED:
            if (!lastReading_) {
                // Quick release (short press)
                state_ = ButtonState::IDLE;
                stateStartTime_ = millis();
                event = ButtonEvent::RELEASED;
            } else if (timeInState >= longPressThreshold_) {
                // Long press threshold reached
                state_ = ButtonState::LONG_HELD;
                stateStartTime_ = millis();
                longPressFired_ = true;
                event = ButtonEvent::LONG_PRESS;
            } else {
                // Still held
                event = ButtonEvent::HELD;
            }
            break;

        case ButtonState::HELD:
            // This state currently unused, but reserved for future
            if (!lastReading_) {
                state_ = ButtonState::IDLE;
                stateStartTime_ = millis();
                event = ButtonEvent::RELEASED;
            } else {
                event = ButtonEvent::HELD;
            }
            break;

        case ButtonState::LONG_HELD:
            if (!lastReading_) {
                // Released after long press
                state_ = ButtonState::IDLE;
                stateStartTime_ = millis();
                event = ButtonEvent::RELEASED;
            } else {
                // Continue holding
                event = ButtonEvent::HELD;
            }
            break;
    }

    // Trigger callback if event occurred
    if (event != ButtonEvent::NONE && event != ButtonEvent::HELD) {
        triggerCallback(event);
    }

    return event;
}

bool ButtonEventTracker::isPressed() const {
    return lastReading_;
}

uint32_t ButtonEventTracker::getTimeInState() const {
    return millis() - stateStartTime_;
}

void ButtonEventTracker::triggerCallback(ButtonEvent event) {
    // Debug output
    const char* eventName = "UNKNOWN";
    switch (event) {
        case ButtonEvent::PRESSED: eventName = "PRESSED"; break;
        case ButtonEvent::HELD: eventName = "HELD"; break;
        case ButtonEvent::RELEASED: eventName = "RELEASED"; break;
        case ButtonEvent::LONG_PRESS: eventName = "LONG_PRESS"; break;
        default: break;
    }
    Serial.printf("[Button] Pin %d: %s\n", pin_, eventName);

    if (callback_) {
        callback_(event);
    }
}

void ButtonEventTracker::reset() {
    state_ = ButtonState::IDLE;
    previousState_ = ButtonState::IDLE;
    stateStartTime_ = millis();
    longPressFired_ = false;
}

// ============================================================================
// ButtonEventEngine Implementation
// ============================================================================

ButtonEventEngine::ButtonEventEngine(uint32_t longPressMs)
    : buttons_{
        ButtonEventTracker(button1, longPressMs),
        ButtonEventTracker(button2, longPressMs),
        ButtonEventTracker(button3, longPressMs),
        ButtonEventTracker(joystickBtnA, longPressMs),
        ButtonEventTracker(encoderBtn, longPressMs)
    }
    , longPressThreshold_(longPressMs)
{
    for (int i = 0; i < BUTTON_COUNT; i++) {
        lastEvents_[i] = ButtonEvent::NONE;
    }
}

void ButtonEventEngine::begin() {
    // Pins already initialized by initInput() in input.cpp
    // Just reset all state machines
    for (int i = 0; i < BUTTON_COUNT; i++) {
        buttons_[i].reset();
    }
}

void ButtonEventEngine::update() {
    // Update all button state machines
    for (int i = 0; i < BUTTON_COUNT; i++) {
        lastEvents_[i] = buttons_[i].update();
    }
}

ButtonEventTracker& ButtonEventEngine::getButton(uint8_t index) {
    if (index >= BUTTON_COUNT) {
        index = 0; // Safety fallback
    }
    return buttons_[index];
}

void ButtonEventEngine::setButtonCallback(uint8_t index, ButtonCallback callback) {
    if (index < BUTTON_COUNT) {
        buttons_[index].setCallback(callback);
    }
}

ButtonEvent ButtonEventEngine::getLastEvent(uint8_t index) const {
    if (index >= BUTTON_COUNT) {
        return ButtonEvent::NONE;
    }
    return lastEvents_[index];
}
