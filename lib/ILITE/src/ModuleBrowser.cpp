/**
 * @file ModuleBrowser.cpp
 * @brief Module Browser Implementation
 */

#include "ModuleBrowser.h"
#include "DisplayCanvas.h"
#include "ModuleRegistry.h"
#include "ILITEModule.h"
#include "HomeScreen.h"
#include "ILITE.h"
#include <Arduino.h>

// Static member initialization
int ModuleBrowser::currentIndex_ = 0;
int ModuleBrowser::scrollOffset_ = 0;

// Generic/fallback logo (32x32 XBM format - simple grid)
static const uint8_t generic_logo_32x32[] = {
    0x00, 0x00, 0x00, 0x00, 0xFE, 0xFF, 0xFF, 0x7F, 0xFE, 0xFF, 0xFF, 0x7F,
    0x06, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x60,
    0x06, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x60, 0xFE, 0xFF, 0xFF, 0x7F,
    0xFE, 0xFF, 0xFF, 0x7F, 0x06, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x60,
    0x06, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x60,
    0xFE, 0xFF, 0xFF, 0x7F, 0xFE, 0xFF, 0xFF, 0x7F, 0x06, 0x00, 0x00, 0x60,
    0x06, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x60,
    0x06, 0x00, 0x00, 0x60, 0xFE, 0xFF, 0xFF, 0x7F, 0xFE, 0xFF, 0xFF, 0x7F,
    0x06, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x60,
    0x06, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x60, 0xFE, 0xFF, 0xFF, 0x7F,
    0xFE, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00
};

// ============================================================================
// Public Methods
// ============================================================================

void ModuleBrowser::begin() {
    currentIndex_ = 0;
    scrollOffset_ = 0;
    Serial.println("[ModuleBrowser] Initialized");
}

void ModuleBrowser::draw(DisplayCanvas& canvas) {
    canvas.clear();

    // Title bar
    canvas.setFont(DisplayCanvas::SMALL);
    canvas.drawText(2, 8, "Modules");
    canvas.drawHLine(0, 10, 128);

    // Get modules
    int moduleCount = ModuleRegistry::getModuleCount();

    if (moduleCount == 0) {
        canvas.setFont(DisplayCanvas::SMALL);
        canvas.drawTextCentered(32, "No modules");
        canvas.sendBuffer();
        return;
    }

    // Draw single card centered (horizontal scrolling UI)
    ILITEModule* module = ModuleRegistry::getModuleByIndex(currentIndex_);
    if (module != nullptr) {
        // Large centered card (64x48)
        int cardWidth = 64;
        int cardHeight = 48;
        int x = (128 - cardWidth) / 2;  // Center horizontally
        int y = 18;  // Below title bar

        drawLargeCard(canvas, module, x, y);
    }

    // Module counter
    canvas.setFont(DisplayCanvas::TINY);
    char counterText[16];
    snprintf(counterText, sizeof(counterText), "%d/%d", currentIndex_ + 1, moduleCount);
    int counterX = 64 - (strlen(counterText) * 2);  // Center
    canvas.drawText(counterX, 63, counterText);

    // Navigation arrows (left/right)
    if (currentIndex_ > 0) {
        // Left arrow: <
        canvas.drawText(8, 40, "<");
    }
    if (currentIndex_ < moduleCount - 1) {
        // Right arrow: >
        canvas.drawText(116, 40, ">");
    }
}

void ModuleBrowser::onEncoderRotate(int delta) {
    int moduleCount = ModuleRegistry::getModuleCount();
    if (moduleCount == 0) return;

    currentIndex_ += delta;
    if (currentIndex_ < 0) currentIndex_ = 0;
    if (currentIndex_ >= moduleCount) currentIndex_ = moduleCount - 1;
}

void ModuleBrowser::onEncoderButton() {
    ILITEModule* module = getSelectedModule();
    if (module != nullptr) {
        Serial.printf("[ModuleBrowser] Selected: %s\n", module->getModuleName());

        // Activate the module (this is like inserting the game cartridge)
        ::ILITEFramework::getInstance().setActiveModule(module);

        // Switch to module dashboard to show it's running
        HomeScreen::setMode(DisplayMode::MODULE_DASHBOARD);
    }
}

void ModuleBrowser::onBack() {
    HomeScreen::goBack();
}

ILITEModule* ModuleBrowser::getSelectedModule() {
    if (currentIndex_ < 0 || currentIndex_ >= ModuleRegistry::getModuleCount()) {
        return nullptr;
    }
    return ModuleRegistry::getModuleByIndex(currentIndex_);
}

int ModuleBrowser::getCurrentIndex() {
    return currentIndex_;
}

// ============================================================================
// Private Methods
// ============================================================================

void ModuleBrowser::drawCard(DisplayCanvas& canvas, ILITEModule* module, int16_t x, int16_t y, bool selected) {
    if (module == nullptr) return;

    const char* moduleName = module->getModuleName();

    // Draw card border
    if (selected) {
        // Double border for selected card
        canvas.drawRect(x - 2, y - 2, 60, 44, false);
        canvas.drawRect(x - 1, y - 1, 58, 42, false);
    } else {
        // Simple border for unselected card
        canvas.drawRect(x, y, 56, 40, false);
    }

    // Get logo from module (or use generic fallback)
    const uint8_t* logo = module->getLogo32x32();
    if (logo == nullptr) {
        logo = generic_logo_32x32;
    }

    // Draw logo (32x32 centered in card)
    int logoX = x + 12;  // Center horizontally (56-32)/2 = 12
    int logoY = y + 4;   // Top of card with small margin

    // Use U8G2 directly for XBM drawing
    canvas.getU8G2().drawXBM(logoX, logoY, 32, 32, logo);

    // Draw module name below logo (truncated if needed)
    canvas.setFont(DisplayCanvas::TINY);
    char nameBuffer[12];
    strncpy(nameBuffer, moduleName, 11);
    nameBuffer[11] = '\0';

    // Center the text
    int textX = x + 28 - (strlen(nameBuffer) * 2);  // Rough centering
    canvas.drawText(textX, y + 38, nameBuffer);
}

void ModuleBrowser::drawLargeCard(DisplayCanvas& canvas, ILITEModule* module, int16_t x, int16_t y) {
    if (module == nullptr) return;

    const char* moduleName = module->getModuleName();

    // Draw card border (double border for emphasis)
    canvas.drawRect(x - 1, y - 1, 66, 50, false);
    canvas.drawRect(x, y, 64, 48, false);

    // Get logo from module (or use generic fallback)
    const uint8_t* logo = module->getLogo32x32();
    if (logo == nullptr) {
        logo = generic_logo_32x32;
    }

    // Draw logo (32x32 centered horizontally at top)
    int logoX = x + 16;  // Center horizontally (64-32)/2 = 16
    int logoY = y + 2;   // Top of card with small margin

    // Use U8G2 directly for XBM drawing
    canvas.getU8G2().drawXBM(logoX, logoY, 32, 32, logo);

    // Draw module name below logo (centered)
    canvas.setFont(DisplayCanvas::SMALL);
    char nameBuffer[16];
    strncpy(nameBuffer, moduleName, 15);
    nameBuffer[15] = '\0';

    // Center the text
    int textWidth = strlen(nameBuffer) * 4;  // Approximate width for SMALL font
    int textX = x + (64 - textWidth) / 2;
    canvas.drawText(textX, y + 42, nameBuffer);
}
