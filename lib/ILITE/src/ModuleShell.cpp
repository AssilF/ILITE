#include "ModuleShell.h"

#include "DisplayCanvas.h"
#include "ILITE.h"
#include "ILITEModule.h"
#include "InputManager.h"
#include "ScreenRegistry.h"
#include "display.h"
#include "input.h"

#include <algorithm>

ILITEModule* ModuleShell::activeModule_ = nullptr;
bool ModuleShell::menuOpen_ = false;
ModuleMenuItem ModuleShell::menuRoot_{};
ModuleMenuBuilder ModuleShell::menuBuilder_{menuRoot_};
std::vector<ModuleShell::MenuLevel> ModuleShell::menuStack_{};
int ModuleShell::lastEncoderCount_ = 0;
bool ModuleShell::lastEncoderButton_ = false;
bool ModuleShell::lastButton3_ = false;

void ModuleShell::onModuleActivated(ILITEModule* module) {
    activeModule_ = module;
    reset();
    if (activeModule_ != nullptr) {
        rebuildMenu();
        lastEncoderCount_ = encoderCount;
        lastEncoderButton_ = (digitalRead(encoderBtn) == LOW);
        lastButton3_ = (digitalRead(button3) == LOW);
    }
}

void ModuleShell::onModuleDeactivated(ILITEModule* module) {
    if (module != nullptr && module == activeModule_) {
        reset();
        activeModule_ = nullptr;
    }
}

bool ModuleShell::draw(DisplayCanvas& canvas, ILITEFramework& framework) {
    if (activeModule_ == nullptr) {
        return false;
    }

    activeModule_->drawDashboard(canvas);

    if (menuOpen_) {
        drawMenuOverlay(canvas);
    }

    return true;
}

void ModuleShell::pollInput() {
    if (activeModule_ == nullptr) {
        return;
    }

    int currentEncoder = encoderCount;
    if (currentEncoder != lastEncoderCount_) {
        int delta = currentEncoder - lastEncoderCount_;
        handleEncoderRotate((delta > 0) ? 1 : -1);
        lastEncoderCount_ = currentEncoder;
    }

    bool encoderButton = (digitalRead(encoderBtn) == LOW);
    if (encoderButton && !lastEncoderButton_) {
        handleEncoderPress();
    }
    lastEncoderButton_ = encoderButton;

    bool button3State = (digitalRead(button3) == LOW);
    if (button3State && !lastButton3_) {
        handleButton3();
    }
    lastButton3_ = button3State;
}

void ModuleShell::reset() {
    menuOpen_ = false;
    menuStack_.clear();
    menuBuilder_.clear();
    menuRoot_.children.clear();
}

void ModuleShell::rebuildMenu() {
    menuBuilder_.clear();
    if (activeModule_ != nullptr) {
        activeModule_->buildModuleMenu(menuBuilder_);
    }
    menuStack_.clear();
    menuStack_.push_back(MenuLevel{&menuRoot_, 0, 0});
}

void ModuleShell::openMenu() {
    if (menuOpen_) {
        return;
    }
    rebuildMenu();
    menuOpen_ = true;
}

void ModuleShell::closeMenu() {
    menuOpen_ = false;
    menuStack_.clear();
    menuStack_.push_back(MenuLevel{&menuRoot_, 0, 0});
}

void ModuleShell::handleEncoderRotate(int delta) {
    if (!menuOpen_) {
        if (ScreenRegistry::hasActiveScreen()) {
            ScreenRegistry::handleEncoderRotate(delta);
        }
        return;
    }

    MenuLevel& level = menuStack_.back();
    auto visible = collectVisibleChildren(*level.node);
    if (visible.empty()) {
        level.focus = 0;
        level.scroll = 0;
        return;
    }

    level.focus += delta;
    if (level.focus < 0) {
        level.focus = 0;
    } else if (static_cast<size_t>(level.focus) >= visible.size()) {
        level.focus = static_cast<int>(visible.size()) - 1;
    }

    constexpr int maxVisible = 4;
    if (level.focus < level.scroll) {
        level.scroll = level.focus;
    } else if (level.focus >= level.scroll + maxVisible) {
        level.scroll = level.focus - (maxVisible - 1);
    }
}

