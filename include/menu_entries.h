#pragma once

#include <Arduino.h>

enum class GlobalMenuAction : uint8_t {
  Dashboards,
  Home,
  Pairing,
  Log,
  Configurations,
  Telemetry,
  PID,
  PacketVariables,
  Modes,
  Back,
};

struct MenuEntry {
  const char* label;
  GlobalMenuAction action;
};

// Fills the provided array with the current set of menu entries.
// Returns the number of valid entries written (up to maxEntries).
int buildGlobalMenuEntries(MenuEntry* entries, int maxEntries);
