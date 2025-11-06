#include "ModuleMenu.h"

#include <algorithm>

ModuleMenuBuilder::ModuleMenuBuilder(ModuleMenuItem& root)
    : root_(root) {
    clear();
}

void ModuleMenuBuilder::clear() {
    root_.id = "root";
    root_.label.clear();
    root_.icon = nullptr;
    root_.type = ModuleMenuItem::Type::Submenu;
    root_.priority = 0;
    root_.onSelect = nullptr;
    root_.toggleState = nullptr;
    root_.condition = nullptr;
    root_.value = nullptr;
    root_.children.clear();
}

ModuleMenuItem& ModuleMenuBuilder::addAction(const std::string& id,
                                             const std::string& label,
                                             std::function<void()> onSelect,
                                             IconID icon,
                                             ModuleMenuItem* parent,
                                             int priority,
                                             std::function<bool()> condition,
                                             std::function<const char*()> value) {
    ModuleMenuItem item;
    item.id = id;
    item.label = label;
    item.icon = icon;
    item.type = ModuleMenuItem::Type::Action;
    item.priority = priority;
    item.onSelect = std::move(onSelect);
    item.condition = std::move(condition);
    item.value = std::move(value);
    return appendItem(resolveParent(parent), std::move(item));
}

ModuleMenuItem& ModuleMenuBuilder::addToggle(const std::string& id,
                                             const std::string& label,
                                             std::function<void()> onToggle,
                                             std::function<bool()> getState,
                                             IconID icon,
                                             ModuleMenuItem* parent,
                                             int priority,
                                             std::function<bool()> condition) {
    ModuleMenuItem item;
    item.id = id;
    item.label = label;
    item.icon = icon;
    item.type = ModuleMenuItem::Type::Toggle;
    item.priority = priority;
    item.onSelect = std::move(onToggle);
    item.toggleState = std::move(getState);
    item.condition = std::move(condition);
    return appendItem(resolveParent(parent), std::move(item));
}

ModuleMenuItem& ModuleMenuBuilder::addSubmenu(const std::string& id,
                                              const std::string& label,
                                              IconID icon,
                                              ModuleMenuItem* parent,
                                              int priority,
                                              std::function<bool()> condition) {
    ModuleMenuItem item;
    item.id = id;
    item.label = label;
    item.icon = icon;
    item.type = ModuleMenuItem::Type::Submenu;
    item.priority = priority;
    item.condition = std::move(condition);
    return appendItem(resolveParent(parent), std::move(item));
}

ModuleMenuItem& ModuleMenuBuilder::root() {
    return root_;
}

ModuleMenuItem& ModuleMenuBuilder::resolveParent(ModuleMenuItem* parent) {
    if (parent != nullptr) {
        return *parent;
    }
    return root_;
}

ModuleMenuItem& ModuleMenuBuilder::appendItem(ModuleMenuItem& parent, ModuleMenuItem item) {
    parent.children.emplace_back(std::move(item));

    // Keep children sorted by priority then label for stable ordering.
    std::sort(parent.children.begin(), parent.children.end(),
              [](const ModuleMenuItem& a, const ModuleMenuItem& b) {
                  if (a.priority == b.priority) {
                      return a.label < b.label;
                  }
                  return a.priority < b.priority;
              });

    return parent.children.back();
}

