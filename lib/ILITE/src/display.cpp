#include "display.h"
#include "thegill.h"
#include "connection_log.h"
#include "menu_entries.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0);

#define iconFont u8g2_font_open_iconic_all_2x_t
#define networkBatteryIconFont u8g2_font_siji_t_6x10
#define textFont u8g2_font_torussansbold8_8r
#define smallFont u8g2_font_5x8_tf
#define bootTitleFont u8g2_font_inb38_mr
#define bootSubFont u8g2_font_6x13_tf
#define smallIconFont u8g2_font_open_iconic_all_1x_t
#define statusFont u8g2_font_5x8_tf
#define tinyFont u8g2_font_4x6_tf

constexpr int16_t kStatusFontHeight = 8;
constexpr int16_t kStatusBarHeight = kStatusFontHeight + 4;

byte displayMode = DISPLAY_MODE_LOG;
int homeMenuIndex = 0;
bool homeSelected = false;
int selectedPeer = 0;
int lastEncoderCount = 0;
unsigned long lastDiscoveryTime = 0;
int infoPeer = 0;
int gillConfigIndex = 0;
bool genericConfigActive = false;
int genericConfigIndex = 0;
int globalMenuIndex = 0;
int dashboardFocusIndex = 0;
int logScrollOffset = 0;
int globalMenuScrollOffset = 0;

constexpr int kGlobalMenuVisibleLines = 4;

extern bool autoDashboardEnabled;
extern ModuleState* lastPairedModule;
extern bool btnmode;

extern PidGains pidGains[PID_AXIS_COUNT];
extern bool pidGainsValid[PID_AXIS_COUNT];
extern int pidTunerAxisIndex;
extern uint8_t pidFocusIndex;
extern bool pidCoarseMode;
extern uint8_t droneStabilizationMask;
extern bool droneStabilizationGlobal;

