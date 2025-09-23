#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoOTA.h>
#include <DacESP32.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "display.h"
#include "input.h"
#include "telemetry.h"
#include "thegill.h"

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
xTaskHandle commsEngine = NULL;



bool isbeeping=1;

bool botmode=0;
bool lastbtn;
bool btnmode = false;
unsigned long beepMillis = 0;



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
unsigned long lastReceiveTime = 0;
bool connected = false;



// Accumulated yaw command. Joystick deflection adjusts this value
// incrementally so yaw is controlled by rate rather than absolute
// joystick position.
int16_t yawCommand = 0;

// Global botSpeed variable
int8_t botSpeed = 0;

static bool isNameThegill(const char* name){
  if(name == nullptr) return false;
  size_t len = strlen(name);
  for(size_t i = 0; i + 3 < len; ++i){
    if(tolower(static_cast<unsigned char>(name[i])) == 'g' &&
       tolower(static_cast<unsigned char>(name[i+1])) == 'i' &&
       tolower(static_cast<unsigned char>(name[i+2])) == 'l' &&
       tolower(static_cast<unsigned char>(name[i+3])) == 'l'){
      return true;
    }
  }
  return false;
}

static bool isNameBulky(const char* name){
  if(name == nullptr) return false;
  const char target[] = {'b','u','l','k','y'};
  size_t len = strlen(name);
  if(len < sizeof(target)){
    return false;
  }
  for(size_t i = 0; i + sizeof(target) - 1 < len; ++i){
    bool match = true;
    for(size_t j = 0; j < sizeof(target); ++j){
      if(tolower(static_cast<unsigned char>(name[i + j])) != target[j]){
        match = false;
        break;
      }
    }
    if(match){
      return true;
    }
  }
  return false;
}

static void resetThegillState(){
  thegillRuntime.targetLeftFront = 0.f;
  thegillRuntime.targetLeftRear = 0.f;
  thegillRuntime.targetRightFront = 0.f;
  thegillRuntime.targetRightRear = 0.f;
  thegillRuntime.actualLeftFront = 0.f;
  thegillRuntime.actualLeftRear = 0.f;
  thegillRuntime.actualRightFront = 0.f;
  thegillRuntime.actualRightRear = 0.f;
  thegillRuntime.easingRate = 4.0f;
  thegillRuntime.brakeActive = false;
  thegillRuntime.honkActive = false;
  thegillCommand.magic = THEGILL_PACKET_MAGIC;
  thegillCommand.leftFront = 0;
  thegillCommand.leftRear = 0;
  thegillCommand.rightFront = 0;
  thegillCommand.rightRear = 0;
  thegillCommand.flags = 0;
  thegillCommand.reserved = 0;
  thegillCommand.mode = thegillConfig.mode;
  thegillCommand.easing = thegillConfig.easing;
  thegillCommand.easingRate = 4.0f;
}

static float readJoystickAxis(uint8_t pin, size_t index){
  static int center[4] = {2048, 2048, 2048, 2048};
  static bool initialised[4] = {false, false, false, false};
  if(index >= 4) return 0.f;
  int raw = analogRead(pin);
  if(!initialised[index]){
    center[index] = raw;
    initialised[index] = true;
  }
  int delta = raw - center[index];
  const int deadzone = 80;
  if(abs(delta) <= deadzone){
    center[index] = (center[index] * 15 + raw) / 16;
    return 0.f;
  }
  if(abs(delta) < deadzone * 3){
    center[index] = (center[index] * 31 + raw) / 32;
  }
  float range = delta > 0 ? (4095 - center[index]) : center[index];
  if(range < 1.f) range = 1.f;
  float value = delta / range;
  return constrain(value, -1.f, 1.f);
}

