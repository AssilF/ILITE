/**
 * @file FrameworkEngine.cpp
 * @brief ILITE Framework Core Engine Implementation
 *
 * @author ILITE Team
 * @date 2025
 */

#include "FrameworkEngine.h"
#include "ModuleRegistry.h"
#include "input.h"
#include <cstring>

// ============================================================================
// Singleton Instance
// ============================================================================

FrameworkEngine& FrameworkEngine::getInstance() {
    static FrameworkEngine instance;
    return instance;
}

// ============================================================================
// Constructor
// ============================================================================

FrameworkEngine::FrameworkEngine()
    : buttonEngine_(800) // 800ms long press
    , currentModule_(nullptr)
    , isPaired_(false)
    , status_(FrameworkStatus::IDLE)
    , selectedStripButton_(StripButton::MENU)
    , stripButtonCount_(1) // Always have MENU button
    , menuOpen_(false)
    , batteryPercent_(100)
    , statusAnimFrame_(0)
{
    hasEncoderFunction_[0] = false;
    hasEncoderFunction_[1] = false;

    for (int i = 0; i < 3; i++) {
        defaultButtonCallbacks_[i] = nullptr;
    }
}

// ============================================================================
// Initialization
// ============================================================================

void FrameworkEngine::begin() {
    Serial.println("[FrameworkEngine] begin() called"); // TEMP DEBUG
    buttonEngine_.begin();
    Serial.println("[FrameworkEngine] ButtonEventEngine initialized"); // TEMP DEBUG

    // Set up button event routing callbacks
    for (uint8_t i = 0; i < 3; i++) {
        buttonEngine_.setButtonCallback(i, [this, i](ButtonEvent event) {
            routeButtonEvent(i, event);
        });
    }
    Serial.println("[FrameworkEngine] Physical button callbacks set"); // TEMP DEBUG

    // Encoder button callback (always handles strip navigation)
    buttonEngine_.setButtonCallback(ButtonEventEngine::ENCODER_BTN,
        [this](ButtonEvent event) {
            Serial.printf("[FrameworkEngine] Encoder button callback fired! Event: %d\n", (int)event); // TEMP DEBUG
            if (event == ButtonEvent::PRESSED) {
                onEncoderPress();
            }
        });
    Serial.println("[FrameworkEngine] Encoder button callback set"); // TEMP DEBUG

    updateStripButtons();
    Serial.println("[FrameworkEngine] Strip buttons updated"); // TEMP DEBUG
    Serial.println("[FrameworkEngine] Initialization complete!"); // TEMP DEBUG
}

// ============================================================================
// Main Update Loop
// ============================================================================

void FrameworkEngine::update() {
    // Update button state machines
    buttonEngine_.update();

    // Handle encoder rotation for strip navigation
    if (encoderCount != 0) {
        onEncoderRotate(encoderCount);
        encoderCount = 0; // Reset encoder count after handling
    }

    // Update status animation frame
    statusAnimFrame_++;

    // Update current module if loaded and paired
    if (currentModule_ && isPaired_) {
        // Module update is called elsewhere at 50Hz (in main control loop)
        // This is just framework-level housekeeping
    }
}

// ============================================================================
// Rendering
// ============================================================================

void FrameworkEngine::render(DisplayCanvas& canvas) {
    canvas.clear();

    // Render top strip (always framework-owned)
    renderTopStrip(canvas);

    // Render dashboard area
    renderDashboard(canvas);

    // Framework sends buffer (only one place does this)
    canvas.sendBuffer();
}

