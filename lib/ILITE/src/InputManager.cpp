/**
 * @file InputManager.cpp
 * @brief Input Management System Implementation
 */

#include "InputManager.h"
#include "input.h"  // Existing pin definitions

// ============================================================================
// Singleton Instance
// ============================================================================

InputManager& InputManager::getInstance() {
    static InputManager instance;
    return instance;
}

// ============================================================================
// Constructor
// ============================================================================

InputManager::InputManager()
    : deadzone_(kDefaultDeadzone),
      sensitivity_(kDefaultSensitivity),
      filteringEnabled_(true),
      encoderCount_(0),
      lastEncoderCount_(0)
{
    // Initialize calibration structs
    joyA_X_ = {2048, false, 0.0f};
    joyA_Y_ = {2048, false, 0.0f};
    joyB_X_ = {2048, false, 0.0f};
    joyB_Y_ = {2048, false, 0.0f};

    // Initialize button states
    button1_ = {false, false, 0};
    button2_ = {false, false, 0};
    button3_ = {false, false, 0};
    joyBtnA_ = {false, false, 0};
    joyBtnB_ = {false, false, 0};
    encoderBtn_ = {false, false, 0};
}

// ============================================================================
// Initialization
// ============================================================================

void InputManager::begin() {
    // Set up GPIO pins
    pinMode(encoderA, INPUT_PULLUP);
    pinMode(encoderB, INPUT_PULLUP);
    pinMode(encoderBtn, INPUT_PULLUP);
    pinMode(button1, INPUT_PULLUP);
    pinMode(button2, INPUT_PULLUP);
    pinMode(button3, INPUT_PULLUP);
    pinMode(joystickBtnA, INPUT_PULLUP);
    pinMode(joystickBtnB, INPUT_PULLUP);

    // Set up analog inputs
    pinMode(joystickA_X, INPUT);
    pinMode(joystickA_Y, INPUT);
    pinMode(joystickB_X, INPUT);
    pinMode(joystickB_Y, INPUT);
    pinMode(potA, INPUT);
    pinMode(batteryPin, INPUT);

    // Attach encoder interrupt (same as original input.cpp)
    attachInterrupt(encoderA, []() {
        // Encoder ISR - increment/decrement count based on direction
        if (digitalRead(encoderB) == HIGH) {
            encoderCount++;
        } else {
            encoderCount--;
        }
        InputManager::getInstance().encoderCount_ = encoderCount;
    }, RISING);

    // Attach encoder button interrupt
    attachInterrupt(encoderBtn, []() {
        uint32_t now = millis();
        auto& mgr = InputManager::getInstance();
        if (now - mgr.encoderBtn_.lastChangeTime >= kDebounceMs) {
            encoderBtnState = true;  // Set global for compatibility
            mgr.encoderBtn_.lastChangeTime = now;
        }
    }, RISING);

    Serial.println("[InputManager] Initialized");

    // Verify pull-ups are working - should read HIGH (3.3V) when not pressed
    delay(100);
    Serial.println("[InputManager] Pull-up verification:");
    Serial.printf("  Joystick A Button (GPIO %d): %s (should be HIGH)\n",
                  joystickBtnA, digitalRead(joystickBtnA) == HIGH ? "HIGH" : "LOW");
    Serial.printf("  Joystick B Button (GPIO %d): %s (should be HIGH)\n",
                  joystickBtnB, digitalRead(joystickBtnB) == HIGH ? "HIGH" : "LOW");
    Serial.printf("  Button 1 (GPIO %d): %s\n", button1, digitalRead(button1) == HIGH ? "HIGH" : "LOW");
    Serial.printf("  Button 2 (GPIO %d): %s\n", button2, digitalRead(button2) == HIGH ? "HIGH" : "LOW");
    Serial.printf("  Button 3 (GPIO %d): %s\n", button3, digitalRead(button3) == HIGH ? "HIGH" : "LOW");
    Serial.println("[InputManager] If any button shows LOW, check wiring or add external pull-up resistor");
}

// ============================================================================
// Update (called each frame by framework)
// ============================================================================

void InputManager::update() {
    uint32_t now = millis();

    // Update all button states for edge detection
    updateButtonState(button1_, button1);
    updateButtonState(button2_, button2);
    updateButtonState(button3_, button3);
    updateButtonState(joyBtnA_, joystickBtnA);
    updateButtonState(joyBtnB_, joystickBtnB);
    updateButtonState(encoderBtn_, encoderBtn);
}

