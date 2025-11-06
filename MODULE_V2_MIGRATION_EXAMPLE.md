# Module Migration to v2.0 - TheGill Example

This document shows how to enhance an existing module to use the new Framework v2.0 features:
- Button event handling (pressed, held, released, long press)
- Encoder functions (top strip software buttons)

---

## Changes Required

### 1. Add Private State Variables

Add to the `TheGillModule` class (private section):

```cpp
private:
    // Button/encoder function states
    bool brakeEngaged_ = false;
    bool honkEngaged_ = false;
    bool easyModeEnabled_ = false;
```

---

### 2. Add Encoder Functions

Add these methods to implement encoder functions in the top strip:

```cpp
public:
    // Number of encoder functions (we'll add 2: Easy Mode and Brake)
    size_t getEncoderFunctionCount() const override {
        return 2;
    }

    // Define encoder function details
    void getEncoderFunction(size_t index, void* funcOut) const override {
        EncoderFunction* func = static_cast<EncoderFunction*>(funcOut);

        if (index == 0) {
            // F1: Easy Mode (toggle button)
            func->label = "EASY";
            func->fullName = "Easy Mode";
            func->callback = [this]() {
                const_cast<TheGillModule*>(this)->toggleEasyMode();
            };
            func->isToggle = true;
            func->toggleState = const_cast<bool*>(&easyModeEnabled_);
        }
        else if (index == 1) {
            // F2: Brake (toggle button)
            func->label = "BRK";
            func->fullName = "Brake";
            func->callback = [this]() {
                const_cast<TheGillModule*>(this)->toggleBrake();
            };
            func->isToggle = true;
            func->toggleState = const_cast<bool*>(&brakeEngaged_);
        }
    }

private:
    void toggleEasyMode() {
        easyModeEnabled_ = !easyModeEnabled_;
        if (easyModeEnabled_) {
            thegillRuntime.easingRate = 2.0f; // Slower acceleration
            Serial.println("[TheGill] Easy mode enabled");
        } else {
            thegillRuntime.easingRate = 4.0f; // Normal acceleration
            Serial.println("[TheGill] Easy mode disabled");
        }
        // Play feedback sound
        getAudio().play("menu_select");
    }

    void toggleBrake() {
        brakeEngaged_ = !brakeEngaged_;
        Serial.printf("[TheGill] Brake %s\n", brakeEngaged_ ? "ON" : "OFF");
        getAudio().play("menu_select");
    }
```

---

### 3. Add Button Event Handling

Add this method to handle physical button events:

```cpp
public:
    // Handle button events from ButtonEventEngine
    void onButtonEvent(size_t buttonIndex, int event) override {
        ButtonEvent btnEvent = static_cast<ButtonEvent>(event);

        switch (buttonIndex) {
            case 0: // Button 1
                if (btnEvent == ButtonEvent::PRESSED) {
                    // Short press: Toggle mode
                    if (thegillConfig.mode == GillMode::Default) {
                        thegillConfig.mode = GillMode::Differential;
                        Serial.println("[TheGill] Switched to Differential mode");
                    } else {
                        thegillConfig.mode = GillMode::Default;
                        Serial.println("[TheGill] Switched to Tank mode");
                    }
                    getAudio().play("menu_select");
                }
                else if (btnEvent == ButtonEvent::LONG_PRESS) {
                    // Long press (800ms): Emergency stop
                    emergencyStop();
                    getAudio().play("error");
                }
                break;

            case 1: // Button 2
                if (btnEvent == ButtonEvent::PRESSED) {
                    // Short press: Toggle honk
                    honkEngaged_ = !honkEngaged_;
                    getAudio().play(honkEngaged_ ? "paired" : "unpaired");
                }
                else if (btnEvent == ButtonEvent::HELD) {
                    // Continuous honk while held
                    honkEngaged_ = true;
                }
                else if (btnEvent == ButtonEvent::RELEASED) {
                    // Stop honk on release
                    honkEngaged_ = false;
                }
                break;

            case 2: // Button 3
                if (btnEvent == ButtonEvent::PRESSED) {
                    // Cycle through easing curves
                    cycleEasingMode();
                    getAudio().play("menu_select");
                }
                break;
        }
    }

private:
    void emergencyStop() {
        Serial.println("[TheGill] EMERGENCY STOP!");
        thegillCommand.leftFront = 0;
        thegillCommand.leftRear = 0;
        thegillCommand.rightFront = 0;
        thegillCommand.rightRear = 0;
        thegillCommand.flags = GILL_FLAG_BRAKE;
        brakeEngaged_ = true;
    }

    void cycleEasingMode() {
        int currentEasing = static_cast<int>(thegillConfig.easing);
        currentEasing = (currentEasing + 1) % 6; // Cycle through 6 easing modes
        thegillConfig.easing = static_cast<GillEasing>(currentEasing);

        const char* easingNames[] = {
            "None", "Linear", "EaseIn", "EaseOut", "EaseInOut", "Sine"
        };
        Serial.printf("[TheGill] Easing mode: %s\n", easingNames[currentEasing]);
    }
```

---

### 4. Update Control Loop

Update `updateControl()` to use the new state variables:

