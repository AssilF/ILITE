#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoOTA.h>
#include <u8g2lib.h>
#include <Wire.h>
#include <DacESP32.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "espnow_discovery.h"

//debug interface
#define verboseEn 1
#if verboseEn == 1
#define debug(x) Serial.print(x);
#define verboseON Serial.begin(115200);
#else
#define debug(x) 
#define verboseON
#endif

// WiFi credentials used for OTA updates. Replace with actual network values.
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

//Pin Definitions:
#define buzzer_Pin GPIO_NUM_26//Takes Tunes
#define encoderA 16 //clock
#define encoderB 17 //data
#define encoderBtn 18 //btn ofc
#define joystickBtnA 19
#define joystickBtnB 13
#define joystickA_X 35
#define joystickA_Y 34
#define joystickB_X 39
#define joystickB_Y 36
#define button1 23
#define button2 25
#define button3 27
#define potA 32 
#define battery_pin 33 //POTB replaced with battery. . .
#define potB 4 //this pin will compare a threshhold, 2S batteries when depleted should be around 6.9volts
#define LED_Pin 2     //by setting this as a reference for an internal comparator, we should get a controller low battery flag.

//other definitions:
#define deadtime 50 //we'll keep the deadtime @ 50 milliseconds as a cheap move against button bounce, note that we still used hardware debounce for the encoder 
                    //(where a software debounce would consume some considerable amount of clock cycles even with polling @ 240MHz, and introducing a deadtime would skip some encoder cycles or worse. . .reading wrong direction).
#define screen_Width 128 //px
#define screen_Height 64

//Instances
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0);
DacESP32 buzzer(buzzer_Pin);
EspNowDiscovery discovery;

//tasks at the time being, we have no need for this it seems. . . 
xTaskHandle joystickPollTask = NULL;
xTimerHandle encoderActionPollTimer = NULL;
xTaskHandle displayEngine = NULL;

//interrupt routines and vars
volatile bool          button1State;
volatile bool          button2State;
volatile bool          button3State;
volatile bool          joystickBtnAState;
volatile bool          encoderBtnState;
volatile int           encoderCount=0;
volatile unsigned long button1Millis;
volatile unsigned long button2Millis;
volatile unsigned long button3Millis;
volatile unsigned long joystickBtnAMillis;
volatile unsigned long encoderBtnMillis;

bool isbeeping=1;

void IRAM_ATTR button1ISR()
{
  if(millis()-button1Millis>=deadtime){ button1State=1;  button1Millis=millis();} //falling edge for all buttons
}

void IRAM_ATTR button2ISR()
{
  if(millis()-button2Millis>=deadtime){ button2State=1;  button2Millis=millis();}
}

void IRAM_ATTR button3ISR()
{
  if(millis()-button3Millis>=deadtime){ button3State=1; button3Millis=millis();}
}

void IRAM_ATTR joystickBtnAISR()
{
  if(millis()-joystickBtnAMillis>=deadtime){ joystickBtnAState=1; joystickBtnAMillis=millis();}
}

void IRAM_ATTR encoderBtnISR()
{
  if(millis()-encoderBtnMillis>=deadtime){  encoderBtnState=1;  encoderBtnMillis=millis();}
}

void IRAM_ATTR encoderClockISR() //no need to put deadtime since we have a hardware debouncer
{
if(digitalRead(encoderB)==1){encoderCount++;}else{encoderCount--;}
Serial.println(encoderCount);
}

bool botmode=0;
bool lastbtn;

void check_Press(){
//  if(digitalRead(button1) == 1){button1State=0;}
//   if(digitalRead(button2) == 1){button2State=0; lastbtn=0;}
//   if(digitalRead(button3) == 1){button3State=0;}
//   if(digitalRead(encoderBtn) == 0){encoderBtnState=0;}
//   if(digitalRead(joystickBtnA) == 1){joystickBtnAState=0;}
}

