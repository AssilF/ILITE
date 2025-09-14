#pragma once
#include <Arduino.h>
#include <u8g2lib.h>
#include "espnow_discovery.h"
#include "telemetry.h"
#include "input.h"

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
extern bool pairedIsBulky;

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
void drawAbout();
void drawBootScreen();
void irData();
void Line_detection();
void Pid_Tuner();
void Fire_detection();
void displayMenu();

// Utility helpers
void drawHeader(const char* title);

// Currently selected PID graph index (0=pitch,1=roll,2=yaw)
extern int pidGraphIndex;
