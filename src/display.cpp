#include "display.h"
#include "thegill.h"
#include "connection_log.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0);

<<<<<<< HEAD
#define iconFont u8g2_font_open_iconic_all_2x_t
#define networkBatteryIconFont u8g2_font_siji_t_6x10
#define textFont u8g2_font_torussansbold8_8r
#define smallFont u8g2_font_5x8_tf
#define bootTitleFont u8g2_font_inb38_mr
#define bootSubFont u8g2_font_6x13_tf
#define smallIconFont u8g2_font_open_iconic_all_1x_t

byte displayMode = 5;
=======
byte displayMode = DISPLAY_MODE_LOG;
>>>>>>> 6041fcac2e7cd785df71e6ab3fb809cbc15f1706
int homeMenuIndex = 0;
bool homeSelected = false;
int selectedPeer = 0;
int lastEncoderCount = 0;
unsigned long lastDiscoveryTime = 0;
int infoPeer = 0;
int gillConfigIndex = 0;
int globalMenuIndex = 0;
int dashboardFocusIndex = 0;
int logScrollOffset = 0;

static const char* modeToString(GillMode mode){
  switch(mode){
    case GillMode::Differential: return "Differential";
    case GillMode::Default:
    default: return "Default";
  }
}

static const char* easingToString(GillEasing easing){
  switch(easing){
    case GillEasing::Linear: return "Linear";
    case GillEasing::EaseIn: return "Ease In";
    case GillEasing::EaseOut: return "Ease Out";
    case GillEasing::EaseInOut: return "Ease InOut";
    case GillEasing::Sine:
    default: return "Sine";
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

int pidGraphIndex = 0;

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
  oled.drawRFrame(14,36,12,28,3);
  oled.drawBox(16,57,8,map(Front_Distance,0,50,27,0));
  oled.setDrawColor(1);
  oled.drawRFrame(1,36,12,28,3);
  oled.drawBox(3,57,8,map(Bottom_Distance,0,50,27,0));
}

void drawSpeed(){
  oled.setFont(textFont);
  oled.setDrawColor(2);
  oled.setCursor(56, 58);
  oled.print(speed);
  oled.setDrawColor(2);
  oled.drawRFrame(28,48,71,12,4);
  oled.drawBox(30,50,map(speed,0,100,0,67),8);
  oled.drawLine(32, 61, map(analogRead(potA),0,4096,32,80), 61);
}

void drawLine(){
  oled.setDrawColor(1);
  oled.drawRFrame(1,11,20,15,3);
  (linePosition>>3)? oled.drawBox(3,13,3,11): (void)0;
  ((linePosition>>2)&1)? oled.drawBox(7,13,3,11): (void)0;
  ((linePosition>>1)&1)? oled.drawBox(12,13,3,11): (void)0;
  ((linePosition&1)&1)? oled.drawBox(16,13,3,11): (void)0;
}

void drawFirePosition(){
  oled.setDrawColor(1);
  oled.drawRFrame(28,35,71,12,4);
  oled.setFont(u8g2_font_open_iconic_all_1x_t);
  oled.drawGlyph(map(firePose,0,180,80,32), 45, 0x00a8);
}

void tirminal () {
  oled.drawRFrame(28,1,71,33,6);
}

void drawMotionJoystickPose(){
  oled.setDrawColor(1);
  oled.drawRBox(100+map(analogRead(joystickB_X),0,4096,0,23),46+map(analogRead(joystickB_Y),0,4096,0,16),3,3,1);
  oled.drawRFrame(100,46,25,18,3);
}

void drawPeripheralJoystickPose(){
  oled.setDrawColor(1);
  oled.drawRBox(100+map(analogRead(joystickA_X),0,4096,0,23),27+map(analogRead(joystickA_Y),0,4096,0,16),3,3,1);
  oled.drawRFrame(100,27,25,18,3);
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

void drawTelemetryInfo(){
  ModuleState* active = getActiveModule();
  if(active && active->descriptor->kind == PeerKind::Thegill){
    drawThegillConfig();
    return;
  }

  oled.clearBuffer();
  drawHeader("Telemetry");
  oled.setFont(smallFont);
  int y = 14;
  oled.setCursor(0, y);       oled.print("Alt: ");  oled.print(telemetry.altitude);
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

void drawPidGraph(){
  oled.clearBuffer();
  int16_t* history;
  const char* label;
  switch(pidGraphIndex){
    case 0: history = pidPitchHistory; label = "Pitch"; break;
    case 1: history = pidRollHistory;  label = "Roll";  break;
    default: history = pidYawHistory; label = "Yaw";   break;
  }
  char title[16];
  snprintf(title, sizeof(title), "PID %s", label);
  drawHeader(title);
  for(int x=1; x<screen_Width; x++){
    oled.drawLine(x-1, map(history[x-1], -500, 500, screen_Height-1, 13),
                  x,   map(history[x],   -500, 500, screen_Height-1, 13));
  }
  drawHomeFooter();
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
  if(moduleCount == 0){
    oled.setFont(smallFont);
    oled.setCursor(8, 32);
    oled.print("No layouts available");
    oled.sendBuffer();
    return;
  }

  if(homeMenuIndex < 0) homeMenuIndex = 0;
  if(homeMenuIndex >= static_cast<int>(moduleCount)) homeMenuIndex = static_cast<int>(moduleCount) - 1;

  ModuleState* state = getModuleState(static_cast<size_t>(homeMenuIndex));
  const int16_t cardX = 6;
  const int16_t cardY = 16;
  if(state && state->descriptor && state->descriptor->drawLayoutCard){
    state->descriptor->drawLayoutCard(*state, cardX, cardY, true);
  }

  if(moduleCount > 1){
    if(homeMenuIndex > 0){
      oled.drawTriangle(2, 36, 6, 30, 6, 42);
    }
    if(homeMenuIndex + 1 < static_cast<int>(moduleCount)){
      int16_t x = screen_Width - 2;
      oled.drawTriangle(x, 36, x-4, 30, x-4, 42);
    }
  }

  oled.setFont(smallFont);
  oled.setCursor(4, screen_Height - 2);
  oled.print("Press to activate");
  char buffer[12];
  snprintf(buffer, sizeof(buffer), "%d/%d", homeMenuIndex + 1, static_cast<int>(moduleCount));
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
  ModuleState* active = getActiveModule();
  const char* baseItems[] = {
    "Dashboard",
    (active && active->descriptor->kind == PeerKind::Thegill) ? "Config" : "Telemetry",
    "PID Graph",
    "Orientation",
    "Pairing",
    "Link Log",
    "About"
  };
  const int baseCount = sizeof(baseItems) / sizeof(baseItems[0]);
  const int functionEntryOffset = baseCount;
  const int functionEntryCount = static_cast<int>(kMaxFunctionSlots);
  const int wifiEntryIndex = functionEntryOffset + functionEntryCount;
  const int backEntryIndex = wifiEntryIndex + 1;
  const int totalCount = backEntryIndex + 1;

  oled.setFont(smallFont);
  int y = 18;
  for(int i = 0; i < totalCount; ++i){
    oled.setCursor(0, y);
    oled.print(i == globalMenuIndex ? ">" : " ");
    if(i < baseCount){
      oled.print(baseItems[i]);
    } else if(i < wifiEntryIndex){
      int slot = i - functionEntryOffset;
      if(active){
        const FunctionActionOption* action = getAssignedAction(*active, slot);
        oled.print("Key "); oled.print(slot + 1); oled.print(": ");
        oled.print(action ? action->name : "--");
      } else {
        oled.print("Key "); oled.print(slot + 1); oled.print(": --");
      }
    } else if(i == wifiEntryIndex){
      oled.print("WiFi: ");
      if(active && active->wifiEnabled){
        oled.print("Enabled");
      } else {
        oled.print("Disabled");
      }
    } else {
      oled.print("Back");
    }
    y += 10;
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

static void formatMotorPercent(float value, char* buffer, size_t size){
  int percent = static_cast<int>(roundf(constrain(value, -1.f, 1.f) * 100.f));
  snprintf(buffer, size, "%+d%%", percent);
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
  drawHeader("The'gill S3");
  oled.setFont(smallFont);
  oled.setCursor(0,14); oled.print("Mode: "); oled.print(modeToString(thegillConfig.mode));
  oled.setCursor(0,22); oled.print("Ease: "); oled.print(easingToString(thegillConfig.easing));
  oled.setCursor(0,30); oled.print("Rate: "); oled.print(thegillRuntime.easingRate, 1);


  float leftTarget = (thegillRuntime.targetLeftFront + thegillRuntime.targetLeftRear) * 0.5f;
  float rightTarget = (thegillRuntime.targetRightFront + thegillRuntime.targetRightRear) * 0.5f;
  float leftActual = (thegillRuntime.actualLeftFront + thegillRuntime.actualLeftRear) * 0.5f;
  float rightActual = (thegillRuntime.actualRightFront + thegillRuntime.actualRightRear) * 0.5f;

  oled.setCursor(0,42); oled.print("Left");
  drawMotorBar(40, 36, leftActual, leftTarget);
  drawMotorBarLabels(40, 36, "LF", thegillRuntime.actualLeftFront,
                             "LR", thegillRuntime.actualLeftRear);

  oled.setCursor(0,56); oled.print("Right");
  drawMotorBar(40, 50, rightActual, rightTarget);
  drawMotorBarLabels(40, 50, "RF", thegillRuntime.actualRightFront,
                              "RR", thegillRuntime.actualRightRear);


  oled.setCursor(100,14); oled.print(thegillRuntime.brakeActive ? "BRK" : "   ");
  oled.setCursor(100,22); oled.print(thegillRuntime.honkActive ? "HNK" : "   ");
}

void drawThegillConfig(){
  oled.clearBuffer();
  drawHeader("Gill Config");
  oled.setFont(smallFont);
  const char* labels[] = {"Mode", "Easing", "Back"};
  const char* values[] = {
    modeToString(thegillConfig.mode),
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
  oled.setFont(smallFont);
  oled.setCursor(0,8);  oled.print("Alt:"); oled.print(telemetry.altitude);
  oled.setCursor(0,16); oled.print("Thr:"); oled.print(emission.throttle);
  oled.setCursor(0,24); oled.print("P:");   oled.print(emission.pitchAngle);
  oled.setCursor(0,32); oled.print("R:");   oled.print(emission.rollAngle);
  oled.setCursor(0,40); oled.print("Y:");   oled.print(emission.yawAngle);
}

void drawConnectionLog(){
  oled.clearBuffer();
  drawHeader("Link Log");
  oled.setFont(smallFont);

  const int visibleLines = 6;
  size_t count = connectionLogGetCount();
  int maxOffset = 0;
  if(count > static_cast<size_t>(visibleLines)){
    maxOffset = static_cast<int>(count) - visibleLines;
  }
  if(logScrollOffset < 0){
    logScrollOffset = 0;
  }
  if(logScrollOffset > maxOffset){
    logScrollOffset = maxOffset;
  }

  if(count == 0){
    oled.setCursor(0, 24);
    oled.print("Waiting for events...");
  } else {
    size_t startIndex = 0;
    if(count > static_cast<size_t>(visibleLines)){
      startIndex = static_cast<size_t>(count - visibleLines - logScrollOffset);
    }
    for(int i = 0; i < visibleLines; ++i){
      size_t index = startIndex + static_cast<size_t>(i);
      if(index >= count){
        break;
      }
      const char* entry = connectionLogGetEntry(index);
      if(!entry){
        continue;
      }
      oled.setCursor(0, 22 + i * 8);
      oled.print(entry);
    }
  }

  oled.setCursor(0, 62);
  oled.print("Press: Pairing");
  oled.sendBuffer();
}

static void drawDashboardOverlays(){
  ModuleState* active = getActiveModule();
  if(!active) return;
  auto drawChip = [&](int16_t x, int16_t y, const char* label, bool focused){
    const int16_t width = 34;
    const int16_t height = 12;
    if(focused){
      oled.setDrawColor(1);
      oled.drawRBox(x, y, width, height, 3);
      oled.setDrawColor(0);
    } else {
      oled.setDrawColor(1);
      oled.drawRFrame(x, y, width, height, 3);
    }
    oled.setFont(smallFont);
    int16_t textWidth = oled.getUTF8Width(label);
    int16_t textX = x + (width - textWidth)/2;
    if(textX < x + 2) textX = x + 2;
    oled.setCursor(textX, y + height - 3);
    oled.print(label);
    oled.setDrawColor(1);
  };

  const int16_t chipX = screen_Width - 36;
  drawChip(chipX, 0, "Menu", dashboardFocusIndex == 1);
  drawChip(chipX, 14, "WiFi", dashboardFocusIndex == 2);

  // WiFi indicator dot
  int16_t dotX = chipX + 26;
  int16_t dotY = 20;
  if(active->wifiEnabled){
    oled.drawDisc(dotX, dotY, 2);
  } else {
    oled.drawCircle(dotX, dotY, 2);
  }

  // Reserve space for function bar and hardware button strip
  const int16_t statusStripHeight = 3;
  const int16_t functionBarHeight = 9;
  const int16_t barY = screen_Height - statusStripHeight - functionBarHeight;
  oled.setDrawColor(0);
  oled.drawBox(0, barY, screen_Width, functionBarHeight + statusStripHeight);
  oled.setDrawColor(1);

  const int16_t slotWidth = screen_Width / static_cast<int16_t>(kMaxFunctionSlots);
  for(size_t slot = 0; slot < kMaxFunctionSlots; ++slot){
    const FunctionActionOption* action = getAssignedAction(*active, slot);
    const char* label = action ? action->shortLabel : "---";
    bool activeState = getFunctionOutput(*active, slot);
    bool focused = dashboardFocusIndex == static_cast<int>(slot) + 3;
    int16_t x = static_cast<int16_t>(slot * slotWidth);
    int16_t width = (slot == kMaxFunctionSlots - 1) ? screen_Width - x : slotWidth;
    int16_t outerWidth = width - 1;
    if(focused){
      oled.drawRFrame(x, barY, outerWidth, functionBarHeight, 2);
    } else {
      oled.drawFrame(x, barY, outerWidth, functionBarHeight);
    }

    int16_t innerX = x + 2;
    int16_t innerY = barY + 2;
    int16_t innerWidth = outerWidth - 3;
    int16_t innerHeight = functionBarHeight - 4;
    if(innerWidth < 2) innerWidth = 2;
    if(innerHeight < 2) innerHeight = 2;

    if(activeState){
      oled.drawBox(innerX, innerY, innerWidth, innerHeight);
      oled.setDrawColor(0);
    } else {
      oled.drawFrame(innerX, innerY, innerWidth, innerHeight);
    }

    oled.setFont(smallFont);
    int16_t labelWidth = oled.getUTF8Width(label);
    int16_t textX = x + (outerWidth - labelWidth) / 2;
    if(textX < x + 2) textX = x + 2;
    int16_t textY = barY + functionBarHeight - 2;
    oled.setCursor(textX, textY);
    oled.print(label);
    oled.setDrawColor(1);
  }

  // Hardware button state strip
  const int16_t stripY = screen_Height - statusStripHeight;
  const uint8_t buttonPins[kMaxFunctionSlots] = {button1, button2, button3};
  for(size_t slot = 0; slot < kMaxFunctionSlots; ++slot){
    int16_t x = static_cast<int16_t>(slot * slotWidth);
    int16_t width = (slot == kMaxFunctionSlots - 1) ? screen_Width - x : slotWidth;
    int16_t outerWidth = width - 1;
    oled.drawFrame(x, stripY, outerWidth, statusStripHeight);
    bool pressed = digitalRead(buttonPins[slot]) == LOW;
    if(pressed){
      oled.drawBox(x + 1, stripY + 1, outerWidth - 2, statusStripHeight - 2);
    }
  }
}

void drawDashboard(){
  if(!discovery.hasPeers()){
    drawConnectionLog();
    return;
  }
  ModuleState* active = getActiveModule();
  if(active && active->descriptor && active->descriptor->drawDashboard){
    active->descriptor->drawDashboard();
  } else {
    drawGenericDashboard();
  }
  drawDashboardOverlays();
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
