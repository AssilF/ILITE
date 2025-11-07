/**
 * @file FrameworkEngine.cpp
 * @brief ILITE Framework Core Engine Implementation
 *
 * @author ILITE Team
 * @date 2025
 */

#include "FrameworkEngine.h"
#include "ModuleRegistry.h"
#include "MenuRegistry.h"
#include "ScreenRegistry.h"
#include "UIComponents.h"
#include "DefaultActions.h"
#include "IconLibrary.h"
#include "InputManager.h"
#include "input.h"
#include <algorithm>
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
    , menuSelection_(0)
    , menuScrollOffset_(0)
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

void FrameworkEngine::registerDefaultMenuEntries() {
    // Register framework menu entries (Terminal, Devices, Settings)

    MenuEntry terminal;
    terminal.id = "framework.terminal";
    terminal.parent = MENU_ROOT;
    terminal.icon = ICON_INFO;
    terminal.label = "Terminal";
    terminal.shortLabel = nullptr;
    terminal.onSelect = []() {
        DefaultActions::showTerminal();
    };
    terminal.condition = nullptr;
    terminal.getValue = nullptr;
    terminal.priority = 10;
    terminal.isSubmenu = false;
    terminal.isToggle = false;
    terminal.getToggleState = nullptr;
    terminal.isReadOnly = false;
    terminal.customDraw = nullptr;
    MenuRegistry::registerEntry(terminal);

    MenuEntry devices;
    devices.id = "framework.devices";
    devices.parent = MENU_ROOT;
    devices.icon = ICON_SIGNAL_FULL;
    devices.label = "Devices";
    devices.shortLabel = nullptr;
    devices.onSelect = []() {
        DefaultActions::openDevices();
    };
    devices.condition = nullptr;
    devices.getValue = nullptr;
    devices.priority = 20;
    devices.isSubmenu = false;
    devices.isToggle = false;
    devices.getToggleState = nullptr;
    devices.isReadOnly = false;
    devices.customDraw = nullptr;
    MenuRegistry::registerEntry(devices);

    Serial.println("[FrameworkEngine] Default menu entries registered");
}

void FrameworkEngine::begin() {
    Serial.println("[FrameworkEngine] begin() called");
    buttonEngine_.begin();
    Serial.println("[FrameworkEngine] ButtonEventEngine initialized");

    // Register default menu entries
    registerDefaultMenuEntries();

    // Set up button event routing callbacks
    for (uint8_t i = 0; i < 3; i++) {
        buttonEngine_.setButtonCallback(i, [this, i](ButtonEvent event) {
            routeButtonEvent(i, event);
        });
    }

    // Encoder button callback (always handles strip navigation)
    buttonEngine_.setButtonCallback(ButtonEventEngine::ENCODER_BTN,
        [this](ButtonEvent event) {
            if (event == ButtonEvent::PRESSED) {
                onEncoderPress();
            }
        });

    updateStripButtons();
    Serial.println("[FrameworkEngine] Initialization complete!");
}

// ============================================================================
// Main Update Loop
// ============================================================================