namespace {
constexpr int kLogVisibleLines = 8;
constexpr int16_t kLogStartY = kStatusBarHeight + 2;
constexpr int16_t kLogLineHeight = 6;
constexpr size_t kLogMaxCharsPerLine = 32;
constexpr size_t kLogLineBufferSize = 80;

int mapHistoryValue(int16_t value, int16_t minValue, int16_t maxValue, int top, int bottom) {
  if (maxValue == minValue) {
    return top;
  }
  if (value < minValue) value = minValue;
  if (value > maxValue) value = maxValue;
  const int range = maxValue - minValue;
  const int span = bottom - top;
  if (span <= 0) {
    return top;
  }
  return bottom - static_cast<int>((static_cast<long>(value - minValue) * span) / range);
}

size_t nextWrappedSegment(const char* text, size_t start, size_t totalLength,
                          size_t* nextStart) {
  if (!text || start >= totalLength) {
    if (nextStart) {
      *nextStart = totalLength;
    }
    return 0;
  }
  size_t remaining = totalLength - start;
  size_t limit = remaining < kLogMaxCharsPerLine ? remaining : kLogMaxCharsPerLine;
  size_t lineLen = limit;
  if (remaining > kLogMaxCharsPerLine) {
    size_t breakPos = start + limit;
    bool foundSpace = false;
    for (size_t i = 0; i < kLogMaxCharsPerLine; ++i) {
      size_t pos = breakPos - 1 - i;
      if (isspace(static_cast<unsigned char>(text[pos]))) {
        lineLen = pos - start + 1;
        foundSpace = true;
        break;
      }
    }
    if (!foundSpace) {
      lineLen = limit;
    }
  }
  size_t copyLen = lineLen;
  while (copyLen > 0 && isspace(static_cast<unsigned char>(text[start + copyLen - 1]))) {
    --copyLen;
  }
  size_t advance = lineLen > 0 ? lineLen : 1;
  size_t newStart = start + advance;
  while (newStart < totalLength && isspace(static_cast<unsigned char>(text[newStart]))) {
    ++newStart;
  }
  if (nextStart) {
    *nextStart = newStart;
  }
  return copyLen;
}

size_t countWrappedLines(const char* text) {
  if (!text) {
    return 0;
  }
  if (text[0] == '\0') {
    return 1;
  }
  size_t totalLength = strlen(text);
  size_t start = 0;
  while (start < totalLength && isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  size_t count = 0;
  while (start < totalLength) {
    size_t nextStart = start;
    size_t copyLen = nextWrappedSegment(text, start, totalLength, &nextStart);
    ++count;
    if (nextStart <= start) {
      break;
    }
    start = nextStart;
  }
  return count > 0 ? count : 1;
}

bool extractWrappedLine(const char* text, size_t lineIndex, char* buffer, size_t bufferSize) {
  if (!buffer || bufferSize == 0) {
    return false;
  }
  buffer[0] = '\0';
  if (!text) {
    return false;
  }
  if (text[0] == '\0') {
    return lineIndex == 0;
  }
  size_t totalLength = strlen(text);
  size_t start = 0;
  while (start < totalLength && isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  if (start >= totalLength) {
    return lineIndex == 0;
  }
  size_t currentLine = 0;
  while (start < totalLength) {
    size_t nextStart = start;
    size_t copyLen = nextWrappedSegment(text, start, totalLength, &nextStart);
    if (currentLine == lineIndex) {
      if (copyLen >= bufferSize) {
        copyLen = bufferSize - 1;
      }
      memcpy(buffer, text + start, copyLen);
      buffer[copyLen] = '\0';
      return true;
    }
    ++currentLine;
    if (nextStart <= start) {
      break;
    }
    start = nextStart;
  }
  return false;
}

size_t getTotalWrappedLineCount() {
  size_t total = 0;
  size_t count = connectionLogGetCount();
  for (size_t i = 0; i < count; ++i) {
    const char* entry = connectionLogGetEntry(i);
    total += countWrappedLines(entry);
  }
  return total;
}

}  // namespace

int getLogMaxScrollOffset() {
  size_t totalLines = getTotalWrappedLineCount();
  if (totalLines > static_cast<size_t>(kLogVisibleLines)) {
    return static_cast<int>(totalLines) - kLogVisibleLines;
  }
  return 0;
}

static const char* profileToString(GillDriveProfile profile){
  switch(profile){
    case GillDriveProfile::Differential: return "Differential";
    case GillDriveProfile::Tank:
    default: return "Tank";
  }
}

static const char* easingToString(GillDriveEasing easing){
  switch(easing){
    case GillDriveEasing::None: return "None";
    case GillDriveEasing::SlewRate: return "Slew";
    case GillDriveEasing::Exponential:
    default: return "Exponential";
  }
}

void drawLayoutCardFrame(int16_t x, int16_t y, const char* title, const char* subtitle, bool focused){
  const int16_t width = screen_Width - x * 2;
  const int16_t height = 40;
  oled.setDrawColor(1);
  oled.drawRFrame(x, y, width, height, 6);
  if(focused){
    oled.drawRFrame(x + 1, y + 1, width - 2, height - 2, 6);
  }
  oled.setFont(textFont);
  oled.setCursor(x + 6, y + 14);
  oled.print(title);
  oled.setFont(smallFont);
  oled.setCursor(x + 6, y + 26);
  oled.print(subtitle);
}

static void drawWifiStatusBadge(const ModuleState& state, int16_t right, int16_t y){
  oled.setFont(smallFont);
  const char* label = state.wifiEnabled ? "WiFi On" : "WiFi Off";
  int16_t width = oled.getUTF8Width(label);
  int16_t cursor = right - width;
  if(cursor < 0) cursor = 0;
  oled.setCursor(cursor, y);
  oled.print(label);
}

void drawGenericLayoutCard(const ModuleState& state, int16_t x, int16_t y, bool focused){
  const ModuleState* active = getActiveModule();
  const bool isActive = active == &state;
  const int16_t width = screen_Width - x * 2;
  drawLayoutCardFrame(x, y, "DroneGaze", isActive ? "Active layout" : "Flight telemetry", focused);
  oled.setFont(smallIconFont);
  oled.setCursor(x + width / 2 - 5, y + 32);
  oled.print("\u00a5");
  drawWifiStatusBadge(state, x + width - 6, y + 14);
}

void drawBulkyLayoutCard(const ModuleState& state, int16_t x, int16_t y, bool focused){
  const ModuleState* active = getActiveModule();
  const bool isActive = active == &state;
  const int16_t width = screen_Width - x * 2;
  drawLayoutCardFrame(x, y, "Bulky", isActive ? "Active layout" : "Utility carrier", focused);
  oled.setFont(smallIconFont);
  oled.setCursor(x + width / 2 - 5, y + 32);
  oled.print("\u00f7");
  drawWifiStatusBadge(state, x + width - 6, y + 14);
}

void drawThegillLayoutCard(const ModuleState& state, int16_t x, int16_t y, bool focused){
  const ModuleState* active = getActiveModule();
  const bool isActive = active == &state;
  const int16_t width = screen_Width - x * 2;
  drawLayoutCardFrame(x, y, "The'gill", isActive ? "Active layout" : "Differential drive", focused);
  oled.setFont(smallIconFont);
  oled.setCursor(x + width / 2 - 5, y + 32);
  oled.print("\u00dd");
  drawWifiStatusBadge(state, x + width - 6, y + 14);
}

#define displayWidth 128
#define displayHeight 64
#define batteryCorner 64-25,5,50,20
#define iconHeight 16
#define iconWidth 16
#define iconClearance 1
#define iconFrameHeight iconHeight+2*iconClearance
#define iconFrameWidth iconWidth+2*iconClearance
#define iconDeckx (displayWidth-(displayWidth/iconFrameWidth))/2



#define compassIcon "\u0087"
#define speedIcon "\u010d"
#define recordPathIcon "\u00ac"

#define visionIcon "\u00a5"
#define avoidanceIcon "\u00f2"
#define radioIcon "\u00f7"
#define lineIcon "\u00dd"

#define fireIcon "\u00a8"
#define proximityIcon "\u0119"
#define alertIcon "\u0118"

#define pumpIcon "\u0098"
#define illegalIcon "\u0057"

#define sentIcon "\u0077"
#define checkIcon "\u0073"
#define receivedIcon "\u0074"

#define batteryDepletedIcon "\ue236"
#define batteryHalfIcon "\ue237"
#define batteryFullIcon "\ue238"

#define batteryNA "\ue211"
#define battery1 "\ue242"
#define battery2 "\ue243"
#define battery3 "\ue244"
#define battery4 "\ue245"
#define battery5 "\ue246"
#define battery6 "\ue247"
#define battery7 "\ue248"
#define battery8 "\ue249"
#define battery9 "\ue24a"
#define battery10 "\ue24b"

int menuIndex;
int settingsIndex;
String deckIcons[] = {};

byte batteryLevel;
byte Front_Distance;
byte Bottom_Distance;
byte speed;
byte speedTarget;
byte linePosition = B00001010;
byte firePose = 90;


int LM_Line=99;
int RM_Line=999;
int L_Line=99;
int R_Line=99;
int LM_Threshold;
int RM_Threshold;
int L_Threshold;
int R_Threshold;
int Line_Mode;
int LKp;
int LKd;
int Base_speed;
int IRBais;
int LFirsensor;
int RFirsensor;
int Detection_Tolerance;
int Detection_Distance;
byte menuCount = 1;
bool runState = false;
int screen = 1;

void initDisplay(){
  oled.begin();
  oled.setFont(textFont);
  drawBootScreen();
}

void drawHeader(const char* title){
  oled.setDrawColor(1);
  oled.drawBox(0,0,screen_Width,12);
  oled.setDrawColor(0);
  oled.setFont(textFont);
  oled.setCursor(2,10);
  oled.print(title);
  oled.setDrawColor(1);
}

void drawStatus(){
  oled.setDrawColor(2);
  oled.setFont(iconFont);
  oled.enableUTF8Print();
  oled.setCursor(iconDeckx,iconFrameHeight-1);
  oled.print(fireIcon);
  oled.setCursor(iconDeckx+iconFrameWidth-1,iconHeight-1);
  oled.print(pumpIcon);
  oled.setCursor(iconDeckx+2*(iconFrameWidth-1),iconHeight-1);
  oled.print(proximityIcon);
  oled.sendBuffer();
}

void drawMenu(){
}

void drawBattery(){
}

void drawGyroLevel(){
}

void drawCompassStatus(){
}

void drawProximity(){
  oled.setDrawColor(1);
  int16_t baseY = kStatusBarHeight + 22;
  oled.drawRFrame(14, baseY, 12, 28, 3);
  oled.drawBox(16, baseY + 21, 8, map(Front_Distance, 0, 50, 27, 0));
  oled.drawRFrame(1, baseY, 12, 28, 3);
  oled.drawBox(3, baseY + 21, 8, map(Bottom_Distance, 0, 50, 27, 0));
}

void drawSpeed(){
  oled.setFont(textFont);
  oled.setDrawColor(2);
  int16_t labelY = kStatusBarHeight + 44;
  oled.setCursor(56, labelY);
  oled.print(speed);
  int16_t frameY = kStatusBarHeight + 34;
  oled.drawRFrame(28, frameY, 71, 12, 4);
  oled.drawBox(30, frameY + 2, map(speed, 0, 100, 0, 67), 8);
  int16_t lineY = kStatusBarHeight + 47;
  oled.drawLine(32, lineY, map(analogRead(potA), 0, 4096, 32, 80), lineY);
}

void drawLine(){
  oled.setDrawColor(1);
  int16_t frameY = kStatusBarHeight + 1;
  oled.drawRFrame(1, frameY, 20, 15, 3);
  int16_t fillY = frameY + 2;
  (linePosition>>3)? oled.drawBox(3, fillY, 3, 11): (void)0;
  ((linePosition>>2)&1)? oled.drawBox(7, fillY, 3, 11): (void)0;
  ((linePosition>>1)&1)? oled.drawBox(12, fillY, 3, 11): (void)0;
  ((linePosition&1)&1)? oled.drawBox(16, fillY, 3, 11): (void)0;
}

void drawFirePosition(){
  oled.setDrawColor(1);
  int16_t frameY = kStatusBarHeight + 21;
  oled.drawRFrame(28, frameY, 71, 12, 4);
  oled.setFont(u8g2_font_open_iconic_all_1x_t);
  int16_t glyphY = frameY + 10;
  oled.drawGlyph(map(firePose, 0, 180, 80, 32), glyphY, 0x00a8);
}

void tirminal () {
  oled.drawRFrame(28,1,71,33,6);
}

void drawMotionJoystickPose(){
  oled.setDrawColor(1);
  int16_t frameY = kStatusBarHeight + 32;
  oled.drawRBox(100 + map(analogRead(joystickB_X), 0, 4096, 0, 23),
                frameY + map(analogRead(joystickB_Y), 0, 4096, 0, 16),
                3, 3, 1);
  oled.drawRFrame(100, frameY, 25, 18, 3);
}

void drawPeripheralJoystickPose(){
  oled.setDrawColor(1);
  int16_t frameY = kStatusBarHeight + 13;
  oled.drawRBox(100 + map(analogRead(joystickA_X), 0, 4096, 0, 23),
                frameY + map(analogRead(joystickA_Y), 0, 4096, 0, 16),
                3, 3, 1);
  oled.drawRFrame(100, frameY, 25, 18, 3);
}

void drawDrongazInterface(){
  oled.clearBuffer();
  oled.setFont(textFont);
  oled.setCursor(0,10);
  oled.print("Alt:");
  oled.print(emission.throttle);
  oled.drawFrame(0,14,screen_Width,6);
  oled.drawBox(0,14,map(emission.throttle,0,2000,0,screen_Width),6);
  oled.setCursor(0,30);
  oled.print("P:");
  oled.print(emission.pitchAngle);
  oled.setCursor(64,30);
  oled.print("R:");
  oled.print(emission.rollAngle);
  oled.setCursor(0,45);
  oled.print("Y:");
  oled.print(emission.yawAngle);
  oled.sendBuffer();
}

static void formatMotorPercent(float value, char* buffer, size_t size){
  int percent = static_cast<int>(roundf(constrain(value, -1.f, 1.f) * 100.f));
  snprintf(buffer, size, "%+d%%", percent);
}

void drawTelemetryInfo(){
  ModuleState* active = getActiveModule();
  if(active && active->descriptor->kind == PeerKind::Thegill){
    drawThegillConfig();
    return;
  }
  if(active && active->descriptor->kind == PeerKind::Generic && genericConfigActive){
    oled.clearBuffer();
    drawHeader("Drone Config");
    oled.setFont(smallFont);
    bool precisionOn = getFunctionOutput(*active, 2);
    const int startY = 22;
    const int lineHeight = 12;
    for(int i = 0; i < 3; ++i){
      oled.setCursor(0, startY + i * lineHeight);
      oled.print(i == genericConfigIndex ? ">" : " ");
      switch(i){
        case 0:
          oled.print("Precision: ");
          oled.print(precisionOn ? "On" : "Off");
          break;
        case 1:
          oled.print("Zero Yaw");
          break;
        default:
          oled.print("Back");
          break;
      }
    }
    oled.setCursor(0, 62);
    oled.print("Press to apply");
    oled.sendBuffer();
    return;
  }

  oled.clearBuffer();
  drawHeader("Telemetry");
  oled.setFont(smallFont);
  int y = 14;
  oled.setCursor(0, y);       oled.print("Alt: ");  oled.print("N/A");
  y += 8;
  oled.setCursor(0, y);       oled.print("Pitch: "); oled.print(telemetry.pitch);
  y += 8;
  oled.setCursor(0, y);       oled.print("Roll: ");  oled.print(telemetry.roll);
  y += 8;
  oled.setCursor(0, y);       oled.print("Yaw: ");   oled.print(telemetry.yaw);
  y += 8;
  oled.setCursor(0, y);       oled.print("VAcc: ");  oled.print(telemetry.verticalAcc);
  oled.setFont(smallIconFont);
  oled.setCursor(112,14);
  if(discovery.hasPeers()){
    oled.print(checkIcon);
  }else{
    oled.print(alertIcon);
  }
  drawHomeFooter();
  oled.sendBuffer();
}

void drawPacketVariables(){
  oled.clearBuffer();
  ModuleState* active = getActiveModule();
  PeerKind kind = active && active->descriptor ? active->descriptor->kind : PeerKind::Generic;
  oled.setFont(smallFont);
  if(kind == PeerKind::Bulky){
    drawHeader("Packet Vars");
    int y = 18;
    oled.setCursor(0, y);      oled.print("Front:");   oled.print(Front_Distance);
    y += 8;
    oled.setCursor(0, y);      oled.print("Bottom:");  oled.print(Bottom_Distance);
    y += 8;
    oled.setCursor(0, y);      oled.print("Speed:");   oled.print(speed);
    y += 8;
    oled.setCursor(0, y);      oled.print("Battery:"); oled.print(batteryLevel);
    y += 8;
    oled.setCursor(0, y);      oled.print("Line:");    oled.print(linePosition, BIN);
  } else if(kind == PeerKind::Thegill){
    drawHeader("Gill Packet");
    int y = 18;
    auto printPair = [&](const char* label, float value){
      char buf[8];
      formatMotorPercent(value, buf, sizeof(buf));
      oled.setCursor(0, y);
      oled.print(label);
      oled.print(':');
      oled.print(buf);
      y += 8;
    };
    printPair("Target L", thegillRuntime.targetLeftFront);
    printPair("Target R", thegillRuntime.targetRightFront);
    printPair("Actual L", thegillRuntime.actualLeftFront);
    printPair("Actual R", thegillRuntime.actualRightFront);
    oled.setCursor(0, y);      oled.print("Brake:"); oled.print(thegillRuntime.brakeActive ? "ON" : "OFF");
    y += 8;
    oled.setCursor(0, y);      oled.print("Honk:");  oled.print(thegillRuntime.honkActive ? "ON" : "OFF");
    y += 8;
    oled.setCursor(0, y);      oled.print("Batt:");  oled.print(thegillRuntime.batteryMillivolts);
    y += 8;
    oled.setCursor(0, y);      oled.print("Pump:");  oled.print(thegillRuntime.pumpDuty);
  } else {
    drawHeader("Packet Vars");
    int y = 18;
    oled.setCursor(0, y);      oled.print("Throttle:"); oled.print(emission.throttle);
    y += 8;
    oled.setCursor(0, y);      oled.print("Pitch:");    oled.print(emission.pitchAngle);
    y += 8;
    oled.setCursor(0, y);      oled.print("Roll:");     oled.print(emission.rollAngle);
    y += 8;
    oled.setCursor(0, y);      oled.print("Yaw:");      oled.print(emission.yawAngle);
    y += 8;
    oled.setCursor(0, y);      oled.print("Arm:");      oled.print(btnmode ? "ON" : "OFF");
  }
  oled.setCursor(0, 62);
  oled.print("Press: Dashboard");
  oled.sendBuffer();
}

void drawModeSummary(){
  oled.clearBuffer();
  ModuleState* active = getActiveModule();
  PeerKind kind = active && active->descriptor ? active->descriptor->kind : PeerKind::Generic;
  drawHeader("Modes");
  oled.setFont(smallFont);
  int y = 18;
  if(!active){
    oled.setCursor(0, y);
    oled.print("No module");
  } else if(kind == PeerKind::Bulky){
    for(size_t slot = 0; slot < kMaxFunctionSlots; ++slot){
      const FunctionActionOption* action = getAssignedAction(*active, slot);
      const char* label = action ? action->name : "Function";
      bool state = getFunctionOutput(*active, slot);
      oled.setCursor(0, y);
      oled.print(label);
      oled.print(": ");
      oled.print(state ? "ON" : "OFF");
      y += 10;
    }
  } else if(kind == PeerKind::Thegill){
    oled.setCursor(0, y);      oled.print("Mode: ");  oled.print(profileToString(thegillConfig.profile));
    y += 10;
    oled.setCursor(0, y);      oled.print("Easing: ");oled.print(easingToString(thegillConfig.easing));
    y += 10;
    oled.setCursor(0, y);      oled.print("Brake: "); oled.print(thegillRuntime.brakeActive ? "ON" : "OFF");
    y += 10;
    oled.setCursor(0, y);      oled.print("Honk: ");  oled.print(thegillRuntime.honkActive ? "ON" : "OFF");
  } else {
    for(size_t slot = 0; slot < kMaxFunctionSlots; ++slot){
      const FunctionActionOption* action = getAssignedAction(*active, slot);
      const char* label = action ? action->name : "Function";
      bool state = getFunctionOutput(*active, slot);
      oled.setCursor(0, y);
      oled.print(label);
      oled.print(": ");
      oled.print(state ? "ON" : "OFF");
      y += 10;
    }
  }
  oled.setCursor(0, 62);
  oled.print("Press: Dashboard");
  oled.sendBuffer();
}

void drawPidGraph(){
  oled.clearBuffer();
  static const char axisLabels[PID_AXIS_COUNT][6] = {"Pitch", "Roll", "Yaw"};
  const int axis = constrain(pidTunerAxisIndex, 0, PID_AXIS_COUNT - 1);

  char title[16];
  snprintf(title, sizeof(title), "PID %s", axisLabels[axis]);
  drawHeader(title);
  static const char axisLetters[PID_AXIS_COUNT] = {'P', 'R', 'Y'};
  static const uint8_t axisMaskBits[PID_AXIS_COUNT] = {0x02, 0x01, 0x04};

  const int graphTop = 14;
  const int graphBottom = 38;
  const int16_t correctionMin = -800;
  const int16_t correctionMax = 800;

  oled.setFont(smallFont);
  oled.setDrawColor(1);

  int zeroY = mapHistoryValue(0, correctionMin, correctionMax, graphTop, graphBottom);
  oled.drawHLine(0, zeroY, screen_Width);

  int prevY = mapHistoryValue(pidCorrectionHistory[axis][0], correctionMin, correctionMax, graphTop, graphBottom);
  for (int x = 1; x < screen_Width; ++x) {
    int currY = mapHistoryValue(pidCorrectionHistory[axis][x], correctionMin, correctionMax, graphTop, graphBottom);
    oled.drawLine(x - 1, prevY, x, currY);
    prevY = currY;
  }

  float setpoint = (axis == 0) ? static_cast<float>(telemetry.pitchAngle)
                 : (axis == 1) ? static_cast<float>(telemetry.rollAngle)
                               : static_cast<float>(telemetry.yawAngle);
  float actual = (axis == 0) ? telemetry.pitch : (axis == 1) ? telemetry.roll : telemetry.yaw;
  float error = setpoint - actual;
  float response = (axis == 0) ? telemetry.pitchCorrection
                 : (axis == 1) ? telemetry.rollCorrection : telemetry.yawCorrection;

  char buffer[16];
  int textX = screen_Width - 40;
  if (textX < 80) textX = 80;
  oled.setCursor(textX, graphTop + 8);
  snprintf(buffer, sizeof(buffer), "SP:%4.1f", setpoint);
  oled.print(buffer);
  oled.setCursor(textX, graphTop + 16);
  snprintf(buffer, sizeof(buffer), "PV:%4.1f", actual);
  oled.print(buffer);
  oled.setCursor(textX, graphTop + 24);
  snprintf(buffer, sizeof(buffer), "Err:%4.1f", error);
  oled.print(buffer);
  oled.setCursor(textX, graphTop + 32);
  snprintf(buffer, sizeof(buffer), "Resp:%4.0f", response);
  oled.print(buffer);

  const bool focusAxis = pidFocusIndex == PID_FOCUS_AXIS;
  const bool focusKp = pidFocusIndex == PID_FOCUS_KP;
  const bool focusKi = pidFocusIndex == PID_FOCUS_KI;
  const bool focusKd = pidFocusIndex == PID_FOCUS_KD;
  const bool focusStep = pidFocusIndex == PID_FOCUS_STEP;

  char axisLine[64];
  axisLine[0] = ' ';
  for (int i = 0; i < PID_AXIS_COUNT; ++i) {
    bool selected = (i == pidTunerAxisIndex);
    bool enabled = (droneStabilizationMask & axisMaskBits[i]) != 0;
    char letter = selected ? axisLetters[i] : static_cast<char>(tolower(axisLetters[i]));
    char segment[16];
    const char* prefix = (selected && focusAxis) ? ">" : " ";
    snprintf(segment, sizeof(segment), "%s%c[%c]", prefix, letter, enabled ? 'x' : ' ');
    strncat(axisLine, segment, sizeof(axisLine) - strlen(axisLine) - 1);
    if (i + 1 < PID_AXIS_COUNT) {
      strncat(axisLine, " ", sizeof(axisLine) - strlen(axisLine) - 1);
    }
  }
  strncat(axisLine, " G:", sizeof(axisLine) - strlen(axisLine) - 1);
  strncat(axisLine, droneStabilizationGlobal ? "ON" : "OFF", sizeof(axisLine) - strlen(axisLine) - 1);

  oled.setCursor(0, 46);
  oled.print(axisLine);

  const PidGains& gains = pidGains[axis];
  char kpBuf[16];
  char kiBuf[16];
  char kdBuf[16];
  if (pidGainsValid[axis]) {
    snprintf(kpBuf, sizeof(kpBuf), "Kp:%4.2f", gains.kp);
    snprintf(kiBuf, sizeof(kiBuf), "Ki:%4.2f", gains.ki);
    snprintf(kdBuf, sizeof(kdBuf), "Kd:%4.2f", gains.kd);
  } else {
    strcpy(kpBuf, "Kp:--");
    strcpy(kiBuf, "Ki:--");
    strcpy(kdBuf, "Kd:--");
  }

  oled.setCursor(0, 54);
  oled.print(focusKp ? ">" : " ");
  oled.print(kpBuf);
  oled.print(" ");
  oled.print(focusKi ? ">" : " ");
  oled.print(kiBuf);
  oled.print(" ");
  oled.print(focusKd ? ">" : " ");
  oled.print(kdBuf);

  oled.setCursor(0, 62);
  oled.print(focusStep ? ">" : " ");
  oled.print("Step:");
  oled.print(pidCoarseMode ? "Coarse" : "Fine  ");
  oled.print(" F1 Axis  F2 Step  F3 Home");

  oled.sendBuffer();
}

void drawOrientationCube(){
  oled.clearBuffer();
  drawHeader("Orientation");
  const float xSize = 20.0f;
  const float ySize = 10.0f;
  const float zSize = 5.0f;
  const int cx = screen_Width/2;
  const int cy = screen_Height/2 + 6;
  struct Vec3 { float x,y,z; };
  const Vec3 verts[8] = {
    {-xSize,-ySize,-zSize}, { xSize,-ySize,-zSize}, { xSize, ySize,-zSize}, {-xSize, ySize,-zSize},
    {-xSize,-ySize, zSize}, { xSize,-ySize, zSize}, { xSize, ySize, zSize}, {-xSize, ySize, zSize}
  };
  int px[8];
  int py[8];
  float roll  = telemetry.roll  * DEG_TO_RAD;
  float pitch = -telemetry.pitch * DEG_TO_RAD;
  float yaw   = -telemetry.yaw   * DEG_TO_RAD + PI;
  for(int i=0;i<8;i++){
    float x=verts[i].x;
    float y=verts[i].y;
    float z=verts[i].z;
    float y1 = y*cos(roll) - z*sin(roll);
    float z1 = y*sin(roll) + z*cos(roll);
    y = y1; z = z1;
    float x1 = x*cos(pitch) + z*sin(pitch);
    float z2 = -x*sin(pitch) + z*cos(pitch);
    x = x1; z = z2;
    float x2 = x*cos(yaw) - y*sin(yaw);
    float y2 = x*sin(yaw) + y*cos(yaw);
    x = x2; y = y2;
    px[i] = cx + (int)x;
    py[i] = cy - (int)y;
  }
  const uint8_t edges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7}
  };
  for(int i=0;i<12;i++){
    oled.drawLine(px[edges[i][0]], py[edges[i][0]],
                  px[edges[i][1]], py[edges[i][1]]);
  }
  int arrowLen = map(static_cast<int>(telemetry.verticalAcc * 100), -1000, 1000, -20, 20);
  oled.drawLine(cx, cy, cx, cy - arrowLen);
  if(arrowLen != 0){
    int head = cy - arrowLen;
    oled.drawTriangle(cx, head, cx-3, head + (arrowLen>0?-5:5),
                      cx+3, head + (arrowLen>0?-5:5));
  }
  drawHomeFooter();
  oled.sendBuffer();
}

