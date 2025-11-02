/**
 * @file InputManager.h
 * @brief ILITE Framework - Input Management System
 *
 * Provides clean, calibrated access to all controller inputs:
 * - Dual analog joysticks with auto-calibration
 * - Rotary encoder with debouncing
 * - Push buttons with edge detection
 * - Potentiometer
 *
 * Features:
 * - Automatic center calibration
 * - Adjustable deadzone
 * - Low-pass filtering
 * - Edge detection (pressed/released)
 * - Thread-safe for FreeRTOS
 *
 * @author ILITE Team
 * @date 2025
 */

#pragma once
#include <Arduino.h>

/**
 * @brief Manages all controller input devices
 *
 * Singleton class providing calibrated, filtered access to joysticks,
 * buttons, encoder, and potentiometer.
 *
 * ## Usage Example
 * ```cpp
 * void updateControl(const InputManager& inputs, float dt) {
 *     float throttle = inputs.getJoystickA_Y();  // -1.0 to +1.0
 *     float yaw = inputs.getJoystickA_X();
 *
 *     if (inputs.getButton1Pressed()) {
 *         // Button just pressed this frame
 *     }
 *
 *     int encoderDelta = inputs.getEncoderDelta();
 *     if (encoderDelta != 0) {
 *         // Encoder rotated
 *     }
 * }
 * ```
 *
 * ## Calibration
 * Joysticks auto-calibrate center on first read. To recalibrate manually:
 * ```cpp
 * inputs.recalibrateJoysticks();  // Center sticks first!
 * ```
 *
 * ## Thread Safety
 * Reading methods are thread-safe (called from multiple FreeRTOS tasks).
 * Configuration methods should only be called from setup() or main loop.
 */
class InputManager {
public:
    // ========================================================================
    // Singleton Access
    // ========================================================================

    /**
     * @brief Get singleton instance
     * @return Reference to InputManager instance
     */
    static InputManager& getInstance();

    // ========================================================================
    // Analog Inputs (Calibrated)
    // ========================================================================

    /**
     * @brief Get Joystick A horizontal axis (calibrated)
     *
     * Auto-calibrated with deadzone and filtering.
     *
     * @return Value from -1.0 (full left) to +1.0 (full right), 0.0 at center
     */
    float getJoystickA_X() const;

    /**
     * @brief Get Joystick A vertical axis (calibrated)
     *
     * @return Value from -1.0 (full down) to +1.0 (full up), 0.0 at center
     */
    float getJoystickA_Y() const;

    /**
     * @brief Get Joystick B horizontal axis (calibrated)
     *
     * @return Value from -1.0 (full left) to +1.0 (full right), 0.0 at center
     */
    float getJoystickB_X() const;

    /**
     * @brief Get Joystick B vertical axis (calibrated)
     *
     * @return Value from -1.0 (full down) to +1.0 (full up), 0.0 at center
     */
    float getJoystickB_Y() const;

    /**
     * @brief Get potentiometer value (calibrated)
     *
     * @return Value from 0.0 (minimum) to 1.0 (maximum)
     */
    float getPotentiometer() const;

    // ========================================================================
    // Analog Inputs (Raw)
    // ========================================================================

    /**
     * @brief Get raw Joystick A X value (no calibration)
     * @return ADC value 0-4095
     */
    uint16_t getJoystickA_X_Raw() const;

    /**
     * @brief Get raw Joystick A Y value (no calibration)
     * @return ADC value 0-4095
     */
    uint16_t getJoystickA_Y_Raw() const;

    /**
     * @brief Get raw Joystick B X value (no calibration)
     * @return ADC value 0-4095
     */
    uint16_t getJoystickB_X_Raw() const;

    /**
     * @brief Get raw Joystick B Y value (no calibration)
     * @return ADC value 0-4095
     */
    uint16_t getJoystickB_Y_Raw() const;

    /**
     * @brief Get raw potentiometer value (no calibration)
     * @return ADC value 0-4095
     */
    uint16_t getPotentiometer_Raw() const;

    // ========================================================================
    // Digital Inputs (Current State)
    // ========================================================================

    /**
     * @brief Get Function Button 1 state
     * @return true if pressed, false if released
     */
    bool getButton1() const;

    /**
     * @brief Get Function Button 2 state
     * @return true if pressed, false if released
     */
    bool getButton2() const;

    /**
     * @brief Get Function Button 3 state
     * @return true if pressed, false if released
     */
    bool getButton3() const;

    /**
     * @brief Get Joystick A button state
     * @return true if pressed, false if released
     */
    bool getJoystickButtonA() const;

    /**
     * @brief Get Joystick B button state
     * @return true if pressed, false if released
     */
    bool getJoystickButtonB() const;

    /**
     * @brief Get Encoder button state
     * @return true if pressed, false if released
     */
    bool getEncoderButton() const;

    // ========================================================================
    // Digital Inputs (Edge Detection)
    // ========================================================================