void FrameworkEngine::update() {
    // Update button state machines
    buttonEngine_.update();

    if (ScreenRegistry::hasActiveScreen()) {
        ScreenRegistry::updateActiveScreen();
    }

    // Handle encoder rotation for strip navigation
    if (encoderCount != 0) {
        onEncoderRotate(encoderCount);
        encoderCount = 0; // Reset encoder count after handling
    }

    // Update status animation frame
    statusAnimFrame_++;

    // Update battery reading (every ~1 second at 50Hz)
    static uint8_t batteryUpdateCounter = 0;
    if (++batteryUpdateCounter >= 50) {
        batteryUpdateCounter = 0;
        batteryPercent_ = InputManager::getInstance().getBatteryPercent();
    }

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

    canvas.setFont(DisplayCanvas::TINY);

    // Track left boundary for right-side content
    uint8_t leftBoundary = 2;

    // Left side: Show page title if menu/screens open, otherwise show strip buttons
    if (menuOpen_) {
        // Show "MENU" title
        canvas.setFont(DisplayCanvas::NORMAL);
        canvas.drawText(2, stripY + 8, "MENU");
        leftBoundary = 45;  // Title takes ~40-45 pixels
    } else if (DefaultActions::hasActiveScreen()) {
        // Show screen title
        canvas.setFont(DisplayCanvas::NORMAL);
        const char* title = DefaultActions::getActiveScreenTitle();
        if (title) {
            canvas.drawText(2, stripY + 8, title);
            leftBoundary = 2 + strlen(title) * 6;  // Approximate width
        }
    } else {
        // Normal mode: Show strip buttons (Menu, F1, F2)
        uint8_t buttonX = 2;
        const uint8_t buttonSpacing = 4;
        const uint8_t buttonWidth = 18;

        // Menu button (always first)
        bool menuSelected = (selectedStripButton_ == StripButton::MENU);
        if (menuSelected) {
            canvas.drawRect(buttonX - 1, stripY + 1, buttonWidth, 7,1); // Highlight
            canvas.setDrawColor(0); // Invert text
        }
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

        leftBoundary = buttonX;
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

    // Module name (if loaded and not in menu/screens mode)
    if (currentModule_ && !menuOpen_ && !DefaultActions::hasActiveScreen()) {
        const char* moduleName = currentModule_->getModuleName();
        uint8_t nameWidth = strlen(moduleName) * 4;
        uint8_t maxNameX = 128 - battWidth - 20;

        // Truncate if too long
        if (nameWidth > (maxNameX - leftBoundary)) {
            nameWidth = maxNameX - leftBoundary;
        }

        canvas.drawText(maxNameX - nameWidth, stripY + 7, moduleName);
    }
}

void FrameworkEngine::renderDashboard(DisplayCanvas& canvas) {
    if (menuOpen_) {
        renderMenu(canvas);
        return;
    }

    // Check for DefaultActions screens (Terminal, Devices, Settings)
    if (DefaultActions::hasActiveScreen()) {
        DefaultActions::renderActiveScreen(canvas);
        return;
    }

    if (ScreenRegistry::hasActiveScreen()) {
        ScreenRegistry::drawActiveScreen(canvas);
        return;
    }

    // If a module is loaded (selected), show its dashboard
    // The module can handle rendering "waiting to pair..." if needed
    if (currentModule_) {
        currentModule_->drawDashboard(canvas);
        return;
    }

    // No module loaded - render generic dashboard
    renderGenericDashboard(canvas);
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
// Menu Rendering
// ============================================================================

void FrameworkEngine::renderMenu(DisplayCanvas& canvas) {
    const int16_t startY = DASHBOARD_Y;
    const MenuID activeMenu = currentMenuId();

    auto entries = MenuRegistry::getVisibleEntries(activeMenu);
    if (entries.empty()) {
        canvas.setFont(DisplayCanvas::SMALL);
        canvas.drawText(2, startY + 12, "No entries available");
        return;
    }

    // Validate selection
    const int entryCount = static_cast<int>(entries.size());
    if (menuSelection_ < 0) {
        menuSelection_ = 0;
    }
    if (menuSelection_ >= entryCount) {
        menuSelection_ = entryCount - 1;
    }

    // Calculate visible items (full height available now, no title or footer)
    const int availableHeight = canvas.getHeight() - startY;
    const int itemHeight = 11;
    const int maxVisibleItems = availableHeight / itemHeight;

    // Update scroll offset to keep selection visible
    if (menuSelection_ < menuScrollOffset_) {
        menuScrollOffset_ = menuSelection_;
    } else if (menuSelection_ >= menuScrollOffset_ + maxVisibleItems) {
        menuScrollOffset_ = menuSelection_ - maxVisibleItems + 1;
    }

    // Render visible entries
    int16_t y = startY + 6;
    const int endIndex = std::min(menuScrollOffset_ + maxVisibleItems, entryCount);
    for (int i = menuScrollOffset_; i < endIndex; ++i) {
        const MenuEntry* entry = entries[i];
        if (!entry) {
            continue;
        }

        const bool selected = (i == menuSelection_);
        if (selected) {
            canvas.drawRect(0, y - 6, canvas.getWidth(), 11, true);
            canvas.setDrawColor(0);
        }

        int16_t textX = 4;
        if (entry->icon != nullptr) {
            canvas.drawIconByID(4, y - 8, entry->icon);
            textX = 14;
        }

        canvas.setFont(DisplayCanvas::SMALL);
        canvas.drawText(textX, y, entry->label ? entry->label : "(unnamed)");

        if (entry->isToggle && entry->getToggleState) {
            canvas.drawTextRight(canvas.getWidth() - 4, y, entry->getToggleState() ? "ON" : "OFF");
        } else if (entry->getValue) {
            const char* value = entry->getValue();
            if (value != nullptr) {
                canvas.drawTextRight(canvas.getWidth() - 4, y, value);
            }
        } else if (MenuRegistry::hasChildren(entry->id) || entry->isSubmenu) {
            canvas.drawTextRight(canvas.getWidth() - 4, y, ">");
        }

        if (selected) {
            canvas.setDrawColor(1);
        }

        y += itemHeight;
    }

    // Draw scroll indicators
    if (entryCount > maxVisibleItems) {
        const int scrollbarX = canvas.getWidth() - 2;
        const int scrollbarHeight = availableHeight - 4;
        const int scrollbarY = startY + 6;

        // Scroll indicator position
        const float scrollPercent = static_cast<float>(menuScrollOffset_) / (entryCount - maxVisibleItems);
        const int indicatorY = scrollbarY + static_cast<int>(scrollPercent * (scrollbarHeight - 4));

        // Draw small scroll indicator
        canvas.drawVLine(scrollbarX, indicatorY, 4);
    }
}

void FrameworkEngine::navigateMenu(int delta) {
    auto entries = MenuRegistry::getVisibleEntries(currentMenuId());
    if (entries.empty()) {
        menuSelection_ = 0;
        return;
    }

    const int count = static_cast<int>(entries.size());
    menuSelection_ = (menuSelection_ + delta) % count;
    if (menuSelection_ < 0) {
        menuSelection_ += count;
    }

    AudioRegistry::play("menu_select");
}

void FrameworkEngine::activateMenuSelection() {
    auto entries = MenuRegistry::getVisibleEntries(currentMenuId());
    if (entries.empty()) {
        AudioRegistry::play("error");
        return;
    }

    if (menuSelection_ < 0) {
        menuSelection_ = 0;
    }
    if (menuSelection_ >= static_cast<int>(entries.size())) {
        menuSelection_ = static_cast<int>(entries.size()) - 1;
    }

    const MenuEntry* entry = entries[menuSelection_];
    if (entry == nullptr) {
        return;
    }

    if (MenuRegistry::hasChildren(entry->id) || entry->isSubmenu) {
        menuStack_.push_back(entry->id);
        menuSelection_ = 0;
        AudioRegistry::play("menu_select");
        return;
    }

    bool handled = false;

    if (entry->onSelect) {
        entry->onSelect();
        handled = true;
    }

    if (ScreenRegistry::hasScreen(entry->id) && ScreenRegistry::show(entry->id)) {
        AudioRegistry::play("paired");
        closeMenu();
        handled = true;
        return;
    }

    if (handled) {
        AudioRegistry::play("paired");
        // Close menu after activating an entry
        closeMenu();
    } else {
        AudioRegistry::play("menu_select");
    }
}

void FrameworkEngine::menuGoBack() {
    if (menuStack_.empty()) {
        closeMenu();
        return;
    }

    menuStack_.pop_back();
    menuSelection_ = 0;
    menuScrollOffset_ = 0;
    AudioRegistry::play("menu_select");
}

MenuID FrameworkEngine::currentMenuId() const {
    if (menuStack_.empty()) {
        return MENU_ROOT;
    }
    return menuStack_.back();
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

    if (menuOpen_) {
        if (event == ButtonEvent::PRESSED) {
            switch (buttonIndex) {
                case 0:
                    menuGoBack();
                    break;
                case 1:
                    activateMenuSelection();
                    break;
                case 2:
                    closeMenu();
                    break;
            }
        }
        return;
    }

    // Handle DefaultActions screens - any button closes them
    if (DefaultActions::hasActiveScreen()) {
        if (event == ButtonEvent::PRESSED) {
            DefaultActions::closeActiveScreen();
            AudioRegistry::play("unpaired");
        }
        return;
    }

    if (ScreenRegistry::hasActiveScreen()) {
        if (event == ButtonEvent::PRESSED) {
            ScreenRegistry::handleButton(static_cast<int>(buttonIndex) + 1);
        }
        return;
    }

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
    if (delta == 0) {
        return;
    }

    if (menuOpen_) {
        navigateMenu(delta);
        return;
    }

    // Handle DefaultActions screens first
    if (DefaultActions::hasActiveScreen()) {
        DefaultActions::handleEncoderRotate(delta);
        return;
    }

    if (ScreenRegistry::hasActiveScreen()) {
        ScreenRegistry::handleEncoderRotate(delta);
        return;
    }

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
    if (menuOpen_) {
        activateMenuSelection();
        return;
    }

    // Handle DefaultActions screens first
    if (DefaultActions::hasActiveScreen()) {
        DefaultActions::handleEncoderPress();
        return;
    }

    if (ScreenRegistry::hasActiveScreen()) {
        ScreenRegistry::handleEncoderPress();
        return;
    }

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
    if (menuOpen_) {
        return;
    }

    menuOpen_ = true;
    menuStack_.clear();
    menuSelection_ = 0;
    menuScrollOffset_ = 0;
    ScreenRegistry::clearStack();
    selectedStripButton_ = StripButton::MENU;
    AudioRegistry::play("paired");
    Serial.println("[Menu] Opened");
}

void FrameworkEngine::closeMenu() {
    if (!menuOpen_) {
        return;
    }

    menuOpen_ = false;
    menuStack_.clear();
    menuSelection_ = 0;
    menuScrollOffset_ = 0;
    AudioRegistry::play("unpaired");
    Serial.println("[Menu] Closed");
}

void FrameworkEngine::toggleMenu() {
    if (menuOpen_) {
        closeMenu();
    } else {
        openMenu();
    }
}