void drawPairingMenu(){
  oled.clearBuffer();
  drawHeader("Pairing");
  oled.setFont(textFont);
  int count = discovery.getPeerCount();
  for(int i=0;i<count && i<4;i++){
    const char* name = discovery.getPeerName(i);
    oled.setCursor(0, 22 + i*12);
    if(i==selectedPeer) oled.print(">"); else oled.print(" ");
    oled.print(name);
  }
  oled.setCursor(0, 22 + count*12);
  if(selectedPeer == count) oled.print(">"); else oled.print(" ");
  oled.print("Home");
  oled.sendBuffer();
}

void drawPeerInfo(){
  oled.clearBuffer();
  drawHeader("Device Info");
  oled.setFont(textFont);
  const char* name = discovery.getPeerName(infoPeer);
  const uint8_t* mac = discovery.getPeer(infoPeer);
  oled.setCursor(0,22); oled.print("Device: "); oled.print(name);
  char buf[18];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  oled.setCursor(0,34); oled.print("MAC: "); oled.print(buf);
  bool isGill = false;
  size_t nameLen = strlen(name);
  for(size_t i = 0; i + 3 < nameLen; ++i){
    if(tolower(static_cast<unsigned char>(name[i])) == 'g' &&
       tolower(static_cast<unsigned char>(name[i+1])) == 'i' &&
       tolower(static_cast<unsigned char>(name[i+2])) == 'l' &&
       tolower(static_cast<unsigned char>(name[i+3])) == 'l'){
      isGill = true;
      break;
    }
  }
  if(strcmp(name, "Bulky") == 0){
    oled.setCursor(0,46);
    oled.print("Bulky detected");
  } else if(isGill){
    oled.setCursor(0,46);
    oled.print("The'gill detected");
  }
  oled.setCursor(0,60);
  if(homeSelected){
    oled.print(">Return");
    oled.setCursor(60,60); oled.print("Pair");
  }else{
    oled.print(">Pair");
    oled.setCursor(60,60); oled.print("Return");
  }
  oled.sendBuffer();
}

