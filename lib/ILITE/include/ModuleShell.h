#pragma once

/**
 * @file ModuleShell.h
 * @brief Runtime UI shell for active modules.
 *
 * ModuleShell is responsible for rendering the module status strip, delegating
 * drawing to the active module, and hosting the module-specific menu overlay.
 * It also centralises encoder/button handling while a module is active.
 */

#include <cstddef>
#include <vector>

class ILITEModule;
class DisplayCanvas;
class ILITEFramework;

#include "ModuleMenu.h"
#include "IconLibrary.h"
#include "UIComponents.h"

class ModuleShell {
public:
    /**
     * @brief Notify the shell that a module became active.
     */
    static void onModuleActivated(ILITEModule* module);

    /**
     * @brief Notify the shell that the currently active module was deactivated.
     */
    static void onModuleDeactivated(ILITEModule* module);

    /**
     * @brief Draw active module UI (status bar, dashboard, optional menu overlay).
     *
     * @return true if the shell rendered the frame (i.e. a module is active).
     */
    static bool draw(DisplayCanvas& canvas, ILITEFramework& framework);

    /**
     * @brief Poll raw inputs (encoder/button) while a module is active.
     */
    static void pollInput();

    /**
     * @brief Close any open menu state (e.g., when unpairing).
     */
    static void reset();

private:
    struct MenuLevel {
        ModuleMenuItem* node;
        int focus = 0;
        int scroll = 0;

        MenuLevel(ModuleMenuItem* n, int f, int s) : node(n), focus(f), scroll(s) {}
    };

    static void rebuildMenu();
    static void openMenu();
    static void closeMenu();

    static void handleEncoderRotate(int delta);
    static void handleEncoderPress();
    static void handleButton3();

    static void drawMenuOverlay(DisplayCanvas& canvas);
    static ModuleMenuItem* currentMenuNode();
    static std::vector<ModuleMenuItem*> collectVisibleChildren(ModuleMenuItem& node);
    static ModuleMenuItem* visibleChildAt(ModuleMenuItem& node, int index);

    static ILITEModule* activeModule_;
    static bool menuOpen_;
    static ModuleMenuItem menuRoot_;
    static ModuleMenuBuilder menuBuilder_;
    static std::vector<MenuLevel> menuStack_;

    // Input edge tracking
    static int lastEncoderCount_;
    static bool lastEncoderButton_;
    static bool lastButton3_;
};
