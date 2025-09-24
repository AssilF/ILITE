#pragma once
#include <Arduino.h>

enum class PeerKind : uint8_t {
  None,
  Generic,
  Bulky,
  Thegill
};

struct ModuleState;

struct FunctionActionOption {
  const char* name;
  const char* shortLabel;
  void (*invoke)(ModuleState& state, size_t slot);
};

struct ModuleDescriptor {
  PeerKind kind;
  const char* displayName;
  void (*drawDashboard)();
  void (*drawLayoutCard)(const ModuleState& state, int16_t x, int16_t y, bool focused);
  void (*handleIncoming)(ModuleState& state, const uint8_t* data, size_t length);
  void (*updateControl)();
  const uint8_t* (*preparePayload)(size_t& size);
  const FunctionActionOption* functionOptions;
  size_t functionOptionCount;
};

struct ModuleState {
  const ModuleDescriptor* descriptor;
  bool wifiEnabled;
  uint8_t assignedActions[3];
  bool functionOutputs[3];
};

size_t getModuleStateCount();
ModuleState* getModuleState(size_t index);
ModuleState* findModuleByKind(PeerKind kind);
ModuleState* findModuleByName(const char* name);
ModuleState* getActiveModule();
void setActiveModule(ModuleState* state);
const FunctionActionOption* getAssignedAction(const ModuleState& state, size_t slot);
void cycleAssignedAction(ModuleState& state, size_t slot, int delta);
void setFunctionOutput(ModuleState& state, size_t slot, bool active);
bool getFunctionOutput(const ModuleState& state, size_t slot);
void toggleModuleWifi(ModuleState& state);
