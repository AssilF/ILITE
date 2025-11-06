/**
 * @file ModuleBrowser.h
 * @brief Module Browser - Card-based Module Selection
 *
 * Displays registered modules as cards with logos/names.
 * Allows browsing and selecting modules to view their dashboards.
 *
 * @author ILITE Framework
 * @date 2025
 */

#pragma once

#include <Arduino.h>

// Forward declarations
class DisplayCanvas;
class ILITEModule;

/**
 * @brief Module Browser - Card-based UI for module selection
 */
class ModuleBrowser {
public:
    /**
     * @brief Initialize module browser
     */
    static void begin();

    /**
     * @brief Draw module browser
     * @param canvas Display canvas to draw on
     */
    static void draw(DisplayCanvas& canvas);

    /**
     * @brief Handle encoder rotation (navigate cards)
     * @param delta Rotation delta (-1 or +1)
     */
    static void onEncoderRotate(int delta);

    /**
     * @brief Handle encoder button press (select module)
     */
    static void onEncoderButton();

    /**
     * @brief Handle back button
     */
    static void onBack();

    /**
     * @brief Get currently selected module
     * @return Pointer to selected module, or nullptr
     */
    static ILITEModule* getSelectedModule();

    /**
     * @brief Get current card index
     * @return Index of selected card
     */
    static int getCurrentIndex();

private:
    static void drawCard(DisplayCanvas& canvas, ILITEModule* module, int16_t x, int16_t y, bool selected);
    static void drawLargeCard(DisplayCanvas& canvas, ILITEModule* module, int16_t x, int16_t y);

    static int currentIndex_;
    static int scrollOffset_;
};
