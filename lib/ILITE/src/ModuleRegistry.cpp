/**
 * @file ModuleRegistry.cpp
 * @brief Module Registration System Implementation
 */

#include "ModuleRegistry.h"
#include <cstring>
#include <cctype>

// Static storage for registered modules
static std::vector<ILITEModule*> g_modules;
static bool g_initialized = false;

void ModuleRegistry::registerModule(ILITEModule* module) {
    if (module == nullptr) {
        return;
    }

    // Check for duplicate IDs
    const char* newId = module->getModuleId();
    for (ILITEModule* existing : g_modules) {
        if (strcmp(existing->getModuleId(), newId) == 0) {
            Serial.printf("[ModuleRegistry] ERROR: Duplicate module ID: %s\n", newId);
            return;
        }
    }

    g_modules.push_back(module);
    Serial.printf("[ModuleRegistry] Registered module: %s (%s)\n",
                  module->getModuleName(), module->getModuleId());
}

std::vector<ILITEModule*>& ModuleRegistry::getModules() {
    return g_modules;
}

size_t ModuleRegistry::getModuleCount() {
    return g_modules.size();
}

ILITEModule* ModuleRegistry::findModuleByName(const char* deviceName) {
    if (deviceName == nullptr || deviceName[0] == '\0') {
        return nullptr;
    }

    // Convert device name to lowercase for comparison
    char lowerName[128];
    size_t nameLen = strlen(deviceName);
    if (nameLen >= sizeof(lowerName)) {
        nameLen = sizeof(lowerName) - 1;
    }

    for (size_t i = 0; i < nameLen; ++i) {
        lowerName[i] = tolower(static_cast<unsigned char>(deviceName[i]));
    }
    lowerName[nameLen] = '\0';

    // Search all modules for keyword match
    for (ILITEModule* module : g_modules) {
        const char** keywords = module->getDetectionKeywords();
        size_t keywordCount = module->getDetectionKeywordCount();

        if (keywords == nullptr || keywordCount == 0) {
            continue;
        }

        for (size_t i = 0; i < keywordCount; ++i) {
            const char* keyword = keywords[i];
            if (keyword == nullptr) {
                continue;
            }

            // Check if device name contains keyword (case-insensitive)
            if (strstr(lowerName, keyword) != nullptr) {
                Serial.printf("[ModuleRegistry] Device '%s' matched module '%s' (keyword: %s)\n",
                              deviceName, module->getModuleName(), keyword);
                return module;
            }
        }
    }

    Serial.printf("[ModuleRegistry] No module matched device name: %s\n", deviceName);
    return nullptr;
}

ILITEModule* ModuleRegistry::findModuleById(const char* moduleId) {
    if (moduleId == nullptr) {
        return nullptr;
    }

    for (ILITEModule* module : g_modules) {
        if (strcmp(module->getModuleId(), moduleId) == 0) {
            return module;
        }
    }

    return nullptr;
}

ILITEModule* ModuleRegistry::getModuleByIndex(size_t index) {
    if (index >= g_modules.size()) {
        return nullptr;
    }
    return g_modules[index];
}

void ModuleRegistry::initializeAll() {
    if (g_initialized) {
        Serial.println("[ModuleRegistry] WARNING: Modules already initialized");
        return;
    }

    Serial.printf("[ModuleRegistry] Initializing %zu modules...\n", g_modules.size());

    for (ILITEModule* module : g_modules) {
        Serial.printf("[ModuleRegistry]   - %s (%s) by %s\n",
                      module->getModuleName(),
                      module->getVersion(),
                      module->getAuthor() ? module->getAuthor() : "Unknown");

        module->onInit();
        module->initialized_ = true;
    }

    g_initialized = true;
    Serial.println("[ModuleRegistry] All modules initialized successfully");
}