void FrameworkEngine::renderTopStrip(DisplayCanvas& canvas) {
    const uint8_t stripY = 0;

    // Draw separator line at bottom of strip
    canvas.drawLine(0, STRIP_HEIGHT - 1, 127, STRIP_HEIGHT - 1);

    // Left side: Strip buttons (Menu, F1, F2)
    uint8_t buttonX = 2;
    const uint8_t buttonSpacing = 4;
    const uint8_t buttonWidth = 18;

    // Menu button (always first)
    bool menuSelected = (selectedStripButton_ == StripButton::MENU);
    if (menuSelected) {
        canvas.drawRect(buttonX - 1, stripY + 1, buttonWidth, 7,1); // Highlight
        canvas.setDrawColor(0); // Invert text
    }
    canvas.setFont(DisplayCanvas::TINY);
    canvas.drawText(buttonX + 2, stripY + 7, "≡ MNU");
    if (menuSelected) {
        canvas.setDrawColor(1); // Restore normal
    }
    buttonX += buttonWidth + buttonSpacing;

    // F1 button (if defined)
    if (hasEncoderFunction_[0]) {
        bool f1Selected = (selectedStripButton_ == StripButton::FUNCTION_1);
        if (f1Selected) {
            canvas.drawRect(buttonX - 1, stripY + 1, buttonWidth, 7,1);
            canvas.setDrawColor(0);
        }

        // Show label and toggle state if applicable
        if (encoderFunctions_[0].isToggle && encoderFunctions_[0].toggleState) {
            bool state = *encoderFunctions_[0].toggleState;
            canvas.drawText(buttonX + 2, stripY + 7,
                state ? encoderFunctions_[0].label : "·");
        } else {
            canvas.drawText(buttonX + 2, stripY + 7, encoderFunctions_[0].label);
        }

        if (f1Selected) {
            canvas.setDrawColor(1);
        }
        buttonX += buttonWidth + buttonSpacing;
    }

    // F2 button (if defined)
    if (hasEncoderFunction_[1]) {
        bool f2Selected = (selectedStripButton_ == StripButton::FUNCTION_2);
        if (f2Selected) {
            canvas.drawRect(buttonX - 1, stripY + 1, buttonWidth, 7,1);
            canvas.setDrawColor(0);
        }

        // Show label and toggle state if applicable
        if (encoderFunctions_[1].isToggle && encoderFunctions_[1].toggleState) {
            bool state = *encoderFunctions_[1].toggleState;
            canvas.drawText(buttonX + 2, stripY + 7,
                state ? encoderFunctions_[1].label : "·");
        } else {
            canvas.drawText(buttonX + 2, stripY + 7, encoderFunctions_[1].label);
        }

        if (f2Selected) {
            canvas.setDrawColor(1);
        }
    }

    // Right side: Module name, battery, status
    canvas.setFont(DisplayCanvas::TINY);

    // Battery icon and percentage (rightmost)
    char battStr[8];
    snprintf(battStr, sizeof(battStr), "%d%%", batteryPercent_);
    uint8_t battWidth = strlen(battStr) * 4;
    canvas.drawText(128 - battWidth - 2, stripY + 7, battStr);

    // Status icon
    const char* statusIcon = " ";
    switch (status_) {
        case FrameworkStatus::SCANNING:
            statusIcon = (statusAnimFrame_ / 10) % 2 ? "." : "o";
            break;
        case FrameworkStatus::PAIRED:
            statusIcon = "●";
            break;
        case FrameworkStatus::ERROR_COMM:
        case FrameworkStatus::ERROR_MODULE:
            statusIcon = "!";
            break;
        default:
            statusIcon = "·";
            break;
    }
    canvas.drawText(128 - battWidth - 12, stripY + 7, statusIcon);

    // Module name (if loaded)
    if (currentModule_) {
        const char* moduleName = currentModule_->getModuleName();
        uint8_t nameWidth = strlen(moduleName) * 4;
        uint8_t maxNameX = 128 - battWidth - 20;

        // Truncate if too long
        if (nameWidth > (maxNameX - buttonX)) {
            nameWidth = maxNameX - buttonX;
        }

        canvas.drawText(maxNameX - nameWidth, stripY + 7, moduleName);
    }
}

void FrameworkEngine::renderDashboard(DisplayCanvas& canvas) {
    if (currentModule_ && isPaired_) {
        // Module is loaded and paired - render its dashboard
        currentModule_->drawDashboard(canvas);
    } else {
        // No module or not paired - render generic dashboard
        renderGenericDashboard(canvas);
    }
}