//Display business
#define displayWidth 128
#define displayHeight 64
#define batteryCorner 64-25,5,50,20 
#define iconHeight 16
#define iconWidth  16
#define iconClearance 1
#define iconFrameHeight iconHeight+2*iconClearance //widthxheight the display can hold 9 of those frames:
#define iconFrameWidth iconWidth+2*iconClearance
#define iconDeckx (displayWidth-(displayWidth/iconFrameWidth))/2

#define iconFont u8g2_font_open_iconic_all_2x_t
#define networkBatteryIconFont u8g2_font_siji_t_6x10
#define textFont u8g2_font_torussansbold8_8r 

//Settings icons
#define compassIcon "\u0087"
#define speedIcon "\u010d"  //make sure to save this stuff in the eeprom 
#define recordPathIcon "\u00ac"

//modes Icons
#define visionIcon "\u00a5" //although the one in 0091 may seem cuter
#define avoidanceIcon "\u00f2"
#define radioIcon "\u00f7"
#define lineIcon "\u00dd"

//Status Icons
#define fireIcon "\u00a8"
#define proximityIcon "\u0119"
#define alertIcon "\u0118"

//Peripheral Icon
#define pumpIcon "\u0098"
#define illegalIcon "\u0057"

//Coms Icons
#define sentIcon "\u0077"
#define checkIcon "\u0073"
#define receivedIcon "\u0074"

//Power Status Icons Remote
#define batteryDepletedIcon "\ue236" //This is for the remote
#define batteryHalfIcon "\ue237" //for the network and battery, use siji
#define batteryFullIcon "\ue238"

//Power Status Icons Rover
#define batteryNA "\ue211"
#define battery1 "\ue242" //This is for the robot
#define battery2 "\ue243"
#define battery3 "\ue244"
#define battery4 "\ue245"
#define battery5 "\ue246"
#define battery6 "\ue247"
#define battery7 "\ue248"
#define battery8 "\ue249"
#define battery9 "\ue24a"
#define battery10 "\ue24b"

int menuIndex; //stores which icon is being modified
int settingsIndex; //stores the state of the settings 

String deckIcons[] = {};

