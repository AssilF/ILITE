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

ModuleMenuItem& ModuleMenuBuilder::addEditableInt(const std::string& id,
                                                   const std::string& label,
                                                   std::function<int()> getValue,
                                                   std::function<void(int)> setValue,
                                                   int minVal,
                                                   int maxVal,
                                                   int step,
                                                   int coarseStep,
                                                   IconID icon,
                                                   ModuleMenuItem* parent,
                                                   int priority) {
    ModuleMenuItem item;
    item.id = id;
    item.label = label;
    item.icon = icon;
    item.type = ModuleMenuItem::Type::EditableInt;
    item.priority = priority;
    item.getIntValue = std::move(getValue);
    item.setIntValue = std::move(setValue);
    item.minValue = minVal;
    item.maxValue = maxVal;
    item.step = static_cast<float>(step);
    item.coarseStep = static_cast<float>(coarseStep);
    return appendItem(resolveParent(parent), std::move(item));
}

ModuleMenuItem& ModuleMenuBuilder::addEditableFloat(const std::string& id,
                                                     const std::string& label,
                                                     std::function<float()> getValue,
                                                     std::function<void(float)> setValue,
                                                     float minVal,
                                                     float maxVal,
                                                     float step,
                                                     float coarseStep,
                                                     IconID icon,
                                                     ModuleMenuItem* parent,
                                                     int priority) {
    ModuleMenuItem item;
    item.id = id;
    item.label = label;
    item.icon = icon;
    item.type = ModuleMenuItem::Type::EditableFloat;
    item.priority = priority;
    item.getFloatValue = std::move(getValue);
    item.setFloatValue = std::move(setValue);
    item.minValueFloat = minVal;
    item.maxValueFloat = maxVal;
    item.step = step;
    item.coarseStep = coarseStep;
    return appendItem(resolveParent(parent), std::move(item));
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

