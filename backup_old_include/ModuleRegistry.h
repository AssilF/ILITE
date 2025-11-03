/**
 * @file ModuleRegistry.h
 * @brief ILITE Framework - Module Registration System
 *
 * Provides automatic module discovery and registration at compile-time.
 * Users simply add REGISTER_MODULE(MyModuleClass) to their .cpp file.
 *
 * @author ILITE Team
 * @date 2025
 */

#pragma once
#include "ILITEModule.h"
#include <vector>

/**
 * @brief Central registry for all ILITE modules
 *
 * Modules self-register at compile-time via static initialization using
 * the REGISTER_MODULE macro. The framework discovers modules automatically
 * without requiring users to manually add them to arrays or lists.
 *
 * ## How It Works
 * 1. User creates class: `class MyRobot : public ILITEModule { ... };`
 * 2. User adds to .cpp: `REGISTER_MODULE(MyRobot);`
 * 3. Static initializer runs before main()
 * 4. Module instance registered in global registry
 * 5. Framework discovers all modules via `ModuleRegistry::getModules()`
 *
 * ## Thread Safety
 * Registration must complete before ILITE.begin() is called (single-threaded).
 * After begin(), the registry is read-only (thread-safe).
 */
class ModuleRegistry {
public:
    /**
     * @brief Register a module instance
     *
     * Called automatically by REGISTER_MODULE macro. Do not call directly.
     *
     * @param module Pointer to module instance (must remain valid for program lifetime)
     */
    static void registerModule(ILITEModule* module);

    /**
     * @brief Get all registered modules
     *
     * @return Vector of module pointers
     */
    static std::vector<ILITEModule*>& getModules();

    /**
     * @brief Get number of registered modules
     *
     * @return Module count
     */
    static size_t getModuleCount();

    /**
     * @brief Find module by device name (auto-detection during pairing)
     *
     * Searches all modules' detection keywords for a match against the
     * device name. Used for automatic module selection when pairing.
     *
     * Case-insensitive substring search.
     *
     * Example:
     * - Device name: "MyRobot-Alpha"
     * - Module keywords: ["myrobot", "robotalpha"]
     * - Match: Yes (contains "myrobot")
     *
     * @param deviceName Device custom ID string from ESP-NOW identity
     * @return Pointer to matching module, or nullptr if no match
     */
    static ILITEModule* findModuleByName(const char* deviceName);

    /**
     * @brief Find module by unique ID
     *
     * @param moduleId Module ID (e.g., "com.example.myrobot")
     * @return Pointer to module, or nullptr if not found
     */
    static ILITEModule* findModuleById(const char* moduleId);

    /**
     * @brief Get module by index
     *
     * @param index Module index (0 to getModuleCount()-1)
     * @return Pointer to module, or nullptr if index out of range
     */
    static ILITEModule* getModuleByIndex(size_t index);

    /**
     * @brief Initialize all registered modules
     *
     * Calls onInit() for each module. Called once by framework at startup.
     * Users should not call this directly.
     */
    static void initializeAll();

private:
    ModuleRegistry() = delete;  // Static class, no instances
};

/**
 * @brief Macro to register a module automatically
 *
 * Add this line to your module's .cpp file (outside any function):
 *
 * ```cpp
 * // MyRobot.cpp
 * #include "MyRobot.h"
 *
 * REGISTER_MODULE(MyRobotModule);  // <- Add this
 *
 * void MyRobotModule::updateControl(...) {
 *     // Implementation
 * }
 * ```
 *
 * ## How It Works
 * 1. Creates a static instance of your module class
 * 2. Creates a static bool that runs an initializer lambda
 * 3. Lambda registers the module instance with ModuleRegistry
 * 4. Static initialization runs before main()
 * 5. Your module is discovered automatically
 *
 * ## Memory Management
 * The module instance is statically allocated (lives for entire program).
 * No dynamic allocation, no cleanup needed.
 *
 * @param ModuleClass Your module class name (not a string)
 */
#define REGISTER_MODULE(ModuleClass) \
    static ModuleClass g_##ModuleClass##_instance; \
    namespace { \
        struct ModuleClass##_Registrar { \
            ModuleClass##_Registrar() { \
                ModuleRegistry::registerModule(&g_##ModuleClass##_instance); \
            } \
        }; \
        static ModuleClass##_Registrar g_##ModuleClass##_registrar; \
    }

/**
 * @brief Helper macro for creating detection keywords array
 *
 * Simplifies keyword array creation:
 *
 * ```cpp
 * const char** MyRobot::getDetectionKeywords() const {
 *     DETECTION_KEYWORDS("myrobot", "my-bot", "robotv2");
 * }
 * ```
 *
 * Expands to:
 * ```cpp
 * static const char* keywords[] = {"myrobot", "my-bot", "robotv2"};
 * return keywords;
 * ```
 */
#define DETECTION_KEYWORDS(...) \
    static const char* keywords[] = {__VA_ARGS__}; \
    return keywords

/**
 * @brief Helper macro for keyword count
 *
 * Use with DETECTION_KEYWORDS:
 *
 * ```cpp
 * size_t MyRobot::getDetectionKeywordCount() const {
 *     DETECTION_KEYWORD_COUNT("myrobot", "my-bot", "robotv2");
 * }
 * ```
 */
#define DETECTION_KEYWORD_COUNT(...) \
    return (sizeof((const char*[]){__VA_ARGS__}) / sizeof(const char*))
