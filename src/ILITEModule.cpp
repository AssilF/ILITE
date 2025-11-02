/**
 * @file ILITEModule.cpp
 * @brief ILITEModule Base Class Implementation
 */

#include "ILITEModule.h"
#include "InputManager.h"
#include "ILITEHelpers.h"
#include "DisplayCanvas.h"
#include "espnow_discovery.h"

// External references
extern EspNowDiscovery discovery;

// ============================================================================
// Helper Function Implementations
// ============================================================================

InputManager& ILITEModule::getInputs() {
    return InputManager::getInstance();
}

PreferencesManager& ILITEModule::getPreferences() {
    return PreferencesManager::getInstance();
}

Logger& ILITEModule::getLogger() {
    return Logger::getInstance();
}

Audio& ILITEModule::getAudio() {
    return Audio::getInstance();
}

bool ILITEModule::isPaired() const {
    return paired_;
}

uint32_t ILITEModule::getTimeSinceLastTelemetry() const {
    if (lastTelemetryTime_ == 0) {
        return 0;
    }
    return millis() - lastTelemetryTime_;
}

bool ILITEModule::sendCommand(const char* command) {
    // TODO: Will be implemented when we integrate with main framework
    // For now, stub implementation
    getLogger().logf("Module command: %s", command);
    return false;
}

// ============================================================================
// Default Implementations for Optional Methods
// ============================================================================

void ILITEModule::drawLayoutCard(DisplayCanvas& canvas, int16_t x, int16_t y, bool focused) {
    // Default implementation - simple card with module name
    const int16_t width = canvas.getWidth() - x * 2;
    const int16_t height = 40;

    canvas.setDrawColor(1);
    canvas.drawRoundRect(x, y, width, height, 6, false);

    if (focused) {
        canvas.drawRoundRect(x + 1, y + 1, width - 2, height - 2, 6, false);
    }

    canvas.setFont(DisplayCanvas::NORMAL);
    canvas.drawText(x + 8, y + 16, getModuleName());

    canvas.setFont(DisplayCanvas::SMALL);
    canvas.drawTextF(x + 8, y + 28, "v%s", getVersion());
}

void ILITEModule::onFunctionButton(size_t buttonSlot) {
    // Default implementation - call assigned action callback
    FunctionAction action = getFunctionAction(buttonSlot);
    if (action.callback) {
        action.callback();
    }
}
