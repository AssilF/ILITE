#pragma once

/**
 * @file ModuleMenu.h
 * @brief Module-scoped menu structures and builder utilities.
 *
 * The module menu system lets each ILITE module describe its own hierarchical
 * menu entries (actions, toggles, sub pages) that are rendered by the
 * ModuleShell when the module menu is opened from the encoder button.
 *
 * Menus are defined in a lightweight tree of ModuleMenuItem nodes. Modules can
 * populate the tree using ModuleMenuBuilder helpers which take care of wiring
 * up icons, callbacks, and optional visibility predicates.
 *
 * Typical usage inside a module:
 *
 * ```cpp
 * void TheGillModule::buildModuleMenu(ModuleMenuBuilder& menu) {
 *     auto& modes = menu.addSubmenu("tg.modes", "Modes", ICON_GRID);
 *     menu.addAction("tg.default", "Default Mode", [this]() { setMode(Default); }, ICON_PLAY, &modes);
 *     menu.addAction("tg.mechiane", "Mech'Iane Mode", [this]() { showMechIane(); }, ICON_ROBOT, &modes);
 *     menu.addToggle("tg.precision", "Precision Mode",
 *                    [this]() { togglePrecision(); },
 *                    [this]() { return isPrecisionEnabled(); },
 *                    ICON_TARGET);
 * }
 * ```
 */

#include <functional>
#include <string>
#include <vector>

#include "IconLibrary.h"

/**
 * @brief A single module menu entry.
 */
struct ModuleMenuItem {
    enum class Type {
        Action,     ///< Invoke callback when selected
        Toggle,     ///< Toggle state, shows checkmark
        Screen,     ///< Opens a custom screen via callback
        Submenu,    ///< Contains nested children
        EditableInt,    ///< Editable integer value
        EditableFloat   ///< Editable float value
    };

    std::string id;                       ///< Unique identifier within module
    std::string label;                    ///< Display label
    IconID icon = nullptr;                ///< Optional icon
    Type type = Type::Action;             ///< Item kind
    int priority = 0;                     ///< Sorting priority (lower first)

    std::function<void()> onSelect;       ///< Selection handler ( Actions / Screens / Toggles )
    std::function<bool()> toggleState;    ///< Toggle state provider (Toggle only)
    std::function<bool()> condition;      ///< Visibility predicate (optional)
    std::function<const char*()> value;   ///< Optional value renderer

    // Editable value support
    std::function<int()> getIntValue;     ///< Get integer value (EditableInt only)
    std::function<void(int)> setIntValue; ///< Set integer value (EditableInt only)
    std::function<float()> getFloatValue; ///< Get float value (EditableFloat only)
    std::function<void(float)> setFloatValue; ///< Set float value (EditableFloat only)
    int minValue = 0;                     ///< Minimum value for editing
    int maxValue = 100;                   ///< Maximum value for editing
    float minValueFloat = 0.0f;           ///< Minimum float value for editing
    float maxValueFloat = 100.0f;         ///< Maximum float value for editing
    float step = 1.0f;                    ///< Step size for editing
    float coarseStep = 10.0f;             ///< Coarse step size for fast editing

    std::vector<ModuleMenuItem> children; ///< Nested entries (Submenu only)
};

/**
 * @brief Builder utility that helps modules populate their menu tree.
 *
 * The builder owns no storage – it directly writes into the supplied root
 * ModuleMenuItem. Returned references remain valid for the lifetime of the
 * root tree.
 */
class ModuleMenuBuilder {
public:
    explicit ModuleMenuBuilder(ModuleMenuItem& root);

    /**
     * @brief Clear previous menu contents.
     */
    void clear();