void FrameworkEngine::renderGenericDashboard(DisplayCanvas& canvas) {
    // Generic dashboard showing framework status

    canvas.setFont(DisplayCanvas::NORMAL);

    // Title
    canvas.drawText(32, DASHBOARD_Y + 12, "ILITE v2.0");

    // Module count
    size_t moduleCount = ModuleRegistry::getModuleCount();
    char countStr[32];
    snprintf(countStr, sizeof(countStr), "Modules: %d", moduleCount);
    canvas.setFont(DisplayCanvas::SMALL);
    canvas.drawText(20, DASHBOARD_Y + 26, countStr);

    // Status
    const char* statusStr = "";
    switch (status_) {
        case FrameworkStatus::IDLE:
            statusStr = "Status: Idle";
            break;
        case FrameworkStatus::SCANNING:
            // Animated scanning indicator
            {
                uint8_t dots = (statusAnimFrame_ / 10) % 4;
                static char scanStr[24] = "Status: Scanning";
                for (int i = 0; i < 3; i++) {
                    scanStr[16 + i] = (i < dots) ? '.' : ' ';
                }
                scanStr[19] = '\0';
                statusStr = scanStr;
            }
            break;
        case FrameworkStatus::PAIRING:
            statusStr = "Status: Pairing...";
            break;
        case FrameworkStatus::PAIRED:
            statusStr = "Status: Paired";
            break;
        case FrameworkStatus::ERROR_COMM:
            statusStr = "ERROR: Comm Fail";
            break;
        case FrameworkStatus::ERROR_MODULE:
            statusStr = "ERROR: Module Fail";
            break;
    }
    canvas.drawText(10, DASHBOARD_Y + 38, statusStr);

    // Help text
    canvas.setFont(DisplayCanvas::TINY);
    canvas.drawText(8, DASHBOARD_Y + 50, "Press encoder for menu");
}

// ============================================================================
// Module Management
// ============================================================================

void FrameworkEngine::loadModule(ILITEModule* module) {
    // Deactivate current module if any
    if (currentModule_) {
        currentModule_->onDeactivate();
    }

    // Load new module
    currentModule_ = module;

    // Activate new module
    if (currentModule_) {
        currentModule_->onActivate();

        // TODO: Load encoder functions from module
        // For now, clear them (modules will set them explicitly)
        clearEncoderFunction(0);
        clearEncoderFunction(1);
    }

    updateStripButtons();
}

void FrameworkEngine::setPaired(bool paired) {
    bool wasPaired = isPaired_;
    isPaired_ = paired;

    if (paired && !wasPaired) {
        // Just paired
        if (currentModule_) {
            currentModule_->onPair();
        }
        status_ = FrameworkStatus::PAIRED;
    } else if (!paired && wasPaired) {
        // Just unpaired
        if (currentModule_) {
            currentModule_->onUnpair();
        }
        status_ = FrameworkStatus::IDLE;
    }
}

// ============================================================================
// Button Event Routing
// ============================================================================

void FrameworkEngine::routeButtonEvent(uint8_t buttonIndex, ButtonEvent event) {
    // Route button events to module or default functions

    if (currentModule_ && isPaired_) {
        // Module is loaded and paired - route to module's button functions
        // TODO: Call module's button mapping
        // For now, call onFunctionButton for short press
        if (event == ButtonEvent::PRESSED) {
            currentModule_->onFunctionButton(buttonIndex);
        }
    } else {
        // No module or not paired - use default functions
        if (buttonIndex < 3 && defaultButtonCallbacks_[buttonIndex]) {
            defaultButtonCallbacks_[buttonIndex](event);
        }
    }
}

void FrameworkEngine::setDefaultButtonCallback(uint8_t buttonIndex, ButtonCallback callback) {
    if (buttonIndex < 3) {
        defaultButtonCallbacks_[buttonIndex] = callback;
    }
}

// ============================================================================
// Top Strip & Encoder Functions
// ============================================================================

