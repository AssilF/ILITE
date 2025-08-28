#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoOTA.h>
#include <DacESP32.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "display.h"
#include "input.h"
#include "telemetry.h"

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
const char* ssid = "ILITE PORT";
const char* password = "ASCE321#";

//Pin Definitions:
#define buzzer_Pin GPIO_NUM_26//Takes Tunes
#define LED_Pin 2     //by setting this as a reference for an internal comparator, we should get a controller low battery flag.

//Instances
DacESP32 buzzer(buzzer_Pin);
EspNowDiscovery discovery;

//tasks at the time being, we have no need for this it seems. . . 
xTaskHandle joystickPollTask = NULL;
xTimerHandle encoderActionPollTimer = NULL;
xTaskHandle displayEngine = NULL;

bool isbeeping=1;

bool botmode=0;
bool lastbtn;



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




//Motion Calcs
static int speedFactor;

//Addresses and statics
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; //This is the MAC universal broadcast address.
uint8_t targetAddress[] = {0x50, 0x78, 0x7D, 0x45, 0xD9, 0xF0};  //this MAC 78:21:84:7E:68:1C new one: 78:21:84:80:3C:40 Jerray: CC:7B:5C:26:DA:30 DRONGAZ C3 50:78:7D:45:D9:F0
static esp_now_peer_info bot;

static bool sent_Status;
static bool receive_Status;



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

void displayTask(void* pvParameters){
  for(;;){
    switch(displayMode){
      case 0: drawHomeMenu(); break;
      case 1: drawTelemetryInfo(); break;
      case 2: drawPidGraph(); break;
      case 3: drawOrientationCube(); break;
      case 4: drawPairingMenu(); break;
    }
    appendPidSample();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {

  //Pin directions ===================
  pinMode(buzzer_Pin,OUTPUT);
  initInput();
  //==================================

  //Init Verbose =====================
  verboseON
  debug("we talkin big times G ! \n")
  //==================================

  //Init Display =====================
  initDisplay();
  xTaskCreatePinnedToCore(displayTask, "display", 4096, NULL, 1, &displayEngine, 1);
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
  WiFi.mode(WIFI_AP_STA); //just in case this is what helped with the uncought load prohibition exception.
  WiFi.softAP(ssid, password);
  Serial.println("Wifi ON");
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
  // Re-enable Soft AP in case discovery initialization resets WiFi.
  WiFi.softAP(ssid, password);
  discovery.discover();
  //==================================

  //Interrupts handled in initInput()
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
  checkPress();
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

  // Automatic discovery while in the pairing menu.
  if(displayMode == 4 && millis() - lastDiscoveryTime > 2000){
    discovery.discover();
    lastDiscoveryTime = millis();
  }

  // Handle encoder rotation depending on the current page.
  if(displayMode == 0){
    int delta = encoderCount - lastEncoderCount;
    int menuCount = 4;
    if(delta != 0){
      homeMenuIndex = (homeMenuIndex + delta) % menuCount;
      if(homeMenuIndex < 0) homeMenuIndex += menuCount;
      lastEncoderCount = encoderCount;
    }
  } else if(displayMode == 4){
    int delta = encoderCount - lastEncoderCount;
    int count = discovery.getPeerCount() + 1; // include Home option
    if(delta != 0 && count > 0){
      selectedPeer = (selectedPeer + delta) % count;
      if(selectedPeer < 0) selectedPeer += count;
      lastEncoderCount = encoderCount;
    }
  } else if(displayMode == 2){
    int delta = encoderCount - lastEncoderCount;
    if(delta != 0){
      pidGraphIndex = (pidGraphIndex + delta) % 3;
      if(pidGraphIndex < 0) pidGraphIndex += 3;
      lastEncoderCount = encoderCount;
    }
  } else {
    int delta = encoderCount - lastEncoderCount;
    if(delta != 0){
      homeSelected = !homeSelected;
      lastEncoderCount = encoderCount;
    }
  }

  // Handle encoder button presses for navigation.
  if(encoderBtnState){
    encoderBtnState = 0;
    if(displayMode == 0){
      switch(homeMenuIndex){
        case 0: displayMode = 1; break;
        case 1: displayMode = 2; break;
        case 2: displayMode = 3; break;
        case 3: displayMode = 4; selectedPeer = 0; break;
      }
      lastEncoderCount = encoderCount;
      homeSelected = false;
    } else if(displayMode == 4){
      int count = discovery.getPeerCount();
      if(selectedPeer == count){
        displayMode = 0;
        homeMenuIndex = 0;
        homeSelected = false;
        lastEncoderCount = encoderCount;
      } else if(count > 0){
        const uint8_t *mac = discovery.getPeer(selectedPeer);
        memcpy(targetAddress, mac, 6);
      }
    } else if(displayMode == 2){
      displayMode = 0;
      homeMenuIndex = 1;
      lastEncoderCount = encoderCount;
    } else {
      if(homeSelected){
        displayMode = 0;
        homeMenuIndex = 0;
        homeSelected = false;
        lastEncoderCount = encoderCount;
      }
    }
    isbeeping = 1; // audible feedback
  }

  // Populate packet with desired control values
  // Altitude is controlled incrementally: joystick deflection adjusts the
  // accumulated altitude target rather than sending raw throttle. Releasing
  // the joystick commands the craft to hold the new altitude.
  int maxClimbRate = map(analogRead(potA), 0, 4095, 0, 200); // 0-50 cm per loop
  int16_t altDelta = map(analogRead(joystickA_Y), 0, 4096, -maxClimbRate, maxClimbRate);
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

  // Display rendering handled in FreeRTOS display task

  // Send packet via ESP-NOW
  if(esp_now_send(targetAddress, (uint8_t *) &emission, sizeof(emission))==ESP_OK)
  {
    sent_Status=1;
  }else{sent_Status=0;}
}