    /**
     * @brief Add an action entry.
     *
     * @param id        Unique identifier (e.g. "tg.telemetry.reset")
     * @param label     Display label
     * @param onSelect  Callback executed on selection
     * @param icon      Optional icon
     * @param parent    Parent submenu (nullptr = root)
     * @param priority  Sorting priority (lower values appear first)
     * @param condition Optional visibility predicate
     * @param value     Optional value renderer for right-aligned text
     * @return Reference to the created item (for further nesting if desired)
     */
    ModuleMenuItem& addAction(const std::string& id,
                              const std::string& label,
                              std::function<void()> onSelect,
                              IconID icon = nullptr,
                              ModuleMenuItem* parent = nullptr,
                              int priority = 0,
                              std::function<bool()> condition = nullptr,
                              std::function<const char*()> value = nullptr);

    /**
     * @brief Add a toggle entry.
     *
     * @param id        Unique identifier
     * @param label     Display label
     * @param onToggle  Callback executed when user toggles (should flip state)
     * @param getState  Returns current state
     * @param icon      Optional icon
     * @param parent    Parent submenu (nullptr = root)
     * @param priority  Sorting priority
     * @param condition Optional visibility predicate
     * @return Reference to the created item
     */
    ModuleMenuItem& addToggle(const std::string& id,
                              const std::string& label,
                              std::function<void()> onToggle,
                              std::function<bool()> getState,
                              IconID icon = nullptr,
                              ModuleMenuItem* parent = nullptr,
                              int priority = 0,
                              std::function<bool()> condition = nullptr);

    /**
     * @brief Add a submenu entry.
     *
     * @param id        Unique identifier
     * @param label     Display label
     * @param icon      Optional icon
     * @param parent    Parent submenu (nullptr = root)
     * @param priority  Sorting priority
     * @param condition Optional visibility predicate
     * @return Reference to the newly created submenu (use it as parent)
     */
    ModuleMenuItem& addSubmenu(const std::string& id,
                               const std::string& label,
                               IconID icon = nullptr,
                               ModuleMenuItem* parent = nullptr,
                               int priority = 0,
                               std::function<bool()> condition = nullptr);

    /**
     * @brief Add an editable integer value entry.
     *
     * @param id        Unique identifier
     * @param label     Display label
     * @param getValue  Function to get current value
     * @param setValue  Function to set new value
     * @param minVal    Minimum value
     * @param maxVal    Maximum value
     * @param step      Fine adjustment step
     * @param coarseStep Coarse adjustment step (for fast rotation)
     * @param icon      Optional icon
     * @param parent    Parent submenu (nullptr = root)
     * @param priority  Sorting priority
     * @return Reference to the created item
     */
    ModuleMenuItem& addEditableInt(const std::string& id,
                                    const std::string& label,
                                    std::function<int()> getValue,
                                    std::function<void(int)> setValue,
                                    int minVal,
                                    int maxVal,
                                    int step = 1,
                                    int coarseStep = 10,
                                    IconID icon = nullptr,
                                    ModuleMenuItem* parent = nullptr,
                                    int priority = 0);

    /**
     * @brief Add an editable float value entry.
     *
     * @param id        Unique identifier
     * @param label     Display label
     * @param getValue  Function to get current value
     * @param setValue  Function to set new value
     * @param minVal    Minimum value
     * @param maxVal    Maximum value
     * @param step      Fine adjustment step
     * @param coarseStep Coarse adjustment step (for fast rotation)
     * @param icon      Optional icon
     * @param parent    Parent submenu (nullptr = root)
     * @param priority  Sorting priority
     * @return Reference to the created item
     */
    ModuleMenuItem& addEditableFloat(const std::string& id,
                                      const std::string& label,
                                      std::function<float()> getValue,
                                      std::function<void(float)> setValue,
                                      float minVal,
                                      float maxVal,
                                      float step = 0.1f,
                                      float coarseStep = 1.0f,
                                      IconID icon = nullptr,
                                      ModuleMenuItem* parent = nullptr,
                                      int priority = 0);

    /**
     * @brief Access the underlying root item.
     */
    ModuleMenuItem& root();

private:
    ModuleMenuItem& resolveParent(ModuleMenuItem* parent);
    ModuleMenuItem& appendItem(ModuleMenuItem& parent, ModuleMenuItem item);

    ModuleMenuItem& root_;
};