void drawStatus() //use draw Vline to seperate regions
{
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

void drawMenu()
{

}



byte batteryLevel;

void drawBattery()
{

}




void drawGyroLevel()
{

}

void drawCompassStatus()
{

}



byte Front_Distance;
byte Bottom_Distance;

void drawProximity()
{
 //Us bot
 oled.setDrawColor(1);
 oled.drawRFrame(14,36,12,28,3);
 oled.drawBox(16,57,8,map(Front_Distance,0,50,27,0));
 //Us Front
 oled.setDrawColor(1);
 oled.drawRFrame(1,36,12,28,3);
 oled.drawBox(3,57,8,map(Bottom_Distance,0,50,27,0));
}



byte speed;
byte speedTarget;

void drawSpeed() //actual speed = numbers, joystick speed = blue, accel = checker blue
{
  oled.setFont(textFont);
  oled.setDrawColor(2);
  oled.setCursor(56, 58);
  oled.print(speed); 
  oled.setDrawColor(2);
  oled.drawRFrame(28,48,71,12,4);
  oled.drawBox(30,50,map(speed,0,100,0,67),8); // max = 67
  oled.drawLine(32, 61, map(analogRead(potA),0,4096,32,80), 61);
}


void doNothing(){}
static byte linePosition = B00001010;

void drawLine(){
  oled.setDrawColor(1);
  oled.drawRFrame(1,11,20,15,3);
  (linePosition>>3)? oled.drawBox(3,13,3,11): doNothing();
  ((linePosition>>2)&1)? oled.drawBox(7,13,3,11): doNothing();
  ((linePosition>>1)&1)? oled.drawBox(12,13,3,11): doNothing();
  ((linePosition&1)&1)? oled.drawBox(16,13,3,11): doNothing();
}





static byte firePose = 90;
void drawFirePosition(){
  oled.setDrawColor(1);
  oled.drawRFrame(28,35,71,12,4);
  oled.setFont(u8g2_font_open_iconic_all_1x_t);
  oled.drawGlyph(map(firePose,0,180,80,32), 45, 0x00a8);
}


void tirminal () {
  oled.drawRFrame(28,1,71,33,6);
}

int botX;
int botY;
byte botMotionState;

//motor defs
#define Motor_Left_Top B11000000 //Mask the target Motor only
#define Motor_Left_Bot B00110000
#define Motor_Right_Top B00001100
#define Motor_Right_Bot B00000011

#define MOVE_FRONT B10101010
#define MOVE_BACK B01010101
#define ROTATE_LEFT B10101111
#define ROTATE_RIGHT B11111010
#define TURN_LEFT B10100101
#define TURN_RIGHT B01011010
#define ROTATE_LEFT_BACK B01011111
#define ROTATE_RIGHT_BACK B11110101
#define TURN_LEFT_BACK B01011010
#define TURN_RIGHT_BACK B10100101
#define BREAK B11111111
#define STOP B00000000


//temporary//motion


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


//Motion Calcs
static int speedFactor;

//Addresses and statics
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; //This is the MAC universal broadcast address.
uint8_t targetAddress[] = {0x50, 0x78, 0x7D, 0x45, 0xD9, 0xF0};  //this MAC 78:21:84:7E:68:1C new one: 78:21:84:80:3C:40 Jerray: CC:7B:5C:26:DA:30 DRONGAZ C3 50:78:7D:45:D9:F0
static esp_now_peer_info bot;

static bool sent_Status;
static bool receive_Status;

//packet structs; these packets must NOT exceed 250bytes @ all costs!
//Sent Packets
/*
typedef struct __attribute__((packed)) {
  int8_t speed;   // -100 to 100
  int8_t turn;    // -100 to 100
  bool isEmergencyStop; // true if emergency stop is pressed
} ControlPacket;
*/
// Packet used to control Drongaz. Values are kept fairly small so the
// packet remains well under the 250 byte ESP-NOW limit.
struct emissionDataPacket
{
    // Target altitude command. The value ranges from 0-2000 and is
    // interpreted by the flight controller as the desired altitude
    // setpoint. When the joystick is released the craft should attempt to
    // hold this altitude using its own PID controller.
    uint16_t altitude;
    // Desired attitude angles in degrees.
    int16_t pitchAngle;
    int16_t rollAngle;
    int16_t yawAngle;
    // Motor arming flag.
    bool arm_motors;
}emission; // packet to be emitted

struct receptionDataPacket
{
  byte INDEX;
  byte statusByte;
  int dataByte[8];
  byte okIndex;
}reception;

// Telemetry packet received from the drone containing the key
// stability variables. All values are kept small to remain well
// under the 250 byte ESP-NOW limit.
struct telemetryPacket
{
  int16_t pitch;      // Current pitch angle in degrees
  int16_t roll;       // Current roll angle in degrees
  int16_t yaw;        // Current yaw angle in degrees
  int16_t altitude;   // Current altitude in centimetres
  int16_t pidPitch;   // PID correction for pitch
  int16_t pidRoll;    // PID correction for roll
  int16_t pidYaw;     // PID correction for yaw
  int16_t accelZ;     // Vertical acceleration (Z axis)
}telemetry;

// History buffers for drawing PID graphs on the OLED. Each buffer
// stores the last screen_Width samples so the graph spans the full
// display width.
int16_t pidPitchHistory[screen_Width];
int16_t pidRollHistory[screen_Width];
int16_t pidYawHistory[screen_Width];

// Index of the newest sample in the history buffers.
// Current display mode toggled with the encoder push button.
// 0 - information screen
// 1 - PID correction graphs
// 2 - 3D attitude cube with vertical acceleration arrow
byte displayMode = 0;
// Index of selected peer in the pairing screen.
int selectedPeer = 0;
int lastEncoderCount = 0;

// Accumulated altitude target and yaw command. Joystick deflection adjusts
// these values incrementally so altitude and yaw are controlled by rate
// rather than absolute joystick position.
int16_t altitudeTarget = 0;
int16_t yawCommand     = 0;

//Coms Fcns
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  // Serial.print("\r\nLast Packet Send Status:\t");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  // If this is a new device, pair with it and remember its address.
  bool isNewPeer = !esp_now_is_peer_exist(mac);
  discovery.handleIncoming(mac, incomingData, len);
  if (isNewPeer) {
    memcpy(targetAddress, mac, 6);
  }

  // Copy telemetry data from the incoming packet. The drone is expected to
  // send a telemetryPacket structure defined above.
  memcpy(&telemetry, incomingData, sizeof(telemetry));

  // Update PID history buffers for the graphing screen. The oldest sample is
  // discarded and the newest sample appended to the end of each buffer.
  memmove(pidPitchHistory, pidPitchHistory + 1, (screen_Width - 1) * sizeof(int16_t));
  memmove(pidRollHistory,  pidRollHistory  + 1, (screen_Width - 1) * sizeof(int16_t));
  memmove(pidYawHistory,   pidYawHistory   + 1, (screen_Width - 1) * sizeof(int16_t));
  pidPitchHistory[screen_Width - 1] = telemetry.pidPitch;
  pidRollHistory[screen_Width - 1]  = telemetry.pidRoll;
  pidYawHistory[screen_Width - 1]   = telemetry.pidYaw;
}

