/**
 * @file ControlBindingSystem.cpp
 * @brief Control Binding System Implementation
 */

#include "ControlBindingSystem.h"
#include "ScreenRegistry.h"
#include "input.h"  // Existing input pin definitions
#include <algorithm>

// Static storage
std::vector<ControlBinding> ControlBindingSystem::bindings_;
bool ControlBindingSystem::enabled_ = true;
ControlBindingSystem::ButtonState ControlBindingSystem::buttonStates_[7];
ControlBindingSystem::EncoderState ControlBindingSystem::encoderState_;
bool ControlBindingSystem::capturingModuleBindings_ = false;

// Constants
static constexpr uint32_t CLICK_MAX_DURATION = 300;    // ms
static constexpr uint32_t DOUBLE_CLICK_WINDOW = 300;   // ms
static constexpr uint32_t LONG_PRESS_DURATION = 2000;  // ms

// ============================================================================
// Initialization
// ============================================================================

void ControlBindingSystem::begin() {
    // Initialize button states
    for (int i = 0; i < 7; ++i) {
        buttonStates_[i] = {false, false, 0, 0, false, 0, 0};
    }

    // Initialize encoder state
    encoderState_.lastCount = encoderCount;  // Use global from input.h

    Serial.println("[ControlBindingSystem] Initialized");
}

// ============================================================================
// Registration
// ============================================================================

void ControlBindingSystem::registerBinding(const ControlBinding& binding) {
    ControlBinding stored = binding;
    stored.moduleOwned = capturingModuleBindings_;
    bindings_.push_back(stored);
    Serial.printf("[ControlBindingSystem] Registered binding for input %d, event %d (priority %d)\n",
                  binding.input, binding.event, binding.priority);
}

std::vector<ControlBinding>& ControlBindingSystem::getAllBindings() {
    return bindings_;
}

void ControlBindingSystem::clear() {
    bindings_.clear();
}

void ControlBindingSystem::clearModuleBindings() {
    bindings_.erase(
        std::remove_if(bindings_.begin(), bindings_.end(),
                       [](const ControlBinding& binding) {
                           return binding.moduleOwned;
                       }),
        bindings_.end());
}

void ControlBindingSystem::beginModuleCapture() {
    capturingModuleBindings_ = true;
}

void ControlBindingSystem::endModuleCapture() {
    capturingModuleBindings_ = false;
}

void ControlBindingSystem::setEnabled(bool enabled) {
    enabled_ = enabled;
}

bool ControlBindingSystem::isEnabled() {
    return enabled_;
}

// ============================================================================
// Update Loop
// ============================================================================

void ControlBindingSystem::update() {
    if (!enabled_) {
        return;
    }

    uint32_t now = millis();

    // Update all button states
    updateButton(INPUT_BUTTON1, digitalRead(button1) == LOW);
    updateButton(INPUT_BUTTON2, digitalRead(button2) == LOW);
    updateButton(INPUT_BUTTON3, digitalRead(button3) == LOW);
    updateButton(INPUT_JOYSTICK_A_BUTTON, digitalRead(joystickBtnA) == LOW);
    updateButton(INPUT_JOYSTICK_B_BUTTON, digitalRead(joystickBtnB) == LOW);
    updateButton(INPUT_ENCODER_BUTTON, digitalRead(encoderBtn) == LOW);

    // Update encoder
    updateEncoder(encoderCount);  // Use global from input.h
}

// ============================================================================
// Button State Management
// ============================================================================