// ============================================================================
// Analog Inputs (Calibrated)
// ============================================================================

float InputManager::getJoystickA_X() const {
    return readJoystickAxis(joystickA_X, const_cast<JoystickCalibration&>(joyA_X_));
}

float InputManager::getJoystickA_Y() const {
    return readJoystickAxis(joystickA_Y, const_cast<JoystickCalibration&>(joyA_Y_));
}

float InputManager::getJoystickB_X() const {
    return readJoystickAxis(joystickB_X, const_cast<JoystickCalibration&>(joyB_X_));
}

float InputManager::getJoystickB_Y() const {
    return readJoystickAxis(joystickB_Y, const_cast<JoystickCalibration&>(joyB_Y_));
}

float InputManager::getPotentiometer() const {
    uint16_t raw = analogRead(potA);
    return constrain(raw / 4095.0f, 0.0f, 1.0f);
}

// ============================================================================
// Analog Inputs (Raw)
// ============================================================================

uint16_t InputManager::getJoystickA_X_Raw() const {
    return analogRead(joystickA_X);
}

uint16_t InputManager::getJoystickA_Y_Raw() const {
    return analogRead(joystickA_Y);
}

uint16_t InputManager::getJoystickB_X_Raw() const {
    return analogRead(joystickB_X);
}

uint16_t InputManager::getJoystickB_Y_Raw() const {
    return analogRead(joystickB_Y);
}

uint16_t InputManager::getPotentiometer_Raw() const {
    return analogRead(potA);
}

// ============================================================================
// Battery Reading
// ============================================================================

uint16_t InputManager::getBatteryRaw() const {
    return analogRead(batteryPin);
}

float InputManager::getBatteryVoltage() const {
    // Read ADC value (0-4095 for 12-bit ADC)
    uint16_t adcValue = analogRead(batteryPin);

    // Convert to voltage (ESP32 ADC reference is 3.3V for 4095)
    float voltage = (adcValue / 4095.0f) * 3.3f;

    // Multiply by 2 due to voltage divider with 2 equal resistors
    voltage *= 2.0*1.82;

    return voltage;
}

uint8_t InputManager::getBatteryPercent() const {
    float voltage = getBatteryVoltage();

    // Li-Po voltage curve (1S cell)
    // 4.2V = 100%, 3.0V = 0%
    const float voltageMax = 4.2f;
    const float voltageMin = 3.0f;

    // Clamp voltage to valid range
    if (voltage >= voltageMax) {
        return 100;
    }
    if (voltage <= voltageMin) {
        return 0;
    }

    // Use non-linear Li-Po discharge curve for better accuracy
    // Voltage points: 4.2, 4.0, 3.85, 3.7, 3.5, 3.3, 3.0
    // Percent points: 100,  90,   75,  50,  25,  10,   0

    if (voltage >= 4.0f) {
        // 100% to 90%: 4.2V to 4.0V
        return 90 + (uint8_t)((voltage - 4.0f) / 0.2f * 10.0f);
    } else if (voltage >= 3.85f) {
        // 90% to 75%: 4.0V to 3.85V
        return 75 + (uint8_t)((voltage - 3.85f) / 0.15f * 15.0f);
    } else if (voltage >= 3.7f) {
        // 75% to 50%: 3.85V to 3.7V
        return 50 + (uint8_t)((voltage - 3.7f) / 0.15f * 25.0f);
    } else if (voltage >= 3.5f) {
        // 50% to 25%: 3.7V to 3.5V
        return 25 + (uint8_t)((voltage - 3.5f) / 0.2f * 25.0f);
    } else if (voltage >= 3.3f) {
        // 25% to 10%: 3.5V to 3.3V
        return 10 + (uint8_t)((voltage - 3.3f) / 0.2f * 15.0f);
    } else {
        // 10% to 0%: 3.3V to 3.0V
        return (uint8_t)((voltage - 3.0f) / 0.3f * 10.0f);
    }
}

// ============================================================================
// Digital Inputs (Current State)
// ============================================================================

bool InputManager::getButton1() const {
    return digitalRead(button1) == LOW;
}

bool InputManager::getButton2() const {
    return digitalRead(button2) == LOW;
}

bool InputManager::getButton3() const {
    return digitalRead(button3) == LOW;
}

bool InputManager::getJoystickButtonA() const {
    return digitalRead(joystickBtnA) == LOW;
}

bool InputManager::getJoystickButtonB() const {
    return digitalRead(joystickBtnB) == LOW;
}