// Simple UI for displaying the current command values for Drongaz.
// The interface shows numeric values and basic bar graphs for the
// altitude and attitude commands on the small OLED display.
void drawDrongazInterface(){
  oled.clearBuffer();
  oled.setFont(textFont);

  // Altitude value and bar graph
  oled.setCursor(0,10);
  oled.print("Alt:");
  oled.print(emission.altitude);
  oled.drawFrame(0,14,screen_Width,6);
  oled.drawBox(0,14,map(emission.altitude,0,2000,0,screen_Width),6);

  // Pitch and roll values
  oled.setCursor(0,30);
  oled.print("P:");
  oled.print(emission.pitchAngle);
  oled.setCursor(64,30);
  oled.print("R:");
  oled.print(emission.rollAngle);

  // Yaw value
  oled.setCursor(0,45);
  oled.print("Y:");
  oled.print(emission.yawAngle);

  oled.sendBuffer();
}

// -----------------------------------------------------------------------------
// Telemetry display helpers
// -----------------------------------------------------------------------------

// Draw a simple information screen with numeric telemetry values.
void drawTelemetryInfo(){
  oled.clearBuffer();
  oled.setFont(textFont);
  oled.setCursor(0,10);  oled.print("Alt:");  oled.print(telemetry.altitude);
  oled.setCursor(0,25);  oled.print("P:");    oled.print(telemetry.pitch);
  oled.setCursor(64,25); oled.print("R:");    oled.print(telemetry.roll);
  oled.setCursor(0,40);  oled.print("Y:");    oled.print(telemetry.yaw);
  oled.setCursor(64,40); oled.print("Acc:");  oled.print(telemetry.accelZ);
  // Connection status icon in the top right
  oled.setFont(iconFont);
  oled.setCursor(112,10);
  if(discovery.hasPeers()){
    oled.print(checkIcon);
  }else{
    oled.print(alertIcon);
  }
  oled.sendBuffer();
}

// Draw rolling graphs of the PID corrections for pitch, roll and yaw.
void drawPidGraphs(){
  oled.clearBuffer();

  for(int x=1; x<screen_Width; x++){
    // Pitch graph (top third)
    oled.drawLine(x-1, map(pidPitchHistory[x-1], -500, 500, 20, 0),
                  x,   map(pidPitchHistory[x],   -500, 500, 20, 0));
    // Roll graph (middle third)
    oled.drawLine(x-1, map(pidRollHistory[x-1],  -500, 500, 42, 22),
                  x,   map(pidRollHistory[x],    -500, 500, 42, 22));
    // Yaw graph (bottom third)
    oled.drawLine(x-1, map(pidYawHistory[x-1],   -500, 500, 64, 44),
                  x,   map(pidYawHistory[x],     -500, 500, 64, 44));
  }

  // Divider lines between graphs
  oled.drawLine(0,21,screen_Width,21);
  oled.drawLine(0,43,screen_Width,43);
  oled.sendBuffer();
}

