#include "display.h"
#include <stdio.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0);

byte displayMode = 5;
int homeMenuIndex = 0;
bool homeSelected = false;
int selectedPeer = 0;
int lastEncoderCount = 0;
unsigned long lastDiscoveryTime = 0;

#define displayWidth 128
#define displayHeight 64
#define batteryCorner 64-25,5,50,20
#define iconHeight 16
#define iconWidth 16
#define iconClearance 1
#define iconFrameHeight iconHeight+2*iconClearance
#define iconFrameWidth iconWidth+2*iconClearance
#define iconDeckx (displayWidth-(displayWidth/iconFrameWidth))/2

#define iconFont u8g2_font_open_iconic_all_2x_t
#define networkBatteryIconFont u8g2_font_siji_t_6x10
#define textFont u8g2_font_torussansbold8_8r
#define smallFont u8g2_font_5x8_tf
#define bootTitleFont u8g2_font_inb38_mr
#define bootSubFont u8g2_font_6x13_tf
#define smallIconFont u8g2_font_open_iconic_all_1x_t

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
  const float size = 15.0f;
  const int cx = screen_Width/2;
  const int cy = screen_Height/2 + 6;
  struct Vec3 { float x,y,z; };
  const Vec3 verts[8] = {
    {-size,-size,-size}, { size,-size,-size}, { size, size,-size}, {-size, size,-size},
    {-size,-size, size}, { size,-size, size}, { size, size, size}, {-size, size, size}
  };
  int px[8];
  int py[8];
  float roll  = telemetry.roll  * DEG_TO_RAD;
  float pitch = telemetry.pitch * DEG_TO_RAD;
  float yaw   = telemetry.yaw   * DEG_TO_RAD;
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
    const uint8_t *mac = discovery.getPeer(i);
    oled.setCursor(0, 22 + i*12);
    if(i==selectedPeer) oled.print(">"); else oled.print(" ");
    char buf[18];
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    oled.print(buf);
  }
  oled.setCursor(0, 22 + count*12);
  if(selectedPeer == count) oled.print(">"); else oled.print(" ");
  oled.print("Home");
  oled.sendBuffer();
}

void drawHomeMenu(){
  oled.clearBuffer();
  drawHeader("Menu");
  oled.setFont(textFont);
  const char* items[] = {"Dashboard","Telemetry","PID Graph","Orientation","Pairing","About"};
  for(int i=0;i<6;i++){
    oled.setCursor(0,22 + i*10);
    if(i==homeMenuIndex) oled.print(">"); else oled.print(" ");
    oled.print(items[i]);
  }
  oled.sendBuffer();
}

void drawHomeFooter(){
  oled.setCursor(0,60);
  if(homeSelected) oled.print(">"); else oled.print(" ");
  oled.print("Home");
}

void drawDashboard(){
  oled.clearBuffer();
  if(!discovery.hasPeers()){
    oled.setFont(smallFont);
    uint8_t w = oled.getUTF8Width("Pair a device");
    oled.setCursor((screen_Width - w)/2, screen_Height/2);
    oled.print("Pair a device");
    oled.sendBuffer();
    return;
  }

  const float size = 15.0f;
  const int cx = screen_Width/2;
  const int cy = screen_Height/2 + 6;
  struct Vec3 { float x,y,z; };
  const Vec3 verts[8] = {
    {-size,-size,-size}, { size,-size,-size}, { size, size,-size}, {-size, size,-size},
    {-size,-size, size}, { size,-size, size}, { size, size, size}, {-size, size, size}
  };
  int px[8];
  int py[8];
  float roll  = telemetry.roll  * DEG_TO_RAD;
  float pitch = telemetry.pitch * DEG_TO_RAD;
  float yaw   = telemetry.yaw   * DEG_TO_RAD;
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