void drawHomeMenu(){
  oled.clearBuffer();
  drawHeader("Layouts");
  size_t moduleCount = getModuleStateCount();
  int totalEntries = static_cast<int>(moduleCount) + 1;
  if(totalEntries <= 0){
    oled.setFont(smallFont);
    oled.setCursor(8, 32);
    oled.print("No layouts available");
    oled.sendBuffer();
    return;
  }

  if(homeMenuIndex < 0) homeMenuIndex = 0;
  if(homeMenuIndex >= totalEntries) homeMenuIndex = totalEntries - 1;

  const int16_t cardX = 6;
  const int16_t cardY = 16;
  if(homeMenuIndex == 0){
    drawLayoutCardFrame(cardX, cardY, "Auto", autoDashboardEnabled ? "Following paired" : "Manual override", true);
    if(lastPairedModule && lastPairedModule->descriptor){
      oled.setFont(smallFont);
      oled.setCursor(cardX + 6, cardY + 26);
      oled.print(lastPairedModule->descriptor->displayName);
    }
  } else {
    ModuleState* state = getModuleState(static_cast<size_t>(homeMenuIndex - 1));
    if(state && state->descriptor && state->descriptor->drawLayoutCard){
      state->descriptor->drawLayoutCard(*state, cardX, cardY, true);
    }
  }

  if(totalEntries > 1){
    if(homeMenuIndex > 0){
      oled.drawTriangle(2, 36, 6, 30, 6, 42);
    }
    if(homeMenuIndex + 1 < totalEntries){
      int16_t x = screen_Width - 2;
      oled.drawTriangle(x, 36, x-4, 30, x-4, 42);
    }
  }

  oled.setFont(smallFont);
  oled.setCursor(4, screen_Height - 2);
  oled.print("Press to activate");
  char buffer[12];
  snprintf(buffer, sizeof(buffer), "%d/%d", homeMenuIndex + 1, totalEntries);
  int width = oled.getUTF8Width(buffer);
  oled.setCursor(screen_Width - width - 2, screen_Height - 2);
  oled.print(buffer);
  oled.sendBuffer();
}