void ModuleShell::handleEncoderPress() {
    if (ScreenRegistry::hasActiveScreen() && !menuOpen_) {
        ScreenRegistry::handleEncoderPress();
        return;
    }

    if (!menuOpen_) {
        openMenu();
        return;
    }

    MenuLevel& level = menuStack_.back();
    auto visible = collectVisibleChildren(*level.node);
    if (visible.empty()) {
        return;
    }

    ModuleMenuItem* item = visibleChildAt(*level.node, level.focus);
    if (item == nullptr) {
        return;
    }

    switch (item->type) {
        case ModuleMenuItem::Type::Submenu:
            menuStack_.push_back(MenuLevel{item, 0, 0});
            break;
        case ModuleMenuItem::Type::Toggle:
            if (item->onSelect) {
                item->onSelect();
            }
            break;
        case ModuleMenuItem::Type::Action:
        case ModuleMenuItem::Type::Screen:
            if (item->onSelect) {
                item->onSelect();
            }
            closeMenu();
            break;
    }
}

void ModuleShell::handleButton3() {
    if (menuOpen_) {
        if (menuStack_.size() > 1) {
            menuStack_.pop_back();
        } else {
            closeMenu();
        }
        return;
    }

    if (ScreenRegistry::hasActiveScreen()) {
        if (!ScreenRegistry::back()) {
            ScreenRegistry::clearStack();
        }
        return;
    }

    if (activeModule_ != nullptr) {
        activeModule_->onFunctionButton(2);
    }
}

void ModuleShell::drawMenuOverlay(DisplayCanvas& canvas) {
    if (menuStack_.empty()) {
        return;
    }

    MenuLevel& level = menuStack_.back();
    auto visible = collectVisibleChildren(*level.node);

    canvas.setDrawColor(1);
    canvas.drawRect(0, 0, canvas.getWidth(), canvas.getHeight(), true);
    canvas.setDrawColor(0);
    canvas.drawRect(2, 2, canvas.getWidth() - 4, canvas.getHeight() - 4, true);
    canvas.setDrawColor(1);

    const char* title = activeModule_ ? activeModule_->getModuleName() : "Module";
    const char* breadcrumb = nullptr;
    if (!menuStack_.empty() && menuStack_.back().node != &menuRoot_) {
        breadcrumb = menuStack_.back().node->label.c_str();
    }

    UIComponents::drawModernHeader(canvas,
                                   title,
                                   ICON_MENU,
                                   0,
                                   0,
                                   true);

    if (breadcrumb) {
        UIComponents::drawBreadcrumb(canvas,
                                     breadcrumb,
                                     UIComponents::HEADER_HEIGHT + 2,
                                     true);
    }

    std::vector<UIComponents::MenuItem> items;
    items.reserve(visible.size());
    for (ModuleMenuItem* child : visible) {
        UIComponents::MenuItem ui{};
        ui.icon = child->icon;
        ui.label = child->label.c_str();
        ui.value = child->value ? child->value() : nullptr;
        ui.hasSubmenu = (child->type == ModuleMenuItem::Type::Submenu);
        ui.isToggle = (child->type == ModuleMenuItem::Type::Toggle);
        ui.toggleState = ui.isToggle && child->toggleState ? child->toggleState() : false;
        ui.isReadOnly = false;
        items.push_back(ui);
    }

    constexpr int contentY = UIComponents::HEADER_HEIGHT + UIComponents::BREADCRUMB_HEIGHT + 4;
    UIComponents::drawModernMenu(canvas,
                                 items.data(),
                                 items.size(),
                                 level.focus,
                                 level.scroll,
                                 contentY);

    UIComponents::drawModernFooter(canvas,
                                   "Select",
                                   "Enter",
                                   "Back",
                                   nullptr,
                                   true);
}


ModuleMenuItem* ModuleShell::currentMenuNode() {
    if (menuStack_.empty()) {
        return nullptr;
    }
    return menuStack_.back().node;
}

std::vector<ModuleMenuItem*> ModuleShell::collectVisibleChildren(ModuleMenuItem& node) {
    std::vector<ModuleMenuItem*> result;
    for (auto& child : node.children) {
        if (child.condition && !child.condition()) {
            continue;
        }
        result.push_back(&child);
    }
    return result;
}

ModuleMenuItem* ModuleShell::visibleChildAt(ModuleMenuItem& node, int index) {
    auto visible = collectVisibleChildren(node);
    if (index < 0 || static_cast<size_t>(index) >= visible.size()) {
        return nullptr;
    }
    return visible[index];
}