void ControlBindingSystem::updateButton(ControlInput input, bool currentValue) {
    int stateIndex = getButtonStateIndex(input);
    if (stateIndex < 0) return;

    ButtonState& state = buttonStates_[stateIndex];
    uint32_t now = millis();

    state.previous = state.current;
    state.current = currentValue;

    // Press event (rising edge)
    if (state.current && !state.previous) {
        state.pressTime = now;
        state.holdFired = false;
        triggerBindings(input, EVENT_PRESS);

        // Check for double-click
        if (state.clickCount > 0 && (now - state.lastClickTime) < DOUBLE_CLICK_WINDOW) {
            state.clickCount = 0;
            triggerBindings(input, EVENT_DOUBLE_CLICK);
        }
    }

    // Release event (falling edge)
    if (!state.current && state.previous) {
        state.releaseTime = now;
        triggerBindings(input, EVENT_RELEASE);

        // Check for click (quick press + release)
        uint32_t pressDuration = state.releaseTime - state.pressTime;
        if (pressDuration < CLICK_MAX_DURATION) {
            triggerBindings(input, EVENT_CLICK);
            state.clickCount++;
            state.lastClickTime = now;
        } else {
            state.clickCount = 0;
        }
    }

    // Hold event (button held down)
    if (state.current && !state.holdFired) {
        // Check for custom hold duration bindings
        for (const auto& binding : bindings_) {
            if (binding.input == input && binding.event == EVENT_HOLD) {
                uint32_t holdDuration = binding.duration > 0 ? binding.duration : 1000;
                if ((now - state.pressTime) >= holdDuration) {
                    if (checkCondition(binding)) {
                        if (binding.action) {
                            binding.action();
                        }
                        state.holdFired = true;
                    }
                }
            }
        }

        // Check for long press (2+ seconds)
        if ((now - state.pressTime) >= LONG_PRESS_DURATION) {
            triggerBindings(input, EVENT_LONG_PRESS);
            state.holdFired = true;
        }
    }
}

// ============================================================================
// Encoder State Management
// ============================================================================

void ControlBindingSystem::updateEncoder(int currentCount) {
    int delta = currentCount - encoderState_.lastCount;

    if (delta != 0) {
        triggerBindings(INPUT_ENCODER_ROTATE, EVENT_PRESS, delta);
        encoderState_.lastCount = currentCount;

        // Also forward to ScreenRegistry if active
        if (ScreenRegistry::hasActiveScreen()) {
            ScreenRegistry::handleEncoderRotate(delta);
        }
    }
}

// ============================================================================
// Binding Execution
// ============================================================================

void ControlBindingSystem::triggerBindings(ControlInput input, ControlEvent event, int value) {
    // Collect matching bindings
    std::vector<const ControlBinding*> matchingBindings;

    for (const auto& binding : bindings_) {
        if (binding.input == input && binding.event == event) {
            if (checkCondition(binding)) {
                matchingBindings.push_back(&binding);
            }
        }
    }

    // Sort by priority (highest first)
    std::sort(matchingBindings.begin(), matchingBindings.end(),
              [](const ControlBinding* a, const ControlBinding* b) {
                  return a->priority > b->priority;
              });

    // Execute bindings
    for (const ControlBinding* binding : matchingBindings) {
        if (input == INPUT_ENCODER_ROTATE && binding->actionWithValue) {
            binding->actionWithValue(value);
        } else if (binding->action) {
            binding->action();
        }
    }
}

bool ControlBindingSystem::checkCondition(const ControlBinding& binding) {
    // Check screen condition
    if (binding.screenId != nullptr) {
        const Screen* activeScreen = ScreenRegistry::getActiveScreen();
        if (activeScreen == nullptr || strcmp(activeScreen->id, binding.screenId) != 0) {
            return false;
        }
    }

    // Check custom condition
    if (binding.condition) {
        return binding.condition();
    }

    return true;
}

// ============================================================================
// Helpers
// ============================================================================

int ControlBindingSystem::getButtonStateIndex(ControlInput input) {
    switch (input) {
        case INPUT_BUTTON1: return 0;
        case INPUT_BUTTON2: return 1;
        case INPUT_BUTTON3: return 2;
        case INPUT_JOYSTICK_A_BUTTON: return 3;
        case INPUT_JOYSTICK_B_BUTTON: return 4;
        case INPUT_ENCODER_BUTTON: return 5;
        default: return -1;
    }
}