//Coms Fcns
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  debug("Last Packet Send Status: ");
  debug(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success\n" : "Delivery Fail\n");
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  debug("Data received: ");
  debug(len);
  debug(" bytes\n");
  // Ignore any frames from the universal broadcast address to prevent
  // pairing with ourselves when our own discovery packets are received.
  if (memcmp(mac, broadcastAddress, 6) == 0) {
    debug("Ignoring broadcast frame\n");
    return;
  }

  // Let the discovery helper consume any handshake packets.
  if (discovery.handleIncoming(mac, incomingData, len)) {
    debug("Discovery handshake\n");
    // Remember the sender as the current target once paired.
    memcpy(targetAddress, mac, 6);
    lastReceiveTime = millis();
    connected = true;
    int peerIndex = discovery.findPeerIndex(mac);
    if(peerIndex >= 0){
      const char* peerName = discovery.getPeerName(peerIndex);
      bool isBulkyPeer = isNameBulky(peerName);
      bool isGillPeer = isNameThegill(peerName);
      if(isBulkyPeer){
        pairedIsBulky = true;
        pairedIsThegill = false;
        botMotionState = STOP;
        botSpeed = 0;
        bulkyCommand = BulkyCommand{0, 0, STOP, {0, 0, 0}};
      } else if(isGillPeer){
        pairedIsThegill = true;
        pairedIsBulky = false;
        resetThegillState();
      } else {
        pairedIsBulky = false;
        pairedIsThegill = false;
      }
    }
    return;
  }

  // Copy telemetry data from the incoming packet. The drone is expected to
  // send a TelemetryPacket structure defined above.
  if(pairedIsThegill){
    lastReceiveTime = millis();
    connected = true;
    return;
  }
  if(pairedIsBulky){
    size_t copySize = static_cast<size_t>(len);
    if(copySize > sizeof(reception)){
      copySize = sizeof(reception);
    }
    memcpy(&reception, incomingData, copySize);
    lastReceiveTime = millis();
    connected = true;
    return;
  }
  memcpy(&telemetry, incomingData, sizeof(telemetry));
  lastReceiveTime = millis();
  connected = true;

}