void drawHomeFooter(){
  oled.setCursor(0,60);
  if(homeSelected) oled.print(">"); else oled.print(" ");
  oled.print("Home");
}

void drawGlobalMenu(){
  oled.clearBuffer();
  drawHeader("Menu");
  oled.setFont(smallFont);
  MenuEntry entries[10];
  int count = buildGlobalMenuEntries(entries, 10);
  if(count <= 0){
    oled.setCursor(0, 18);
    oled.print("No options");
  } else {
    if(globalMenuIndex >= count) globalMenuIndex = count - 1;
    if(globalMenuIndex < 0) globalMenuIndex = 0;
    int maxOffset = count - kGlobalMenuVisibleLines;
    if(maxOffset < 0) maxOffset = 0;
    if(globalMenuScrollOffset > maxOffset) globalMenuScrollOffset = maxOffset;
    if(globalMenuIndex < globalMenuScrollOffset) globalMenuScrollOffset = globalMenuIndex;
    if(globalMenuIndex >= globalMenuScrollOffset + kGlobalMenuVisibleLines) {
      globalMenuScrollOffset = globalMenuIndex - kGlobalMenuVisibleLines + 1;
    }
    int y = 18;
    int end = globalMenuScrollOffset + kGlobalMenuVisibleLines;
    if(end > count) end = count;
    for(int i = globalMenuScrollOffset; i < end; ++i){
      oled.setCursor(0, y);
      oled.print(i == globalMenuIndex ? ">" : " ");
      oled.print(entries[i].label);
      y += 10;
    }
    if(globalMenuScrollOffset > 0){
      oled.drawTriangle(screen_Width - 6, 18, screen_Width - 2, 18, screen_Width - 4, 12);
    }
    if(globalMenuScrollOffset + kGlobalMenuVisibleLines < count){
      int arrowY = 18 + (kGlobalMenuVisibleLines - 1) * 10 + 6;
      oled.drawTriangle(screen_Width - 6, arrowY, screen_Width - 2, arrowY, screen_Width - 4, arrowY + 6);
    }
  }

  oled.setCursor(0, 62);
  oled.print("Press to select");
  oled.sendBuffer();
}