// Draw a 3D cube representing the drone's attitude. Vertical acceleration is
// displayed as an arrow at the centre of the cube.
void drawOrientationCube(){
  oled.clearBuffer();

  const float size = 15.0f;
  const int cx = screen_Width/2;
  const int cy = screen_Height/2;
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

    // Rotation around X (roll)
    float y1 = y*cos(roll) - z*sin(roll);
    float z1 = y*sin(roll) + z*cos(roll);
    y = y1; z = z1;
    // Rotation around Y (pitch)
    float x1 = x*cos(pitch) + z*sin(pitch);
    float z2 = -x*sin(pitch) + z*cos(pitch);
    x = x1; z = z2;
    // Rotation around Z (yaw)
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

  // Vertical acceleration arrow
  int arrowLen = map(telemetry.accelZ, -1000, 1000, -20, 20);
  oled.drawLine(cx, cy, cx, cy - arrowLen);
  if(arrowLen != 0){
    int head = cy - arrowLen;
    oled.drawTriangle(cx, head, cx-3, head + (arrowLen>0?-5:5),
                      cx+3, head + (arrowLen>0?-5:5));
  }

  oled.sendBuffer();
}

// Simple pairing menu displaying connected peers. Press the encoder
// button to trigger a new discovery broadcast.
void drawPairingMenu(){
  oled.clearBuffer();
  oled.setFont(textFont);
  oled.setCursor(0,10);
  oled.print("Pairing");

  int count = discovery.getPeerCount();
  if(count == 0){
    oled.setCursor(0,30);
    oled.print("No peers");
  } else {
    for(int i=0;i<count && i<4;i++){
      const uint8_t *mac = discovery.getPeer(i);
      oled.setCursor(0, 20 + i*12);
      if(i==selectedPeer) oled.print(">"); else oled.print(" ");
      char buf[18];
      sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
              mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
      oled.print(buf);
    }
  }

  oled.setCursor(0,60);
  oled.print("Press to scan");
  oled.sendBuffer();
}



//=============================================================================================================================================================================================
//Menu Goods 
int LM_Line=99;
int RM_Line=999;
int L_Line=99;
int R_Line=99;
int LM_Threshold;
int RM_Threshold;
int L_Threshold;
int R_Threshold;
int Line_Mode;
  ///////Pid_Tuner///////
int LKp;
int LKd;
int Base_speed;
///////Fire_detection////////
int IRBais;
int LFirsensor;
int RFirsensor;
int Detection_Tolerance;
int Detection_Distance;

byte menuCount = 1;
bool runState = false;

int screen = 1;


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
   // Set font

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

  oled.setCursor(10, 60);
  oled.print("LineMode:");
  oled.setCursor(80, 60);
  oled.print(Line_Mode);




  oled.setCursor(2, (menuCount * 10) + 10);
  oled.print(">");
}


void Pid_Tuner() {
  oled.setFont(u8g2_font_6x12_mr);; // Set font

  oled.setCursor(10, 20);
  oled.print("LKp:");
  oled.setCursor(80, 20);
  oled.print(LKp);

  oled.setCursor(10, 30);
  oled.print("LKd:");
  oled.setCursor(80, 30);
  oled.print(LKd);

  oled.setCursor(10, 40);
  oled.print("BaiseSpeed:");
  oled.setCursor(80, 40);
  oled.print(Base_speed);
  oled.setCursor(2, (menuCount * 10) + 10);
  oled.print(">");
}


