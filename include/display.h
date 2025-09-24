#pragma once
#include <Arduino.h>
#include <u8g2lib.h>
#include "espnow_discovery.h"
#include "telemetry.h"
#include "input.h"
#include "ui_modules.h"

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C oled;
extern EspNowDiscovery discovery;

void initDisplay();

extern byte displayMode;
extern int homeMenuIndex;
extern bool homeSelected;
extern int selectedPeer;
extern int lastEncoderCount;
extern unsigned long lastDiscoveryTime;
extern int infoPeer;
extern int gillConfigIndex;
extern int globalMenuIndex;
extern int dashboardFocusIndex;

extern byte batteryLevel;
extern byte Front_Distance;
extern byte Bottom_Distance;
extern byte speed;
extern byte speedTarget;
extern byte linePosition;
extern byte firePose;

void drawStatus();
void drawMenu();
void drawBattery();
void drawGyroLevel();
void drawCompassStatus();
void drawProximity();
void drawSpeed();
void drawLine();
void drawFirePosition();
void tirminal();
void drawMotionJoystickPose();
void drawPeripheralJoystickPose();
void drawDrongazInterface();
void drawTelemetryInfo();
// Draw a single PID graph based on the currently selected axis
void drawPidGraph();
void drawOrientationCube();
void drawPairingMenu();
void drawPeerInfo();
void drawHomeMenu();
void drawHomeFooter();
void drawDashboard();
void drawThegillDashboard();
void drawGenericDashboard();
void drawBulkyDashboard();
void drawAbout();
void drawBootScreen();
void drawThegillConfig();
void drawGlobalMenu();
void drawLayoutCardFrame(int16_t x, int16_t y, const char* title, const char* subtitle, bool focused);
void drawGenericLayoutCard(const ModuleState& state, int16_t x, int16_t y, bool focused);
void drawBulkyLayoutCard(const ModuleState& state, int16_t x, int16_t y, bool focused);
void drawThegillLayoutCard(const ModuleState& state, int16_t x, int16_t y, bool focused);
void irData();
void Line_detection();
void Pid_Tuner();
void Fire_detection();
void displayMenu();

// Utility helpers
void drawHeader(const char* title);

// Currently selected PID graph index (0=pitch,1=roll,2=yaw)
extern int pidGraphIndex;