static void drawMotorBar(int x, int y, float actual, float target){
  const int width = 72;
  const int height = 10;
  const int mid = x + width / 2;
  const int range = width / 2 - 2;
  oled.setDrawColor(1);
  oled.drawFrame(x, y, width, height);
  int actualPixels = static_cast<int>(roundf(constrain(actual, -1.f, 1.f) * range));
  if(actualPixels >= 0){
    oled.drawBox(mid, y + 1, actualPixels, height - 2);
  } else {
    oled.drawBox(mid + actualPixels, y + 1, -actualPixels, height - 2);

  }
  int percent = static_cast<int>(roundf(constrain(actual, -1.f, 1.f) * 100.f));
  oled.print(percent);
  oled.print('%');
}

static void printMotorPercent(float value) {
  int percent = static_cast<int>(roundf(constrain(value, -1.f, 1.f) * 100.f));
  oled.print(percent);
  oled.print('%');
}

static void drawMotorValue(int x, int y, const char* label, float actual, float target){
  oled.setCursor(x, y);
  oled.print(label);
  oled.print(':');
  oled.setCursor(x + 16, y);
  printMotorPercent(actual);
  oled.setCursor(x + 44, y);
  oled.print("->");
  oled.setCursor(x + 58, y);
  printMotorPercent(target);
}


static void drawMotorBarLabels(int x, int y,
                               const char* frontLabel, float frontValue,
                               const char* rearLabel, float rearValue){
  const int width = 72;
  const int height = 10;
  char frontBuf[8];
  char rearBuf[8];
  formatMotorPercent(frontValue, frontBuf, sizeof(frontBuf));
  formatMotorPercent(rearValue, rearBuf, sizeof(rearBuf));

  char line[32];
  snprintf(line, sizeof(line), "%s%s %s%s", frontLabel, frontBuf, rearLabel, rearBuf);

  int textWidth = oled.getUTF8Width(line);
  int cursorX = x + (width - textWidth) / 2;
  if(cursorX < x + 1){
    cursorX = x + 1;
  }

  oled.setDrawColor(2);
  oled.setCursor(cursorX, y + height - 2);
  oled.print(line);
  oled.setDrawColor(1);
}

void drawThegillDashboard(){
  oled.clearBuffer();
  oled.setFont(smallFont);
  int16_t top = kStatusBarHeight + 4;
  oled.setCursor(0, top);         oled.print("Mode: "); oled.print(profileToString(thegillConfig.profile));
  oled.setCursor(0, top + 8);     oled.print("Ease: "); oled.print(easingToString(thegillConfig.easing));
  oled.setCursor(0, top + 16);    oled.print("Strength: "); oled.print(thegillConfig.easingRate, 2);


  float leftTarget = (thegillRuntime.targetLeftFront + thegillRuntime.targetLeftRear) * 0.5f;
  float rightTarget = (thegillRuntime.targetRightFront + thegillRuntime.targetRightRear) * 0.5f;
  float leftActual = (thegillRuntime.actualLeftFront + thegillRuntime.actualLeftRear) * 0.5f;
  float rightActual = (thegillRuntime.actualRightFront + thegillRuntime.actualRightRear) * 0.5f;

  oled.setCursor(0, top + 26);    oled.print("Left");
  drawMotorBar(40, top + 20, leftActual, leftTarget);
  drawMotorBarLabels(40, top + 20, "LF", thegillRuntime.actualLeftFront,
                             "LR", thegillRuntime.actualLeftRear);

  oled.setCursor(0, top + 40);    oled.print("Right");
  drawMotorBar(40, top + 34, rightActual, rightTarget);
  drawMotorBarLabels(40, top + 34, "RF", thegillRuntime.actualRightFront,
                              "RR", thegillRuntime.actualRightRear);


  oled.setCursor(96, top);        oled.print(thegillRuntime.brakeActive ? "BRK" : "   ");
  oled.setCursor(96, top + 8);    oled.print(thegillRuntime.honkActive ? "HNK" : "   ");
}