void FrameworkEngine::setEncoderFunction(uint8_t slot, const EncoderFunction& func) {
    if (slot < 2) {
        encoderFunctions_[slot] = func;
        hasEncoderFunction_[slot] = (func.callback != nullptr);
        updateStripButtons();
    }
}

void FrameworkEngine::clearEncoderFunction(uint8_t slot) {
    if (slot < 2) {
        hasEncoderFunction_[slot] = false;
        encoderFunctions_[slot] = EncoderFunction();
        updateStripButtons();
    }
}

void FrameworkEngine::updateStripButtons() {
    // Count available strip buttons
    stripButtonCount_ = 1; // Always have MENU
    if (hasEncoderFunction_[0]) stripButtonCount_++;
    if (hasEncoderFunction_[1]) stripButtonCount_++;

    // Reset selection if current selection is no longer valid
    uint8_t currentIndex = static_cast<uint8_t>(selectedStripButton_);
    if (currentIndex >= stripButtonCount_) {
        selectedStripButton_ = StripButton::MENU;
    }
}

void FrameworkEngine::onEncoderRotate(int delta) {
    // Navigate through strip buttons
    int currentIndex = static_cast<int>(selectedStripButton_);
    currentIndex += delta;

    // Wrap around
    if (currentIndex < 0) {
        currentIndex = stripButtonCount_ - 1;
    } else if (currentIndex >= stripButtonCount_) {
        currentIndex = 0;
    }

    selectedStripButton_ = static_cast<StripButton>(currentIndex);

    // Audio feedback for encoder rotation
    AudioRegistry::play("menu_select");
    Serial.printf("[Encoder] Rotated %s, selected button: %d\n",
                  delta > 0 ? "CW" : "CCW", currentIndex);
}

void FrameworkEngine::onEncoderPress() {
    // Menu behavior:
    // - When NOT in dashboard (no module or not paired): ALWAYS open menu
    // - When in dashboard (module paired): Only activate selected button

    bool inDashboard = (currentModule_ != nullptr && isPaired_);

    // Audio feedback for encoder press
    AudioRegistry::play("paired");
    Serial.printf("[Encoder] Button pressed, inDashboard: %d, selected: %d\n",
                  inDashboard, static_cast<int>(selectedStripButton_));

    if (!inDashboard) {
        // Not in dashboard - encoder press ALWAYS opens menu
        Serial.println("[Encoder] Opening menu (not in dashboard)");
        openMenu();
    } else {
        // In dashboard - activate selected strip button
        switch (selectedStripButton_) {
            case StripButton::MENU:
                Serial.println("[Encoder] Toggling menu");
                toggleMenu();
                break;

            case StripButton::FUNCTION_1:
                if (hasEncoderFunction_[0] && encoderFunctions_[0].callback) {
                    Serial.printf("[Encoder] Activating F1: %s\n",
                                  encoderFunctions_[0].fullName);
                    encoderFunctions_[0].callback();
                } else {
                    Serial.println("[Encoder] F1 not available");
                }
                break;

            case StripButton::FUNCTION_2:
                if (hasEncoderFunction_[1] && encoderFunctions_[1].callback) {
                    Serial.printf("[Encoder] Activating F2: %s\n",
                                  encoderFunctions_[1].fullName);
                    encoderFunctions_[1].callback();
                } else {
                    Serial.println("[Encoder] F2 not available");
                }
                break;
        }
    }
}

// ============================================================================
// Menu System
// ============================================================================

void FrameworkEngine::openMenu() {
    if (!menuOpen_) {
        menuOpen_ = true;
        AudioRegistry::play("paired");
        Serial.println("[Menu] Opened");
        // TODO: Initialize menu system
    }
}

void FrameworkEngine::closeMenu() {
    if (menuOpen_) {
        menuOpen_ = false;
        AudioRegistry::play("unpaired");
        Serial.println("[Menu] Closed");
    }
}

void FrameworkEngine::toggleMenu() {
    if (menuOpen_) {
        closeMenu();
    } else {
        openMenu();
    }
}
