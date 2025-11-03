/**
 * @file ScreenRegistry.cpp
 * @brief Screen Registry Implementation
 */

#include "ScreenRegistry.h"
#include <cstring>

// Static storage
std::vector<Screen> ScreenRegistry::screens_;
std::vector<const Screen*> ScreenRegistry::navigationStack_;

// ============================================================================
// Registration
// ============================================================================

void ScreenRegistry::registerScreen(const Screen& screen) {
    // Check for duplicate IDs
    for (const auto& existing : screens_) {
        if (strcmp(existing.id, screen.id) == 0) {
            Serial.printf("[ScreenRegistry] WARNING: Duplicate screen '%s' (ignoring)\n", screen.id);
            return;
        }
    }

    screens_.push_back(screen);
    Serial.printf("[ScreenRegistry] Registered screen: %s (%s)\n",
                  screen.id, screen.title ? screen.title : "Untitled");
}

// ============================================================================
// Queries
// ============================================================================

const Screen* ScreenRegistry::getScreen(ScreenID id) {
    if (id == nullptr) {
        return nullptr;
    }

    for (const auto& screen : screens_) {
        if (strcmp(screen.id, id) == 0) {
            return &screen;
        }
    }

    return nullptr;
}

bool ScreenRegistry::hasScreen(ScreenID id) {
    return getScreen(id) != nullptr;
}

std::vector<Screen>& ScreenRegistry::getAllScreens() {
    return screens_;
}

void ScreenRegistry::clear() {
    screens_.clear();
    navigationStack_.clear();
}

// ============================================================================
// Navigation
// ============================================================================

bool ScreenRegistry::show(ScreenID id) {
    const Screen* screen = getScreen(id);
    if (screen == nullptr) {
        Serial.printf("[ScreenRegistry] Screen not found: %s\n", id);
        return false;
    }

    // Check condition if present
    if (screen->condition && !screen->condition()) {
        Serial.printf("[ScreenRegistry] Screen condition not met: %s\n", id);
        return false;
    }

    // Call onHide for previous screen
    if (!navigationStack_.empty()) {
        const Screen* prevScreen = navigationStack_.back();
        if (prevScreen->onHide) {
            prevScreen->onHide();
        }
    }

    // Push onto stack
    navigationStack_.push_back(screen);

    // Call onShow
    if (screen->onShow) {
        screen->onShow();
    }

    Serial.printf("[ScreenRegistry] Showing screen: %s\n", id);
    return true;
}

bool ScreenRegistry::back() {
    if (navigationStack_.empty()) {
        return false;
    }

    // Call onHide for current screen
    const Screen* currentScreen = navigationStack_.back();
    if (currentScreen->onHide) {
        currentScreen->onHide();
    }

    // Pop from stack
    navigationStack_.pop_back();

    // Call onShow for previous screen (if any)
    if (!navigationStack_.empty()) {
        const Screen* prevScreen = navigationStack_.back();
        if (prevScreen->onShow) {
            prevScreen->onShow();
        }
        Serial.printf("[ScreenRegistry] Back to screen: %s\n", prevScreen->id);
    } else {
        Serial.println("[ScreenRegistry] Back to default display");
    }

    return true;
}

void ScreenRegistry::clearStack() {
    // Call onHide for all screens
    for (auto it = navigationStack_.rbegin(); it != navigationStack_.rend(); ++it) {
        const Screen* screen = *it;
        if (screen->onHide) {
            screen->onHide();
        }
    }

    navigationStack_.clear();
    Serial.println("[ScreenRegistry] Navigation stack cleared");
}

const Screen* ScreenRegistry::getActiveScreen() {
    if (navigationStack_.empty()) {
        return nullptr;
    }
    return navigationStack_.back();
}

bool ScreenRegistry::hasActiveScreen() {
    return !navigationStack_.empty();
}

// ============================================================================
// Event Handling
// ============================================================================

void ScreenRegistry::handleEncoderRotate(int delta) {
    const Screen* active = getActiveScreen();
    if (active && active->onEncoderRotate) {
        active->onEncoderRotate(delta);
    }
}

void ScreenRegistry::handleEncoderPress() {
    const Screen* active = getActiveScreen();
    if (active && active->onEncoderPress) {
        active->onEncoderPress();
    }
}

void ScreenRegistry::handleButton(int button) {
    const Screen* active = getActiveScreen();
    if (active) {
        switch (button) {
            case 1:
                if (active->onButton1) active->onButton1();
                break;
            case 2:
                if (active->onButton2) active->onButton2();
                break;
            case 3:
                if (active->onButton3) active->onButton3();
                break;
        }
    }
}

// ============================================================================
// Rendering
// ============================================================================

void ScreenRegistry::updateActiveScreen() {
    const Screen* active = getActiveScreen();
    if (active && active->updateFunc) {
        active->updateFunc();
    }
}

void ScreenRegistry::drawActiveScreen(DisplayCanvas& canvas) {
    const Screen* active = getActiveScreen();
    if (active && active->drawFunc) {
        if (active->isModal) {
            // For modal screens, don't clear canvas (draw over existing content)
            active->drawFunc(canvas);
        } else {
            // For full-screen, clear first
            canvas.clear();
            active->drawFunc(canvas);
        }
    }
}