void drawThegillConfig(){
  oled.clearBuffer();
  drawHeader("Gill Config");
  oled.setFont(smallFont);
  const char* labels[] = {"Mode", "Easing", "Back"};
  const char* values[] = {
    profileToString(thegillConfig.profile),
    easingToString(thegillConfig.easing),
    ""};
  for(int i = 0; i < 3; ++i){
    oled.setCursor(0, 22 + i * 12);
    oled.print(i == gillConfigIndex ? ">" : " ");
    oled.print(labels[i]);
    if(i < 2){
      oled.setCursor(60, 22 + i * 12);
      oled.print(values[i]);
    }
  }
  oled.sendBuffer();
}

void drawBulkyDashboard(){
  oled.clearBuffer();
  drawFirePosition();
  drawLine();
  drawMotionJoystickPose();
  drawPeripheralJoystickPose();
  drawProximity();
  drawSpeed();
}

void drawGenericDashboard(){
  oled.clearBuffer();
  const float xSize = 20.0f;
  const float ySize = 10.0f;
  const float zSize = 5.0f;
  const int16_t top = kStatusBarHeight + 2;
  const int cx = screen_Width/2;
  const int cy = top + (screen_Height - top) / 2;
  struct Vec3 { float x,y,z; };
  const Vec3 verts[8] = {
    {-xSize,-ySize,-zSize}, { xSize,-ySize,-zSize}, { xSize, ySize,-zSize}, {-xSize, ySize,-zSize},
    {-xSize,-ySize, zSize}, { xSize,-ySize, zSize}, { xSize, ySize, zSize}, {-xSize, ySize, zSize}
  };
  int px[8];
  int py[8];
  float roll  = telemetry.roll  * DEG_TO_RAD;
  float pitch = -telemetry.pitch * DEG_TO_RAD;
  float yaw   = -telemetry.yaw   * DEG_TO_RAD + PI;
  for(int i=0;i<8;i++){
    float x=verts[i].x;
    float y=verts[i].y;
    float z=verts[i].z;
    float y1 = y*cos(roll) - z*sin(roll);
    float z1 = y*sin(roll) + z*cos(roll);
    y = y1; z = z1;
    float x1 = x*cos(pitch) + z*sin(pitch);
    float z2 = -x*sin(pitch) + z*cos(pitch);
    x = x1; z = z2;
    float x2 = x*cos(yaw) - y*sin(yaw);
    float y2 = x*sin(yaw) + y*cos(yaw);
    x = x2; y = y2;
    px[i] = cx + (int)x;
    py[i] = cy - (int)y;
  }
  const uint8_t edges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7}
  };
  for(int i=0;i<12;i++){
    oled.drawLine(px[edges[i][0]], py[edges[i][0]],
                  px[edges[i][1]], py[edges[i][1]]);
  }
  int arrowLen = map(static_cast<int>(telemetry.verticalAcc * 100), -1000, 1000, -20, 20);
  oled.drawLine(cx, cy, cx, cy - arrowLen);
  if(arrowLen != 0){
    int head = cy - arrowLen;
    oled.drawTriangle(cx, head, cx-3, head + (arrowLen>0?-5:5),
                      cx+3, head + (arrowLen>0?-5:5));
  }
  oled.setFont(smallFont);
  int16_t textY = top + 6;
  oled.setCursor(0, textY);           oled.print("Alt:"); oled.print("N/A");
  textY += 8;
  oled.setCursor(0, textY);           oled.print("Thr:"); oled.print(emission.throttle);
  textY += 8;
  oled.setCursor(0, textY);           oled.print("P:");   oled.print(emission.pitchAngle);
  textY += 8;
  oled.setCursor(0, textY);           oled.print("R:");   oled.print(emission.rollAngle);
  textY += 8;
  oled.setCursor(0, textY);           oled.print("Y:");   oled.print(emission.yawAngle);
}

void drawConnectionLog(){
  oled.clearBuffer();
  drawHeader("Link Log");
  oled.setFont(tinyFont);

  const size_t count = connectionLogGetCount();
  const size_t totalLines = getTotalWrappedLineCount();
  int maxOffset = 0;
  if(totalLines > static_cast<size_t>(kLogVisibleLines)){
    maxOffset = static_cast<int>(totalLines) - kLogVisibleLines;
  }
  if(logScrollOffset < 0){
    logScrollOffset = 0;
  }
  if(logScrollOffset > maxOffset){
    logScrollOffset = maxOffset;
  }

  if(count == 0){
    oled.setCursor(0, kLogStartY);
    oled.print("Waiting for events...");
  } else {
    size_t startLine = 0;
    if(totalLines > static_cast<size_t>(kLogVisibleLines)){
      startLine = totalLines - kLogVisibleLines - static_cast<size_t>(logScrollOffset);
    }
    size_t currentLine = 0;
    int drawnLines = 0;
    for(size_t entryIndex = 0; entryIndex < count && drawnLines < kLogVisibleLines; ++entryIndex){
      const char* entry = connectionLogGetEntry(entryIndex);
      if(!entry){
        continue;
      }
      size_t entryLineCount = countWrappedLines(entry);
      if(currentLine + entryLineCount <= startLine){
        currentLine += entryLineCount;
        continue;
      }
      size_t skipLines = 0;
      if(startLine > currentLine){
        skipLines = startLine - currentLine;
      }
      for(size_t line = skipLines; line < entryLineCount && drawnLines < kLogVisibleLines; ++line){
        char buffer[kLogLineBufferSize];
        if(!extractWrappedLine(entry, line, buffer, sizeof(buffer))){
          continue;
        }
        int16_t y = kLogStartY + drawnLines * kLogLineHeight;
        oled.setCursor(0, y);
        oled.print(buffer);
        ++drawnLines;
      }
      currentLine += entryLineCount;
    }
  }

  oled.sendBuffer();
}