void Fire_detection() {
  oled.setFont(u8g2_font_6x12_mr);; // Set font

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


  oled.setCursor(2, (menuCount * 10) + 10);
  oled.print(">");
}

void displayMenu()
{
  if (screen==1){
//////Line_detection////////
  Line_detection();
  irData ();
}
if (screen==2){
///////Pid_Tuner///////
  Pid_Tuner();
}
if (screen==3){
///////Fire_detection////////
  Fire_detection();
}
}

byte botSpeed;

void processSpeed(int a)
{
  //if(a<8){botMotionState=STOP;botSpeed=0;}
  //else{
    botSpeed=map(a,0,4096,-100,100);
  //}
}

void processJoyStickA(int a, int b)
{
  if(a>=3000)
  {
    if(b>=3000)
    {botMotionState=ROTATE_LEFT_BACK;}else
    if(b>=1500){botMotionState=MOVE_BACK;}else
    {botMotionState=ROTATE_RIGHT_BACK;}
  }else
  if(a>=1500)
  {
    if(b>=3000)
    {botMotionState=TURN_LEFT;}else
    if(b>=1500){botMotionState=BREAK;}else
    {botMotionState=TURN_RIGHT;}
  }else
  {
    if(b>=3000)
    {botMotionState=ROTATE_LEFT;}else
    if(b>=1500){botMotionState=MOVE_FRONT;}else
    {botMotionState=ROTATE_RIGHT;}
  }
  processSpeed(analogRead(potA));
}



void processComsData(byte index)

{
  switch (index)
  {
  case 0:
  linePosition   = reception.dataByte[0];
  Front_Distance = reception.dataByte[1];
  Bottom_Distance= reception.dataByte[2];
  firePose       = reception.dataByte[3];
  speed          = reception.dataByte[4];  
  batteryLevel   = reception.dataByte[5];
  break;
  
  default:
  break;
  }
}

// void packData(byte index)
// {
//   switch (index)
//   {
//   case 0:
//   emission.motionMode = botMotionState;
//   emission.botMode = botmode;
//   emission.butt1 = digitalRead(button1);
//   break;

//   case 1:
//   emission.a = LM_Threshold;
//   emission.b  = L_Threshold;
//   emission.x = R_Threshold;
//   emission.y     = RM_Threshold;
//   emission.butt1 = Line_Mode; 
//   break;

//   case 2:

//   emission.x=LKd;
//   emission.y = LKp;
//   emission.a = Base_speed;
//   break;

//     case 3:
//   emission.x = Detection_Distance;

//   break;


  
//   default:
//   break;
//   }
// }

//Peripheral Functions
// void readJoystick()
// {
//   emission.x=analogRead(joystickA_X); //default analog settings for joystick : 1960 default, 100-4096
//   emission.y=analogRead(joystickA_Y);
//   speedFactor = map(analogRead(potA),1,4096,0,12)*10; 

//   emission.x =map(emission.x> 2000 || emission.x<1600? emission.x:1940,0,4096,-speedFactor,speedFactor); 
//   if(!joystickBtnAState)
//   {   speedFactor = speedFactor*0.6;
//       emission.y = map(emission.y > 2000 || emission.y<1600? emission.y:1940,0,4096,-speedFactor,speedFactor);
//   }else{emission.y = map(emission.y > 2000 || emission.y<1600? emission.y:1940,0,4096,-speedFactor,speedFactor);}
//   emission.y = emission.y<= 0? emission.y < -15? emission.y: 0 : emission.y> 15 ? emission.y : 0;
//   emission.x = emission.x<= 0? emission.x < -15? emission.x: 0 : emission.x > 15 ? emission.x : 0;
//   emission.y=constrain(emission.y,-99,99);
//   emission.x=constrain(emission.x,-99,99);
// }

