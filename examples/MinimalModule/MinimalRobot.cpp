/**
 * @file MinimalRobot.cpp
 * @brief Minimal ILITE Module Implementation
 *
 * This file shows how to register your module with the ILITE framework.
 * The REGISTER_MODULE macro is all that's needed!
 */

#include "MinimalRobot.h"

// ============================================================================
// Module Registration - This is all you need!
// ============================================================================

/**
 * This single line:
 * 1. Creates a static instance of MinimalRobotModule
 * 2. Registers it with ModuleRegistry before main() runs
 * 3. Makes it discoverable by the ILITE framework
 *
 * That's it! No manual array editing, no include guards, no complex setup.
 */
REGISTER_MODULE(MinimalRobotModule);

// ============================================================================
// Done! Module is now part of the ILITE system.
// ============================================================================

/*
 * What happens at startup:
 *
 * 1. Static initialization runs (before setup())
 * 2. REGISTER_MODULE creates module instance
 * 3. Instance added to ModuleRegistry
 * 4. ILITE.begin() calls ModuleRegistry::initializeAll()
 * 5. Module's onInit() is called
 * 6. Module appears in home screen automatically
 * 7. When device pairs, framework routes packets to your module
 *
 * To use this module in your project:
 * - Add MinimalRobot.h and MinimalRobot.cpp to your project
 * - Include <ILITE.h> in main.cpp
 * - Call ILITE.begin() in setup()
 * - Call ILITE.update() in loop()
 * - Done! Module automatically discovered and loaded.
 */