static void drawStatusBar(){
  oled.setFont(statusFont);
  oled.setDrawColor(0);
  oled.drawBox(0, 0, screen_Width, kStatusBarHeight);
  oled.setDrawColor(1);
  oled.drawHLine(0, kStatusBarHeight - 1, screen_Width);

  ModuleState* active = getActiveModule();
  bool hasFunction = active && active->descriptor && active->descriptor->functionOptionCount > 0;
  const FunctionActionOption* functionAction = hasFunction ? getAssignedAction(*active, 0) : nullptr;
  const char* functionLabel = functionAction ? functionAction->shortLabel : "---";
  bool functionActive = hasFunction ? getFunctionOutput(*active, 0) : false;

  const int buttonCount = hasFunction ? 2 : 1;
  const int16_t kButtonWidth = 28;
  int16_t buttonAreaWidth = kButtonWidth * buttonCount;
  if(buttonAreaWidth > screen_Width / 2){
    buttonAreaWidth = screen_Width / 2;
  }
  int16_t nameAreaX = buttonAreaWidth;
  int16_t nameAreaWidth = screen_Width - nameAreaX;
  if(nameAreaWidth < 32){
    nameAreaWidth = 32;
    buttonAreaWidth = screen_Width - nameAreaWidth;
  }

  auto drawButton = [&](int index, const char* label, bool focused, bool activeIndicator){
    int16_t x = index * kButtonWidth;
    if(x >= buttonAreaWidth){
      x = buttonAreaWidth - kButtonWidth;
    }
    int16_t width = kButtonWidth;
    if(index == buttonCount - 1){
      width = buttonAreaWidth - x;
    }
    if(width < 20){
      width = 20;
    }
    bool fill = focused;
    if(fill){
      oled.drawBox(x, 0, width, kStatusBarHeight - 1);
      oled.setDrawColor(0);
    } else {
      oled.drawFrame(x, 0, width, kStatusBarHeight - 1);
      oled.setDrawColor(1);
    }
    int16_t textWidth = oled.getUTF8Width(label);
    int16_t textX = x + (width - textWidth) / 2;
    if(textX < x + 1) textX = x + 1;
    int16_t textY = (kStatusBarHeight + kStatusFontHeight) / 2;
    oled.setCursor(textX, textY);
    oled.print(label);
    oled.setDrawColor(1);
    if(!fill && activeIndicator){
      oled.drawBox(x + width - 5, 2, 3, 3);
    }
  };

  drawButton(0, "Menu", dashboardFocusIndex == 0, false);
  if(hasFunction){
    drawButton(1, functionLabel, dashboardFocusIndex == 1, functionActive);
  }

  if(nameAreaWidth > 0){
    oled.drawVLine(nameAreaX, 0, kStatusBarHeight - 1);
  }
  const char* name = "--";
  char buffer[32];
  if(autoDashboardEnabled && lastPairedModule && lastPairedModule->descriptor){
    snprintf(buffer, sizeof(buffer), "Auto:%s", lastPairedModule->descriptor->displayName);
    name = buffer;
  } else if(active && active->descriptor){
    name = active->descriptor->displayName;
  } else if(lastPairedModule && lastPairedModule->descriptor){
    name = lastPairedModule->descriptor->displayName;
  }
  int16_t textWidth = oled.getUTF8Width(name);
  if(textWidth > nameAreaWidth - 4){
    size_t len = strlen(name);
    while(len > 0){
      strncpy(buffer, name, len);
      buffer[len] = '\0';
      int16_t candidateWidth = oled.getUTF8Width(buffer);
      if(candidateWidth <= nameAreaWidth - 4){
        name = buffer;
        textWidth = candidateWidth;
        break;
      }
      --len;
    }
    if(len == 0){
      buffer[0] = '\0';
      name = buffer;
      textWidth = 0;
    }
  }
  int16_t textX = nameAreaX + (nameAreaWidth - textWidth) / 2;
  if(textX < nameAreaX + 1) textX = nameAreaX + 1;
  int16_t textY = (kStatusBarHeight + kStatusFontHeight) / 2;
  oled.setCursor(textX, textY);
  oled.print(name);
}

void drawDashboard(){
  ModuleState* active = getActiveModule();
  if(active && active->descriptor && active->descriptor->drawDashboard){
    active->descriptor->drawDashboard();
  } else {
    oled.clearBuffer();
    oled.setFont(smallFont);
    oled.setCursor(0, kStatusBarHeight + 12);
    oled.print("No dashboard available");
  }
  drawStatusBar();
  oled.sendBuffer();
}

void drawAbout(){
  oled.clearBuffer();
  drawHeader("About");
  oled.setFont(smallFont);
  oled.setCursor(0,24); oled.print("Devices:");
  oled.setCursor(0,34); oled.print("Bulky");
  oled.setCursor(0,44); oled.print("The'gill");
  oled.setCursor(0,54); oled.print("\xB5Tomba");
  drawHomeFooter();
  oled.sendBuffer();
}

void drawBootScreen(){
  oled.clearBuffer();
  oled.setFont(bootTitleFont);
  uint8_t w = oled.getUTF8Width("ASCE");
  oled.setCursor((screen_Width - w)/2, 40);
  oled.print("ASCE");
  oled.setFont(bootSubFont);
  w = oled.getUTF8Width("ILITE V2");
  oled.setCursor((screen_Width - w)/2, 62);
  oled.print("ILITE V2");
  oled.sendBuffer();
  delay(2000);
}

void irData (){
  oled.drawStr(10,10,"R");
  oled.setCursor(17, 10);
  oled.print(R_Line);

  oled.drawStr(39,10,"L");
  oled.setCursor(45, 10);
  oled.print(L_Line);

  oled.drawStr(64,10,"LM");
  oled.setCursor(76, 10);
  oled.print(LM_Line);

  oled.drawStr(96,10,"RM");
  oled.setCursor(108, 10);
  oled.print(RM_Line);
}

void Line_detection() {
  oled.setCursor(10, 20);
  oled.print("LM_Thrs:");
  oled.setCursor(80, 20);
  oled.print(LM_Threshold);

  oled.setCursor(10, 30);
  oled.print("RM_Thrs:");
  oled.setCursor(80, 30);
  oled.print(RM_Threshold);

  oled.setCursor(10, 40);
  oled.print("L_Thrs:");
  oled.setCursor(80, 40);
  oled.print(L_Threshold);

  oled.setCursor(10, 50);
  oled.print("R_Thrs:");
  oled.setCursor(80, 50);
  oled.print(R_Threshold);
}

void Pid_Tuner() {
  oled.setCursor(10, 20);
  oled.print("LKp:");
  oled.setCursor(95, 20);
  oled.print(LKp);

  oled.setCursor(10, 30);
  oled.print("LKd:");
  oled.setCursor(95, 30);
  oled.print(LKd);

  oled.setCursor(10, 40);
  oled.print("Base_speed:");
  oled.setCursor(95, 40);
  oled.print(Base_speed);
}

void Fire_detection() {
  oled.setCursor(10, 20);
  oled.print("IRBais:");
  oled.setCursor(95, 20);
  oled.print(IRBais);

  oled.setCursor(10, 30);
  oled.print("LFirsensor:");
  oled.setCursor(95, 30);
  oled.print(LFirsensor);

  oled.setCursor(10, 40);
  oled.print("RFirsensor:");
  oled.setCursor(95, 40);
  oled.print(RFirsensor);

  oled.setCursor(10, 50);
  oled.print("Det Tolerance:");
  oled.setCursor(95, 50);
  oled.print(Detection_Tolerance);

  oled.setCursor(10, 60);
  oled.print("Det Distance:");
  oled.setCursor(95, 60);
  oled.print(Detection_Distance);
}

void displayMenu(){
  if (screen==1){
    Line_detection();
    irData();
  }
  if (screen==2){
    Pid_Tuner();
  }
  if (screen==3){
    Fire_detection();
  }
}