void setup() {

  //Pin directions ===================
  pinMode(buzzer_Pin,OUTPUT);
  pinMode(encoderA,INPUT_PULLUP);
  pinMode(encoderB,INPUT_PULLUP);
  pinMode(button1,INPUT_PULLUP);
  pinMode(button2,INPUT_PULLUP);
  pinMode(button3,INPUT_PULLUP);
  pinMode(joystickBtnA,INPUT_PULLUP);
  pinMode(encoderBtn,INPUT_PULLUP);
  pinMode(joystickA_X,INPUT);
  pinMode(joystickA_Y,INPUT);
  pinMode(joystickB_X,INPUT);
  pinMode(joystickB_Y,INPUT);
  pinMode(potA,INPUT);
  //==================================

  //Init Verbose =====================
  verboseON
  debug("we talkin big times G ! \n")
  //==================================

  //Init Display =====================
  oled.begin();
  oled.setFont(textFont);
  //==================================

  //Init PWM drivers =================
  ledcSetup(0,1000,12);
  ledcAttachPin(LED_Pin,0);
  //==================================

  //CPU config =======================
  // setCpuFrequencyMhz(240);
  debug(getCpuFrequencyMhz())
  //==================================

  //Init Wifi & ESPNOW ===============
  WiFi.mode(WIFI_STA); //just in case this is what helped with the uncought load prohibition exception.
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(500);
  }

  ArduinoOTA.onStart([]() {
    Serial.println("OTA Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  debug("\nwifi loaded\n");
  if (esp_now_init() != ESP_OK) {
  debug("ESPnow said no :(")
  while(1){oled.drawStr(0,15,"ESPNOW FAILED :(");  oled.sendBuffer(); delay(5000);}
  }debug("\n\nespnow initialized\n")

  esp_now_register_send_cb(OnDataSent); debug("sending callback set \n \n")//Sent Callback Function associated
  memcpy(bot.peer_addr, targetAddress, 6); //putting the bot ID card into memory ;)
  bot.channel = 0;     
  bot.encrypt = false; //data won't be encrypted so we don't waste too much cpu juice

  if (esp_now_add_peer(&bot) != ESP_OK){
  debug("What the bot doin ?");
  while(1){oled.drawStr(0,15,"someting wong wit ya bot ID");  oled.sendBuffer();delay(5000);}
  }debug("peer added \n")

  esp_now_register_recv_cb(OnDataRecv); debug("reception callback set \n \n")//Recv Callback Function associated

  oled.clear();
  oled.sendBuffer();
  oled.drawStr(0,32,"Found'e bot ;)");
  oled.sendBuffer();
  delay(1000);debug("Coms in place\n\n")
  discovery.begin();
  discovery.discover();
  //==================================

  //Interrupts setting ups ===========
  // attachInterrupt(joystickBtnA,joystickBtnAISR,FALLING);
  attachInterrupt(encoderBtn,encoderBtnISR,RISING);
  // attachInterrupt(button1,button1ISR,FALLING);
  // attachInterrupt(button2,button2ISR,FALLING);
  // attachInterrupt(button3,button3ISR,FALLING);
  attachInterrupt(encoderA,encoderClockISR,RISING);
  //==================================

  //DAC inits ========================
  buzzer.enable();
  ledcWrite(0,4096);
  // buzzer.setCwScale(DAC_CW_SCALE_1); //3.3 volts amplitude output
  // buzzer.outputCW(440); //Buzz a bit to assert dominance.
  // delay(500);
   ledcWrite(0,1600);
  // buzzer.outputCW(390); //Buzz a bit to assert dominance.
  // delay(600);
   ledcWrite(0,500);
  // buzzer.outputCW(1750); //Buzz a bit to assert dominance.
  // delay(50);

  buzzer.outputCW(300);
  delay(100);
  buzzer.outputCW(600);
  delay(200);
  buzzer.outputCW(720);
  delay(300);
  buzzer.outputCW(0);

  buzzer.disable();
  ledcWrite(0,0);
  oled.clearBuffer();

  //side note about playing the tones as we've designed them, use pointers for efficiency. 
  //==================================
  //Data Config
  //==================================

}

unsigned long beepMillis;
void beep()
{ 
  if(isbeeping==1){
  buzzer.enable();
  buzzer.outputCW(400);
  if(millis()-beepMillis>=200)
  {
    isbeeping=0;
  }
  }else
  {
    buzzer.disable();
  }
}

unsigned long lastBtnModeMillis;
bool btnmode=0;
bool ispressed;

void loop() {
  ArduinoOTA.handle();
  check_Press();
  beep();

  //Send Data Through the Air
  
  processJoyStickA(analogRead(joystickA_Y),analogRead(joystickA_X));
  // debug("\n speed: ");debug(botSpeed)
  // debug("\n bot motionstate: ")debug(String(botMotionState,BIN))
  // debug("\n joystickX : ")debug(analogRead(joystickA_X))
  // debug("\n joystickY : ")debug(analogRead(joystickA_Y))
  // delay(500);
  // displayMenu();
  processComsData(0);

  if(millis()-lastBtnModeMillis >= 200)
  {
    if(!ispressed){
    if(!digitalRead(joystickBtnA))
    {
      btnmode=!btnmode;
      isbeeping=1;
      ispressed=1;
    }}else{
      if(digitalRead(joystickBtnA))
      {
        ispressed=0;
      }
    }
    lastBtnModeMillis=millis();
  }

  if(!digitalRead(encoderBtn))
  {
    ledcWrite(0,4050);
  }else
  {
    ledcWrite(0,0);
  }

  // Change display mode when the encoder button is pressed.
  if(encoderBtnState){
    encoderBtnState = 0;
    if(displayMode == 3){
      discovery.discover();
    } else {
      displayMode = (displayMode + 1) % 4;
      if(displayMode == 3){
        lastEncoderCount = encoderCount;
        selectedPeer = 0;
      }
    }
    isbeeping = 1; // audible feedback
  }

  // When in the pairing menu, use encoder rotation to select peers.
  if(displayMode == 3){
    int delta = encoderCount - lastEncoderCount;
    int count = discovery.getPeerCount();
    if(delta != 0 && count > 0){
      selectedPeer = (selectedPeer + delta) % count;
      if(selectedPeer < 0) selectedPeer += count;
      lastEncoderCount = encoderCount;
    }
  }

  // Populate packet with desired control values
  // Altitude is controlled incrementally: joystick deflection adjusts the
  // accumulated altitude target rather than sending raw throttle. Releasing
  // the joystick commands the craft to hold the new altitude.
  int16_t altDelta = map(analogRead(joystickA_Y),0,4096,-20,20);
  if (abs(altDelta) < 2) altDelta = 0; // small deadband to prevent drift
  altitudeTarget = constrain(altitudeTarget + altDelta, 0, 2000);
  emission.altitude = altitudeTarget;

  // Yaw is controlled incrementally: joystick deflection adjusts the
  // accumulated yaw command rather than setting an absolute angle.
  int16_t yawDelta = map(analogRead(joystickA_X),0,4096,-10,10);
  if (abs(yawDelta) < 2) yawDelta = 0; // small deadband to prevent drift
  yawCommand = constrain(yawCommand + yawDelta, -180, 180);
  emission.yawAngle = yawCommand;

  emission.rollAngle  = map(analogRead(joystickB_X),0,4096,-90,90);
  emission.pitchAngle = map(analogRead(joystickB_Y),0,4096,-90,90);
  emission.arm_motors = btnmode;

  // Update OLED with the selected telemetry view
  switch(displayMode){
    case 0: drawTelemetryInfo(); break;
    case 1: drawPidGraphs(); break;
    case 2: drawOrientationCube(); break;
    case 3: drawPairingMenu(); break;
  }

  // Send packet via ESP-NOW
  if(esp_now_send(targetAddress, (uint8_t *) &emission, sizeof(emission))==ESP_OK)
  {
    sent_Status=1;
  }else{sent_Status=0;}
}