```cpp
void updateControl(const InputManager& inputs, float dt) override {
    // Call existing control update
    updateThegillControl();

    // Apply easy mode multiplier
    if (easyModeEnabled_) {
        thegillCommand.leftFront /= 2;
        thegillCommand.leftRear /= 2;
        thegillCommand.rightFront /= 2;
        thegillCommand.rightRear /= 2;
    }

    // Apply brake state
    if (brakeEngaged_) {
        thegillCommand.flags |= GILL_FLAG_BRAKE;
    } else {
        thegillCommand.flags &= ~GILL_FLAG_BRAKE;
    }

    // Apply honk state
    if (honkEngaged_) {
        thegillCommand.flags |= GILL_FLAG_HONK;
    } else {
        thegillCommand.flags &= ~GILL_FLAG_HONK;
    }
}
```

---

### 5. Update Dashboard Rendering

Update `drawDashboard()` to show new states:

```cpp
void drawDashboard(DisplayCanvas& canvas) override {
    // Dashboard starts at Y=10 (below top strip)
    canvas.setFont(DisplayCanvas::NORMAL);
    canvas.drawTextCentered(20, "TheGill");

    // Show current mode
    canvas.setFont(DisplayCanvas::SMALL);
    const char* modeName = (thegillConfig.mode == GillMode::Default)
        ? "Tank" : "Differential";
    canvas.drawTextCentered(32, modeName);

    // Show motor values
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "L:%d R:%d",
             thegillCommand.leftFront, thegillCommand.rightFront);
    canvas.drawTextCentered(42, buffer);

    // Show status flags at bottom
    canvas.setFont(DisplayCanvas::TINY);
    int x = 2;
    if (easyModeEnabled_) {
        canvas.drawText(x, 60, "[EASY]");
        x += 36;
    }
    if (brakeEngaged_) {
        canvas.drawText(x, 60, "[BRAKE]");
        x += 42;
    }
    if (honkEngaged_) {
        canvas.drawText(x, 60, "[HONK]");
    }
}
```

---

### 6. Register Encoder Functions on Activation

Add to `onActivate()`:

```cpp
void onActivate() override {
    Serial.println("[TheGillModule] Activated");

    // Register encoder functions with FrameworkEngine
    EncoderFunction func;

    // F1: Easy Mode
    getEncoderFunction(0, &func);
    FrameworkEngine::getInstance().setEncoderFunction(0, func);

    // F2: Brake
    getEncoderFunction(1, &func);
    FrameworkEngine::getInstance().setEncoderFunction(1, func);
}

void onDeactivate() override {
    Serial.println("[TheGillModule] Deactivated");

    // Clear encoder functions
    FrameworkEngine::getInstance().clearEncoderFunction(0);
    FrameworkEngine::getInstance().clearEncoderFunction(1);
}
```

---

## Summary of Changes

### Before (v1.x)
```
TheGillModule:
- Basic lifecycle (onInit, onPair, onUnpair)
- Control update
- Dashboard rendering
- No button handling
- No encoder functions
```

### After (v2.0)
```
TheGillModule:
- Basic lifecycle (onInit, onPair, onUnpair, onActivate, onDeactivate)
- Control update (enhanced with easy mode)
- Dashboard rendering (shows new states)
- Button event handling (3 buttons, short/long press)
- Encoder functions (2 functions: Easy Mode, Brake)
- Top strip integration
```

---

## Visual Example

### Top Strip with TheGill Active

```
┌────────────────────────────────────────────────┐
│ [≡ MNU] [EASY]* [BRK] │ TheGill │ 󰁹 98% │ ●  │
├────────────────────────────────────────────────┤
│                                                │
│              TheGill                           │
│            Differential                        │
│               L:512 R:512                      │
│                                                │
│ [EASY] [BRAKE]                                 │
└────────────────────────────────────────────────┘
     ^selected encoder function
```

### Button Mapping

- **Button 1 (Short)**: Toggle Tank/Differential mode
- **Button 1 (Long, 800ms)**: Emergency stop
- **Button 2 (Short)**: Toggle honk
- **Button 2 (Hold)**: Continuous honk
- **Button 2 (Release)**: Stop honk
- **Button 3 (Short)**: Cycle easing mode

### Encoder Functions

- **F1 (EASY)**: Toggle easy mode (half speed)
- **F2 (BRK)**: Toggle brake

---

## Testing Checklist

- [ ] Module loads without errors
- [ ] Top strip shows [≡ MNU] [EASY] [BRK]
- [ ] Encoder can select between menu/F1/F2
- [ ] Pressing encoder on EASY toggles easy mode
- [ ] Pressing encoder on BRK toggles brake
- [ ] Easy mode reduces motor speed by half
- [ ] Brake flag appears in command packet
- [ ] Button 1 short press switches modes
- [ ] Button 1 long press triggers emergency stop
- [ ] Button 2 controls honk (short/hold/release)
- [ ] Button 3 cycles easing modes
- [ ] Dashboard shows current mode and states
- [ ] Status flags display correctly at bottom

---

## Notes

- **Backward Compatibility**: Old modules without these methods still work
- **Optional Features**: Modules can implement only what they need
- **State Management**: Module owns all state, framework just routes events
- **Audio Feedback**: Use `getAudio().play()` for user feedback
- **Serial Logging**: Use Serial.print for debugging

---

**Last Updated:** 2025-01-06
**Version:** 2.0.0
**Author:** ILITE Team