    /**
     * @brief Check if Button 1 was just pressed (rising edge)
     *
     * True for one frame when button transitions from released to pressed.
     * Use for single-shot actions (toggle, menu select, etc.).
     *
     * @return true if pressed this frame
     */
    bool getButton1Pressed() const;

    /**
     * @brief Check if Button 2 was just pressed
     * @return true if pressed this frame
     */
    bool getButton2Pressed() const;

    /**
     * @brief Check if Button 3 was just pressed
     * @return true if pressed this frame
     */
    bool getButton3Pressed() const;

    /**
     * @brief Check if Joystick A button was just pressed
     * @return true if pressed this frame
     */
    bool getJoystickButtonA_Pressed() const;

    /**
     * @brief Check if Joystick B button was just pressed
     * @return true if pressed this frame
     */
    bool getJoystickButtonB_Pressed() const;

    /**
     * @brief Check if Encoder button was just pressed
     * @return true if pressed this frame
     */
    bool getEncoderButtonPressed() const;

    // ========================================================================
    // Encoder
    // ========================================================================

    /**
     * @brief Get encoder rotation delta since last call
     *
     * Positive = clockwise, negative = counterclockwise.
     * Resets to 0 after reading (non-accumulating).
     *
     * @return Rotation delta (+/- N detents)
     */
    int getEncoderDelta();

    /**
     * @brief Get absolute encoder count since boot
     *
     * Accumulates indefinitely (wraps at INT_MAX).
     *
     * @return Total rotation count
     */
    int getEncoderCount() const;

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * @brief Set joystick deadzone radius
     *
     * Values within deadzone of center return 0.0. Reduces drift/jitter.
     *
     * @param deadzone Radius from center (0.0 to 1.0, default 0.05)
     */
    void setJoystickDeadzone(float deadzone);

    /**
     * @brief Get current deadzone setting
     * @return Deadzone radius (0.0 to 1.0)
     */
    float getJoystickDeadzone() const;

    /**
     * @brief Set joystick sensitivity multiplier
     *
     * Values < 1.0 reduce sensitivity, > 1.0 increase it.
     * Applied after deadzone but before output clamp.
     *
     * @param sensitivity Multiplier (0.1 to 2.0, default 1.0)
     */
    void setJoystickSensitivity(float sensitivity);

    /**
     * @brief Get current sensitivity setting
     * @return Sensitivity multiplier
     */
    float getJoystickSensitivity() const;

    /**
     * @brief Recalibrate joystick center points
     *
     * Call when joysticks are centered. Updates center point for all axes.
     * Useful if joysticks drift over time or controller is moved.
     */
    void recalibrateJoysticks();

    /**
     * @brief Enable/disable low-pass filtering on joysticks
     *
     * Reduces noise but adds slight lag. Enabled by default.
     *
     * @param enable true to enable filtering, false to disable
     */
    void setJoystickFiltering(bool enable);

    /**
     * @brief Check if filtering is enabled
     * @return true if filtering enabled
     */
    bool isJoystickFilteringEnabled() const;

    // ========================================================================
    // System Methods (called by framework)
    // ========================================================================

    /**
     * @brief Initialize input hardware
     *
     * Called once by framework during ILITE.begin().
     * Sets up GPIO, ADC, interrupts.
     */
    void begin();

    /**
     * @brief Update input state (called each loop)
     *
     * Called by framework in main loop to update button edge detection
     * and encoder delta calculation. Should not be called by user code.
     */
    void update();

private:
    InputManager();  // Private constructor (singleton)
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // ========================================================================
    // Internal State
    // ========================================================================

    // Joystick calibration data
    struct JoystickCalibration {
        int16_t center;           // Center point (typically ~2048)
        bool initialized;         // Whether center has been calibrated
        float filtered;           // Filtered output value
    };

    JoystickCalibration joyA_X_;
    JoystickCalibration joyA_Y_;
    JoystickCalibration joyB_X_;
    JoystickCalibration joyB_Y_;

    // Button edge detection
    struct ButtonState {
        bool current;             // Current state (true = pressed)
        bool previous;            // Previous frame state
        uint32_t lastChangeTime;  // For debouncing
    };

    ButtonState button1_;
    ButtonState button2_;
    ButtonState button3_;
    ButtonState joyBtnA_;
    ButtonState joyBtnB_;
    ButtonState encoderBtn_;

    // Encoder state
    int encoderCount_;            // Absolute count
    int lastEncoderCount_;        // Last read count (for delta)

    // Configuration
    float deadzone_;              // Deadzone radius (0.0 to 1.0)
    float sensitivity_;           // Sensitivity multiplier
    bool filteringEnabled_;       // Whether to apply low-pass filter

    // Constants
    static constexpr float kDefaultDeadzone = 0.05f;
    static constexpr float kDefaultSensitivity = 1.0f;
    static constexpr uint32_t kDebounceMs = 50;
    static constexpr float kFilterAlpha = 0.2f;  // IIR filter coefficient

    // Helper methods
    float readJoystickAxis(uint8_t pin, JoystickCalibration& cal) const;
    void updateButtonState(ButtonState& state, uint8_t pin);
};