bool InputManager::getEncoderButton() const {
    return digitalRead(encoderBtn) == LOW;
}

// ============================================================================
// Digital Inputs (Edge Detection)
// ============================================================================

bool InputManager::getButton1Pressed() const {
    return button1_.current && !button1_.previous;
}

bool InputManager::getButton2Pressed() const {
    return button2_.current && !button2_.previous;
}

bool InputManager::getButton3Pressed() const {
    return button3_.current && !button3_.previous;
}

bool InputManager::getJoystickButtonA_Pressed() const {
    return joyBtnA_.current && !joyBtnA_.previous;
}

bool InputManager::getJoystickButtonB_Pressed() const {
    return joyBtnB_.current && !joyBtnB_.previous;
}

bool InputManager::getEncoderButtonPressed() const {
    return encoderBtn_.current && !encoderBtn_.previous;
}

// ============================================================================
// Encoder
// ============================================================================

int InputManager::getEncoderDelta() {
    int delta = encoderCount_ - lastEncoderCount_;
    lastEncoderCount_ = encoderCount_;
    return delta;
}

int InputManager::getEncoderCount() const {
    return encoderCount_;
}

// ============================================================================
// Configuration
// ============================================================================

void InputManager::setJoystickDeadzone(float deadzone) {
    deadzone_ = constrain(deadzone, 0.0f, 1.0f);
}

float InputManager::getJoystickDeadzone() const {
    return deadzone_;
}

void InputManager::setJoystickSensitivity(float sensitivity) {
    sensitivity_ = constrain(sensitivity, 0.1f, 2.0f);
}

float InputManager::getJoystickSensitivity() const {
    return sensitivity_;
}

void InputManager::recalibrateJoysticks() {
    // Read current positions as new center points
    joyA_X_.center = analogRead(joystickA_X);
    joyA_Y_.center = analogRead(joystickA_Y);
    joyB_X_.center = analogRead(joystickB_X);
    joyB_Y_.center = analogRead(joystickB_Y);

    joyA_X_.initialized = true;
    joyA_Y_.initialized = true;
    joyB_X_.initialized = true;
    joyB_Y_.initialized = true;

    Serial.println("[InputManager] Joysticks recalibrated");
    Serial.printf("  JoyA center: %d, %d\n", joyA_X_.center, joyA_Y_.center);
    Serial.printf("  JoyB center: %d, %d\n", joyB_X_.center, joyB_Y_.center);
}

void InputManager::setJoystickFiltering(bool enable) {
    filteringEnabled_ = enable;
}

bool InputManager::isJoystickFilteringEnabled() const {
    return filteringEnabled_;
}

// ============================================================================
// Helper Methods
// ============================================================================

float InputManager::readJoystickAxis(uint8_t pin, JoystickCalibration& cal) const {
    int raw = analogRead(pin);

    // Auto-calibrate center on first read
    if (!cal.initialized) {
        cal.center = raw;
        cal.initialized = true;
        cal.filtered = 0.0f;
    }

    // Calculate delta from center
    int delta = raw - cal.center;

    // Apply deadzone (absolute pixel deadzone)
    const int deadzonePixels = static_cast<int>(deadzone_ * 2048.0f);
    if (abs(delta) <= deadzonePixels) {
        // Within deadzone - slowly update center to reduce drift
        cal.center = (cal.center * 15 + raw) / 16;
        return 0.0f;
    }

    // Close to center - update center more aggressively
    if (abs(delta) < deadzonePixels * 3) {
        cal.center = (cal.center * 31 + raw) / 32;
    }

    // Calculate normalized value (-1.0 to +1.0)
    float range = delta > 0 ? (4095 - cal.center) : cal.center;
    if (range < 1.0f) range = 1.0f;
    float value = delta / range;

    // Apply sensitivity
    value *= sensitivity_;

    // Clamp to valid range
    value = constrain(value, -1.0f, 1.0f);

    // Apply low-pass filter if enabled
    if (filteringEnabled_) {
        cal.filtered = cal.filtered * (1.0f - kFilterAlpha) + value * kFilterAlpha;
        value = cal.filtered;
    }

    return value;
}

void InputManager::updateButtonState(ButtonState& state, uint8_t pin) {
    uint32_t now = millis();

    // Update previous state
    state.previous = state.current;

    // Read current state with debouncing
    bool reading = digitalRead(pin) == LOW;

    if (reading != state.current) {
        if (now - state.lastChangeTime >= kDebounceMs) {
            state.current = reading;
            state.lastChangeTime = now;
        }
    }
}
