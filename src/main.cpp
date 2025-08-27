#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <u8g2lib.h>
#include <Wire.h>
#include <DacESP32.h>

//debug interface
#define verboseEn 1
#if verboseEn == 1
#define debug(x) Serial.print(x);
#define verboseON Serial.begin(115200);
#else
#define debug(x) 
#define verboseON
#endif

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
struct emissionDataPacket  
{
    uint16_t throttle;
    int8_t pitchBias;
    int8_t rollBias;
    int8_t yawBias;
    bool arm_motors;
}emission; //dummy packet

struct receptionDataPacket
{
  byte INDEX;
  byte statusByte;
  int dataByte[8];
  byte okIndex;
}reception;

//Coms Fcns
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) 
{
  // Serial.print("\r\nLast Packet Send Status:\t");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&reception, incomingData, sizeof(reception));
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
  oled.clearBuffer();
  drawFirePosition();
  drawLine();
  drawMotionJoystickPose();
  drawPeripheralJoystickPose();
  drawProximity();
  drawSpeed();
  oled.sendBuffer();
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

  // emission.MotionState = botMotionState;
  emission.throttle= (abs(map(analogRead(joystickA_Y),0,4096,-1000,1000))*map(analogRead(potA),0,4096,0,100)*0.01)+1000;
  emission.yawBias= map(analogRead(joystickA_X),0,4096,-100,100);
  //emission.isEmergencyStop= !digitalRead(button1); 
  emission.arm_motors = btnmode;
  emission.rollBias= map(analogRead(joystickB_X),0,4096,-100,100);
  emission.pitchBias= map(analogRead(joystickB_Y),0,4096,-100,100);
  // emission.pitch= map(analogRead(joystickB_Y),0,4096,0,180);

  if(esp_now_send(targetAddress, (uint8_t *) &emission, sizeof(emission))==ESP_OK)
  {
    sent_Status=1;
  }else{sent_Status=0;}
}
