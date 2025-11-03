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
extern bool genericConfigActive;
extern int genericConfigIndex;
extern int globalMenuIndex;
extern int globalMenuScrollOffset;
extern int dashboardFocusIndex;
extern int logScrollOffset;

constexpr byte DISPLAY_MODE_HOME = 0;
constexpr byte DISPLAY_MODE_TELEMETRY = 1;
constexpr byte DISPLAY_MODE_PID = 2;
constexpr byte DISPLAY_MODE_ORIENTATION = 3;
constexpr byte DISPLAY_MODE_PAIRING = 4;
constexpr byte DISPLAY_MODE_DASHBOARD = 5;
constexpr byte DISPLAY_MODE_ABOUT = 6;
constexpr byte DISPLAY_MODE_PEER_INFO = 7;
constexpr byte DISPLAY_MODE_GLOBAL_MENU = 8;
constexpr byte DISPLAY_MODE_LOG = 9;
constexpr byte DISPLAY_MODE_PACKET = 10;
constexpr byte DISPLAY_MODE_MODES = 11;

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

constexpr uint8_t PID_FOCUS_AXIS = 0;
constexpr uint8_t PID_FOCUS_KP = 1;
constexpr uint8_t PID_FOCUS_KI = 2;
constexpr uint8_t PID_FOCUS_KD = 3;
constexpr uint8_t PID_FOCUS_STEP = 4;
constexpr uint8_t PID_FOCUS_COUNT = 5;
void drawPairingMenu();
void drawPeerInfo();
void drawHomeMenu();
void drawHomeFooter();
void drawDashboard();
void drawThegillDashboard();
void drawGenericDashboard();
void drawBulkyDashboard();
void drawPacketVariables();
void drawModeSummary();
void drawConnectionLog();
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

// Returns the maximum scroll offset for the connection log based on the
// currently stored entries and display dimensions.
int getLogMaxScrollOffset();

// Utility helpers
void drawHeader(const char* title);

// Currently selected PID graph index (0=pitch,1=roll,2=yaw)
struct PidGains {
    float kp;
    float ki;
    float kd;
};

extern PidGains pidGains[PID_AXIS_COUNT];
extern bool pidGainsValid[PID_AXIS_COUNT];
extern int pidTunerAxisIndex;
extern uint8_t pidFocusIndex;
extern bool pidCoarseMode;