void processSpeed(int a)
{
  //if(a<8){botMotionState=STOP;botSpeed=0;}
  //else{
    int mapped = map(a,0,4096,-100,100);
    botSpeed = static_cast<int8_t>(constrain(mapped, -100, 100));
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

static void updateDroneControl();
static void updateThegillControl();
static void updateBulkyControl();

static void updateDroneControl(){
  static uint16_t potFiltered = analogRead(potA);
  potFiltered = (potFiltered * 3 + analogRead(potA)) / 4; // IIR filter
  uint16_t potOffset = map(potFiltered, 0, 4095, 0, 500);
  potOffset = constrain(potOffset, 0, 500);

  emission.throttle = constrain(
      map(analogRead(joystickA_Y), 0, 4095, 2000, -1000) + potOffset,
      1000,
      2000);

  int16_t yawDelta = map(analogRead(joystickA_X),0,4096,-10,10);
  if (abs(yawDelta) < 2) yawDelta = 0; // small deadband to prevent drift
  yawCommand = constrain(yawCommand + yawDelta, -180, 180);
  emission.yawAngle = yawCommand;

  int16_t roll = map(analogRead(joystickB_X), 0, 4095, -90, 90);
  if (abs(roll) < 10) roll = 0; // eliminate small deadzone around center
  emission.rollAngle = roll;

  int16_t pitch = map(analogRead(joystickB_Y), 0, 4095, -90, 90);
  if (abs(pitch) < 10) pitch = 0;
  emission.pitchAngle = pitch;
  emission.arm_motors = btnmode;
}

static void updateThegillControl(){
  static uint32_t lastUpdate = millis();
  uint32_t now = millis();
  float dt = (now - lastUpdate) / 1000.0f;
  if(dt <= 0.f || dt > 0.25f){
    dt = 0.02f;
  }
  lastUpdate = now;

  bool brakePressed = digitalRead(joystickBtnA) == LOW;
  bool honkPressed = digitalRead(joystickBtnB) == LOW;
  thegillRuntime.brakeActive = brakePressed;
  thegillRuntime.honkActive = honkPressed;

  static uint16_t potFiltered = analogRead(potA);
  potFiltered = (potFiltered * 7 + analogRead(potA)) / 8;
  float potNorm = constrain(static_cast<float>(potFiltered) / 4095.0f, 0.f, 1.f);

  float leftTarget = 0.f;
  float rightTarget = 0.f;
  float easingRate = 4.0f;

  if(thegillConfig.mode == GillMode::Default){
    float x = readJoystickAxis(joystickA_X, 0);
    float y = readJoystickAxis(joystickA_Y, 1);
    float speedScale = potNorm;
    leftTarget = constrain(y + x, -1.f, 1.f) * speedScale;
    rightTarget = constrain(y - x, -1.f, 1.f) * speedScale;
    easingRate = 6.0f;
  } else {
    float leftY = readJoystickAxis(joystickA_Y, 1);
    float rightY = readJoystickAxis(joystickB_Y, 3);
    leftTarget = constrain(leftY, -1.f, 1.f);
    rightTarget = constrain(rightY, -1.f, 1.f);
    const float minRate = 1.5f;
    const float maxRate = 10.0f;
    easingRate = minRate + (maxRate - minRate) * potNorm;
  }

  if(brakePressed){
    leftTarget = 0.f;
    rightTarget = 0.f;
    if(easingRate < 12.0f){
      easingRate = 12.0f;
    }
  }

  thegillRuntime.targetLeftFront = leftTarget;
  thegillRuntime.targetLeftRear = leftTarget;
  thegillRuntime.targetRightFront = rightTarget;
  thegillRuntime.targetRightRear = rightTarget;

  auto smooth = [&](float current, float target){
    if(fabsf(target - current) < 0.0005f){
      return target;
    }
    float step = constrain(easingRate * dt, 0.f, 1.f);
    float eased = applyEasingCurve(thegillConfig.easing, step);
    if(step >= 1.f){
      return target;
    }
    return current + (target - current) * eased;
  };

  thegillRuntime.actualLeftFront = smooth(thegillRuntime.actualLeftFront, leftTarget);
  thegillRuntime.actualLeftRear = smooth(thegillRuntime.actualLeftRear, leftTarget);
  thegillRuntime.actualRightFront = smooth(thegillRuntime.actualRightFront, rightTarget);
  thegillRuntime.actualRightRear = smooth(thegillRuntime.actualRightRear, rightTarget);

  if(brakePressed){
    if(fabsf(thegillRuntime.actualLeftFront) < 0.01f) thegillRuntime.actualLeftFront = 0.f;
    if(fabsf(thegillRuntime.actualLeftRear) < 0.01f) thegillRuntime.actualLeftRear = 0.f;
    if(fabsf(thegillRuntime.actualRightFront) < 0.01f) thegillRuntime.actualRightFront = 0.f;
    if(fabsf(thegillRuntime.actualRightRear) < 0.01f) thegillRuntime.actualRightRear = 0.f;
  }

  thegillRuntime.easingRate = easingRate;

  constexpr float scale = 1000.0f;
  thegillCommand.magic = THEGILL_PACKET_MAGIC;
  thegillCommand.mode = thegillConfig.mode;
  thegillCommand.easing = thegillConfig.easing;
  thegillCommand.easingRate = easingRate;
  thegillCommand.leftFront = static_cast<int16_t>(roundf(constrain(thegillRuntime.actualLeftFront, -1.f, 1.f) * scale));
  thegillCommand.leftRear = static_cast<int16_t>(roundf(constrain(thegillRuntime.actualLeftRear, -1.f, 1.f) * scale));
  thegillCommand.rightFront = static_cast<int16_t>(roundf(constrain(thegillRuntime.actualRightFront, -1.f, 1.f) * scale));
  thegillCommand.rightRear = static_cast<int16_t>(roundf(constrain(thegillRuntime.actualRightRear, -1.f, 1.f) * scale));

  uint8_t flags = 0;
  if(brakePressed) flags |= GILL_FLAG_BRAKE;
  if(honkPressed) flags |= GILL_FLAG_HONK;
  thegillCommand.flags = flags;
  thegillCommand.reserved = 0;

  static bool lastHonk = false;
  if(honkPressed && !lastHonk){
    isbeeping = 1;
    beepMillis = now;
  }
  lastHonk = honkPressed;
}

static void updateBulkyControl(){
  processJoyStickA(analogRead(joystickA_Y), analogRead(joystickA_X));
  bulkyCommand.replyIndex = 0;
  bulkyCommand.motionState = botMotionState;
  bulkyCommand.speed = botSpeed;
  bulkyCommand.buttonStates[0] = digitalRead(button1) == LOW ? 1 : 0;
  bulkyCommand.buttonStates[1] = digitalRead(button2) == LOW ? 1 : 0;
  bulkyCommand.buttonStates[2] = digitalRead(button3) == LOW ? 1 : 0;
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
      case 5: drawDashboard(); break;
      case 6: drawAbout(); break;
      case 7: drawPeerInfo(); break;
    }
    appendPidSample();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void commTask(void* pvParameters){
  const TickType_t delay = 10 / portTICK_PERIOD_MS; // 50Hz
  for(;;){
    const uint8_t* payload;
    size_t payloadSize;
    if(pairedIsThegill){
      payload = reinterpret_cast<const uint8_t*>(&thegillCommand);
      payloadSize = sizeof(thegillCommand);
    } else if(pairedIsBulky){
      payload = reinterpret_cast<const uint8_t*>(&bulkyCommand);
      payloadSize = sizeof(bulkyCommand);
    } else {
      payload = reinterpret_cast<const uint8_t*>(&emission);
      payloadSize = sizeof(emission);
    }
    if(esp_now_send(targetAddress, payload, payloadSize)==ESP_OK){
      sent_Status = true;
    }else{
      sent_Status = false;
    }
    vTaskDelay(delay);
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
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
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

  // memcpy(bot.peer_addr, targetAddress, 6); //putting the bot ID card into memory ;)
  // bot.channel = 0;     
  // bot.encrypt = false; //data won't be encrypted so we don't waste too much cpu juice

  // if (esp_now_add_peer(&bot) != ESP_OK){
  // debug("What the bot doin ?");
  // while(1){oled.drawStr(0,15,"someting wong wit ya bot ID");  oled.sendBuffer();delay(5000);}
  // }debug("peer added \n")

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
  xTaskCreatePinnedToCore(commTask, "comms", 4096, NULL, 3, &commsEngine, 0);
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
bool ispressed;

void loop() {
  ArduinoOTA.handle();
  checkPress();
  beep();

  //Send Data Through the Air

  if(pairedIsBulky){
    updateBulkyControl();
    processComsData(0);
  }

  if(connected && millis() - lastReceiveTime > 3000){
    connected = false;
    displayMode = 4;
    selectedPeer = 0;
    discovery.discover();
    lastDiscoveryTime = millis();
    pairedIsBulky = false;
    pairedIsThegill = false;
    botMotionState = STOP;
    botSpeed = 0;
    bulkyCommand = BulkyCommand{0, 0, 0, {0, 0, 0}};
    resetThegillState();
  }

  if(!pairedIsThegill && millis()-lastBtnModeMillis >= 200)
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
    int menuCount = 6;
    if(delta != 0){
      homeMenuIndex = (homeMenuIndex + delta) % menuCount;
      if(homeMenuIndex < 0) homeMenuIndex += menuCount;
      lastEncoderCount = encoderCount;
    }
  } else if(displayMode == 1 && pairedIsThegill){
    int delta = encoderCount - lastEncoderCount;
    if(delta != 0){
      gillConfigIndex = (gillConfigIndex + delta) % 3;
      if(gillConfigIndex < 0) gillConfigIndex += 3;
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
  } else if(displayMode == 5){
    // no rotary action on dashboard
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
    if(displayMode == 5){
      displayMode = 0;
      homeMenuIndex = 0;
      homeSelected = false;
      lastEncoderCount = encoderCount;
    } else if(displayMode == 0){
      switch(homeMenuIndex){
        case 0: displayMode = 5; break; // Dashboard
        case 1: displayMode = 1; gillConfigIndex = 0; break; // Telemetry / Config
        case 2: displayMode = 2; break; // PID Graph
        case 3: displayMode = 3; break; // Orientation
        case 4: displayMode = 4; selectedPeer = 0; break; // Pairing
        case 5: displayMode = 6; break; // About
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
        infoPeer = selectedPeer;
        displayMode = 7;
        homeSelected = false;
        lastEncoderCount = encoderCount;
      }
    } else if(displayMode == 1 && pairedIsThegill){
      if(gillConfigIndex == 0){
        thegillConfig.mode = (thegillConfig.mode == GillMode::Default) ? GillMode::Differential : GillMode::Default;
        resetThegillState();
      } else if(gillConfigIndex == 1){
        int next = (static_cast<int>(thegillConfig.easing) + 1) % 5;
        thegillConfig.easing = static_cast<GillEasing>(next);
        thegillCommand.easing = thegillConfig.easing;
      } else {
        displayMode = 0;
        homeMenuIndex = 1;
        homeSelected = false;
      }
      lastEncoderCount = encoderCount;
    } else if(displayMode == 7){
      if(homeSelected){
        displayMode = 4;
        homeSelected = false;
      }else{
        const uint8_t *mac = discovery.getPeer(infoPeer);
        memcpy(targetAddress, mac, 6);
        const char* peerName = discovery.getPeerName(infoPeer);
        pairedIsBulky = strcmp(peerName, "Bulky") == 0;
        pairedIsThegill = isNameThegill(peerName);
        if(pairedIsBulky){
          botMotionState = STOP;
          botSpeed = 0;
          bulkyCommand = BulkyCommand{0, 0, STOP, {0, 0, 0}};
        } else {
          bulkyCommand = BulkyCommand{0, 0, 0, {0, 0, 0}};
        }
        if(pairedIsThegill){
          resetThegillState();
          gillConfigIndex = 0;
        }
        displayMode = 5;
        homeSelected = false;
      }
      lastEncoderCount = encoderCount;
    } else if(displayMode == 2){
      displayMode = 0;
      homeMenuIndex = 2;
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

  if(!pairedIsBulky){
    // Populate packet with desired control values.  Smooth the potentiometer
    // used for throttle offset and map its full travel to 0–500 units.
    static uint16_t potFiltered = analogRead(potA);
    potFiltered = (potFiltered * 3 + analogRead(potA)) / 4; // IIR filter
    uint16_t potOffset = map(potFiltered, 0, 4095, 0, 800);
    potOffset = constrain(potOffset, 0, 800);

    emission.throttle = constrain(
        map(analogRead(joystickA_Y), 0, 4095, 2000, -1000) + potOffset,
        1000,
        2000);

    // if (rawThrottle <= 2048) {
    //   emission.throttle = 1000;
    // } else {
    //   emission.throttle = map(rawThrottle, 2048, 4095, 1000, 2000);
    // }

    // Yaw is controlled incrementally: joystick deflection adjusts the
    // accumulated yaw command rather than setting an absolute angle.
    int16_t yawDelta = map(analogRead(joystickA_X),0,4096,-10,10);
    if (abs(yawDelta) < 2) yawDelta = 0; // small deadband to prevent drift
    yawCommand = constrain(yawCommand + yawDelta, -180, 180);
    emission.yawAngle = yawCommand;

    int16_t roll = map(analogRead(joystickB_X), 0, 4095, -90, 90);
    if (abs(roll) < 12) roll = 0; // eliminate small deadzone around center
    emission.rollAngle = roll;

    int16_t pitch = map(analogRead(joystickB_Y), 0, 4095, -90, 90);
    if (abs(pitch) < 12) pitch = 0;
    emission.pitchAngle = pitch;
    emission.arm_motors = btnmode;
    if(pairedIsThegill){
      updateThegillControl();
    } else {
      updateDroneControl();
    }
  }

  // Display rendering handled in FreeRTOS display task
}
