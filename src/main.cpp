#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoOTA.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "display.h"
#include "connection_log.h"
#include "input.h"
#include "telemetry.h"
#include "thegill.h"
#include "audio_feedback.h"
#include "ui_modules.h"
#include "menu_entries.h"

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
EspNowDiscovery discovery;

//tasks at the time being, we have no need for this it seems. . .
xTaskHandle joystickPollTask = NULL;
xTimerHandle encoderActionPollTimer = NULL;
xTaskHandle displayEngine = NULL;
xTaskHandle commsEngine = NULL;

WifiControlCommand wifiControlCommand{PACKET_MAGIC, 1, {0, 0, 0}};
bool wifiCommandPending = false;
uint8_t wifiCommandTarget[6] = {0};

bool autoDashboardEnabled = true;
ModuleState* lastPairedModule = nullptr;

bool controlSessionActive = false;
byte logReturnMode = DISPLAY_MODE_HOME;



bool botmode=0;
bool lastbtn;
bool btnmode = false;



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
uint8_t targetAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  //this MAC 78:21:84:7E:68:1C new one: 78:21:84:80:3C:40 Jerray: CC:7B:5C:26:DA:30 DRONGAZ C3 50:78:7D:45:D9:F0
static esp_now_peer_info bot;

static bool sent_Status;
static bool receive_Status;
unsigned long lastReceiveTime = 0;
bool connected = false;

static bool dronePrecisionMode = false;
static bool bulkyHonkLatch = false;
static bool bulkyLightLatch = false;
static bool bulkySlowMode = false;
static bool gillHonkLatch = false;
static bool functionButtonLatch[kMaxFunctionSlots] = {false, false, false};



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

static void handleGenericIncoming(ModuleState& state, const uint8_t* data, size_t length);
static void handleBulkyIncoming(ModuleState& state, const uint8_t* data, size_t length);
static void handleGillIncoming(ModuleState& state, const uint8_t* data, size_t length);

//Coms Fcns
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  debug("Last Packet Send Status: ");
  debug(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success\n" : "Delivery Fail\n");
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  // debug("Data received: ");
  // debug(len);
  // debug(" bytes\n");
  // Ignore any frames from the universal broadcast address to prevent
  // pairing with ourselves when our own discovery packets are received.
  // if (memcmp(mac, broadcastAddress, 6) == 0) {
  //   debug("Ignoring broadcast frame\n");
  //   return;
  // }
  // Let the discovery helper consume any handshake packets.
  connectionLogAddf("RX module payload (%d bytes)", len);
  bool handshake = discovery.handleIncoming(mac, incomingData, len);
  if (handshake) {
    debug("Discovery handshake\n");
    int peerIndex = discovery.findPeerIndex(mac);
    if(peerIndex >= 0){
      const char* peerName = discovery.getPeerName(peerIndex);
      ModuleState* module = findModuleByName(peerName);
      if(module){
        lastPairedModule = module;
        if(autoDashboardEnabled && controlSessionActive){
          setActiveModule(module);
        }
      }
      if(module && module->descriptor){
        switch(module->descriptor->kind){
          case PeerKind::Bulky:
            botMotionState = STOP;
            botSpeed = 0;
            bulkyCommand = BulkyCommand{0, 0, STOP, {0, 0, 0}};
            bulkyHonkLatch = false;
            bulkyLightLatch = false;
            bulkySlowMode = false;
            break;
          case PeerKind::Thegill:
            resetThegillState();
            gillHonkLatch = false;
            gillConfigIndex = 0;
            break;
          case PeerKind::Generic:
          default:
            break;
        }
      }
      connectionLogAddf("Active peer: %s", peerName);
    }
    if(controlSessionActive && EspNowDiscovery::macEqual(mac, targetAddress)){
      lastReceiveTime = millis();
      connected = true;
    }
    return;
  }

  // Copy telemetry data from the incoming packet. The drone is expected to
  // send a TelemetryPacket structure defined above.
  bool fromSelectedPeer = controlSessionActive && EspNowDiscovery::macEqual(mac, targetAddress);
  if(!fromSelectedPeer){
    if(!controlSessionActive){
      connected = false;
    }
    return;
  }

  ModuleState* active = getActiveModule();
  if(active == nullptr){
    active = getModuleState(0);
  }
  if(active && active->descriptor){
    if(active->descriptor->handleIncoming){
      active->descriptor->handleIncoming(*active, incomingData, static_cast<size_t>(len));
    } else {
      handleGenericIncoming(*active, incomingData, static_cast<size_t>(len));
    }
  }

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
  if(dronePrecisionMode){
    roll /= 2;
  }
  emission.rollAngle = roll;

  int16_t pitch = map(analogRead(joystickB_Y), 0, 4095, -90, 90);
  if (abs(pitch) < 10) pitch = 0;
  if(dronePrecisionMode){
    pitch /= 2;
  }
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
  bool honkPressed = (digitalRead(joystickBtnB) == LOW) || gillHonkLatch;
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
    audioFeedback(AudioCue::Select);
  }
  lastHonk = honkPressed;
}

static void updateBulkyControl(){
  processJoyStickA(analogRead(joystickA_Y), analogRead(joystickA_X));
  bulkyCommand.replyIndex = 0;
  bulkyCommand.motionState = botMotionState;
  if(bulkySlowMode){
    botSpeed = botSpeed / 2;
  }
  bulkyCommand.speed = botSpeed;
  bulkyCommand.buttonStates[0] = bulkyHonkLatch ? 1 : 0;
  bulkyCommand.buttonStates[1] = bulkyLightLatch ? 1 : 0;
  bulkyCommand.buttonStates[2] = bulkySlowMode ? 1 : 0;
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

static const uint8_t* prepareGenericPayload(size_t& size){
  size = sizeof(emission);
  return reinterpret_cast<const uint8_t*>(&emission);
}

static const uint8_t* prepareBulkyPayload(size_t& size){
  size = sizeof(bulkyCommand);
  return reinterpret_cast<const uint8_t*>(&bulkyCommand);
}

static const uint8_t* prepareThegillPayload(size_t& size){
  size = sizeof(thegillCommand);
  return reinterpret_cast<const uint8_t*>(&thegillCommand);
}

static void handleGenericIncoming(ModuleState& state, const uint8_t* data, size_t length){
  (void)state;
  size_t copySize = length;
  if(copySize > sizeof(telemetry)){
    copySize = sizeof(telemetry);
  }
  memcpy(&telemetry, data, copySize);
  if(copySize < sizeof(telemetry)){
    uint8_t* dest = reinterpret_cast<uint8_t*>(&telemetry);
    memset(dest + copySize, 0, sizeof(telemetry) - copySize);
  }
}

static void handleBulkyIncoming(ModuleState& state, const uint8_t* data, size_t length){
  (void)state;
  size_t copySize = length;
  if(copySize > sizeof(reception)){
    copySize = sizeof(reception);
  }
  memcpy(&reception, data, copySize);
  if(copySize < sizeof(reception)){
    uint8_t* dest = reinterpret_cast<uint8_t*>(&reception);
    memset(dest + copySize, 0, sizeof(reception) - copySize);
  }
  processComsData(0);
}

static void handleGillIncoming(ModuleState& state, const uint8_t* data, size_t length){
  (void)state;
  (void)data;
  (void)length;
}

static void actionToggleArm(ModuleState& state, size_t slot);
static void actionTogglePrecision(ModuleState& state, size_t slot);
static void actionOpenPidSettings(ModuleState& state, size_t slot);
static void actionToggleBulkyHonk(ModuleState& state, size_t slot);
static void actionToggleBulkyLight(ModuleState& state, size_t slot);
static void actionToggleBulkySlow(ModuleState& state, size_t slot);
static void actionToggleGillMode(ModuleState& state, size_t slot);
static void actionCycleGillEasing(ModuleState& state, size_t slot);
static void actionOpenGillConfig(ModuleState& state, size_t slot);
static void actionToggleGillHonk(ModuleState& state, size_t slot);

static const FunctionActionOption kGenericOptions[] = {
  {"Toggle Arm", "ARM", actionToggleArm},
  {"PID Screen", "PID", actionOpenPidSettings},
  {"Precision Mode", "PREC", actionTogglePrecision}
};

static const FunctionActionOption kBulkyOptions[] = {
  {"Honk Toggle", "HNK", actionToggleBulkyHonk},
  {"Light Toggle", "LITE", actionToggleBulkyLight},
  {"Slow Mode", "SLOW", actionToggleBulkySlow}
};

static const FunctionActionOption kGillOptions[] = {
  {"Control Mode", "MODE", actionToggleGillMode},
  {"Config Screen", "CFG", actionOpenGillConfig},
  {"Honk Latch", "HNK", actionToggleGillHonk}
};

static const ModuleDescriptor kGenericDescriptor{
  PeerKind::Generic,
  "DroneGaze",
  drawGenericDashboard,
  drawGenericLayoutCard,
  handleGenericIncoming,
  updateDroneControl,
  prepareGenericPayload,
  kGenericOptions,
  sizeof(kGenericOptions) / sizeof(kGenericOptions[0])
};

static const ModuleDescriptor kBulkyDescriptor{
  PeerKind::Bulky,
  "Bulky",
  drawBulkyDashboard,
  drawBulkyLayoutCard,
  handleBulkyIncoming,
  updateBulkyControl,
  prepareBulkyPayload,
  kBulkyOptions,
  sizeof(kBulkyOptions) / sizeof(kBulkyOptions[0])
};

static const ModuleDescriptor kGillDescriptor{
  PeerKind::Thegill,
  "The'gill",
  drawThegillDashboard,
  drawThegillLayoutCard,
  handleGillIncoming,
  updateThegillControl,
  prepareThegillPayload,
  kGillOptions,
  sizeof(kGillOptions) / sizeof(kGillOptions[0])
};

static ModuleState moduleStates[] = {
  {&kGenericDescriptor, true, {0, 1, 2}, {false, false, false}},
  {&kBulkyDescriptor, true, {0, 1, 2}, {false, false, false}},
  {&kGillDescriptor, true, {0, 1, 2}, {false, false, false}}
};

static ModuleState* activeModule = &moduleStates[0];

static void initializeModuleAssignments(){
  size_t count = getModuleStateCount();
  for(size_t i = 0; i < count; ++i){
    ModuleState& state = moduleStates[i];
    size_t optionCount = state.descriptor->functionOptionCount;
    for(size_t slot = 0; slot < kMaxFunctionSlots; ++slot){
      if(optionCount == 0){
        state.assignedActions[slot] = 0;
      } else {
        state.assignedActions[slot] = slot % optionCount;
      }
      state.functionOutputs[slot] = false;
    }
    state.wifiEnabled = true;
  }
  activeModule = &moduleStates[0];
  lastPairedModule = activeModule;
  autoDashboardEnabled = true;
}

size_t getModuleStateCount(){
  return sizeof(moduleStates) / sizeof(moduleStates[0]);
}

ModuleState* getModuleState(size_t index){
  if(index >= getModuleStateCount()) return nullptr;
  return &moduleStates[index];
}

ModuleState* findModuleByKind(PeerKind kind){
  size_t count = getModuleStateCount();
  for(size_t i = 0; i < count; ++i){
    if(moduleStates[i].descriptor->kind == kind){
      return &moduleStates[i];
    }
  }
  return &moduleStates[0];
}

ModuleState* findModuleByName(const char* name){
  if(name == nullptr){
    return findModuleByKind(PeerKind::Generic);
  }
  if(isNameBulky(name)){
    return findModuleByKind(PeerKind::Bulky);
  }
  if(isNameThegill(name)){
    return findModuleByKind(PeerKind::Thegill);
  }
  return findModuleByKind(PeerKind::Generic);
}

ModuleState* getActiveModule(){
  return activeModule;
}

void setActiveModule(ModuleState* state){
  if(state){
    activeModule = state;
    for(size_t i = 0; i < kMaxFunctionSlots; ++i){
      functionButtonLatch[i] = false;
    }
    dashboardFocusIndex = 0;
    genericConfigActive = false;
    genericConfigIndex = 0;
  }
}

const FunctionActionOption* getAssignedAction(const ModuleState& state, size_t slot){
  if(slot >= kMaxFunctionSlots) return nullptr;
  size_t count = state.descriptor->functionOptionCount;
  if(count == 0) return nullptr;
  uint8_t index = state.assignedActions[slot] % count;
  return &state.descriptor->functionOptions[index];
}

void cycleAssignedAction(ModuleState& state, size_t slot, int delta){
  if(slot >= kMaxFunctionSlots) return;
  size_t count = state.descriptor->functionOptionCount;
  if(count == 0) return;
  int current = state.assignedActions[slot] % static_cast<int>(count);
  current = (current + delta) % static_cast<int>(count);
  if(current < 0) current += static_cast<int>(count);
  state.assignedActions[slot] = static_cast<uint8_t>(current);
  setFunctionOutput(state, slot, false);
}

void setFunctionOutput(ModuleState& state, size_t slot, bool active){
  if(slot >= kMaxFunctionSlots) return;
  state.functionOutputs[slot] = active;
}

bool getFunctionOutput(const ModuleState& state, size_t slot){
  if(slot >= kMaxFunctionSlots) return false;
  return state.functionOutputs[slot];
}

void toggleModuleWifi(ModuleState& state){
  state.wifiEnabled = !state.wifiEnabled;
  audioFeedback(state.wifiEnabled ? AudioCue::ToggleOn : AudioCue::ToggleOff);
  wifiControlCommand.magic = PACKET_MAGIC;
  wifiControlCommand.enableTelemetry = state.wifiEnabled ? 1 : 0;
  wifiCommandPending = true;
  memcpy(wifiCommandTarget, targetAddress, sizeof(wifiCommandTarget));
}

int buildGlobalMenuEntries(MenuEntry* entries, int maxEntries){
  if(!entries || maxEntries <= 0){
    return 0;
  }
  int count = 0;
  auto pushEntry = [&](const char* label, GlobalMenuAction action){
    if(count >= maxEntries){
      return;
    }
    entries[count].label = label;
    entries[count].action = action;
    ++count;
  };

  pushEntry("Dashboards", GlobalMenuAction::Dashboards);
  pushEntry("Pairing", GlobalMenuAction::Pairing);
  pushEntry("Log", GlobalMenuAction::Log);
  pushEntry("Configurations", GlobalMenuAction::Configurations);

  ModuleState* active = getActiveModule();
  PeerKind kind = active && active->descriptor ? active->descriptor->kind : PeerKind::Generic;
  switch(kind){
    case PeerKind::Generic:
      pushEntry("Telemetry", GlobalMenuAction::Telemetry);
      pushEntry("PID", GlobalMenuAction::PID);
      break;
    case PeerKind::Bulky:
      pushEntry("Packet Vars", GlobalMenuAction::PacketVariables);
      pushEntry("Modes", GlobalMenuAction::Modes);
      break;
    case PeerKind::Thegill:
      pushEntry("Packet Vars", GlobalMenuAction::PacketVariables);
      pushEntry("Modes", GlobalMenuAction::Modes);
      break;
    default:
      break;
  }

  pushEntry("Back", GlobalMenuAction::Back);
  return count;
}

static void actionToggleArm(ModuleState& state, size_t slot){
  bool next = !getFunctionOutput(state, slot);
  setFunctionOutput(state, slot, next);
  btnmode = next;
  audioFeedback(next ? AudioCue::ToggleOn : AudioCue::ToggleOff);
}

static void actionTogglePrecision(ModuleState& state, size_t slot){
  dronePrecisionMode = !dronePrecisionMode;
  setFunctionOutput(state, slot, dronePrecisionMode);
  audioFeedback(dronePrecisionMode ? AudioCue::ToggleOn : AudioCue::ToggleOff);
}

static void actionOpenPidSettings(ModuleState& state, size_t slot){
  (void)slot;
  displayMode = DISPLAY_MODE_PID;
  pidGraphIndex = 0;
  setFunctionOutput(state, slot, false);
  genericConfigActive = false;
  genericConfigIndex = 0;
  audioFeedback(AudioCue::Select);
}

static void actionToggleBulkyHonk(ModuleState& state, size_t slot){
  bulkyHonkLatch = !bulkyHonkLatch;
  setFunctionOutput(state, slot, bulkyHonkLatch);
  audioFeedback(bulkyHonkLatch ? AudioCue::ToggleOn : AudioCue::ToggleOff);
}

static void actionToggleBulkyLight(ModuleState& state, size_t slot){
  bulkyLightLatch = !bulkyLightLatch;
  setFunctionOutput(state, slot, bulkyLightLatch);
  audioFeedback(bulkyLightLatch ? AudioCue::ToggleOn : AudioCue::ToggleOff);
}

static void actionToggleBulkySlow(ModuleState& state, size_t slot){
  bulkySlowMode = !bulkySlowMode;
  setFunctionOutput(state, slot, bulkySlowMode);
  audioFeedback(bulkySlowMode ? AudioCue::ToggleOn : AudioCue::ToggleOff);
}

static void actionToggleGillMode(ModuleState& state, size_t slot){
  if(thegillConfig.mode == GillMode::Default){
    thegillConfig.mode = GillMode::Differential;
  } else {
    thegillConfig.mode = GillMode::Default;
  }
  resetThegillState();
  setFunctionOutput(state, slot, thegillConfig.mode == GillMode::Differential);
  audioFeedback(thegillConfig.mode == GillMode::Differential ? AudioCue::ToggleOn : AudioCue::ToggleOff);
}

static void actionCycleGillEasing(ModuleState& state, size_t slot){
  (void)state;
  (void)slot;
  int next = (static_cast<int>(thegillConfig.easing) + 1) % 5;
  thegillConfig.easing = static_cast<GillEasing>(next);
  thegillCommand.easing = thegillConfig.easing;
  audioFeedback(AudioCue::Scroll);
}

static void actionOpenGillConfig(ModuleState& state, size_t slot){
  (void)slot;
  setActiveModule(&state);
  displayMode = DISPLAY_MODE_TELEMETRY;
  gillConfigIndex = 0;
  genericConfigActive = false;
  genericConfigIndex = 0;
  setFunctionOutput(state, slot, false);
  audioFeedback(AudioCue::Select);
}

static void actionToggleGillHonk(ModuleState& state, size_t slot){
  gillHonkLatch = !gillHonkLatch;
  setFunctionOutput(state, slot, gillHonkLatch);
  audioFeedback(gillHonkLatch ? AudioCue::ToggleOn : AudioCue::ToggleOff);
}

static void processFunctionKeys(){
  const uint8_t pins[kMaxFunctionSlots] = {button1, button2, button3};
  if(displayMode == DISPLAY_MODE_LOG){
    for(size_t slot = 0; slot < kMaxFunctionSlots; ++slot){
      bool pressed = digitalRead(pins[slot]) == LOW;
      if(pressed && !functionButtonLatch[slot]){
        functionButtonLatch[slot] = true;
        switch(slot){
          case 0: {
            byte target = logReturnMode;
            if(target == DISPLAY_MODE_DASHBOARD){
              target = DISPLAY_MODE_HOME;
            }
            if(target == DISPLAY_MODE_LOG){
              target = DISPLAY_MODE_HOME;
            }
            displayMode = target;
            if(displayMode == DISPLAY_MODE_PAIRING){
              selectedPeer = 0;
            }
            audioFeedback(AudioCue::Back);
            break;
          }
          case 1:
            displayMode = DISPLAY_MODE_PAIRING;
            selectedPeer = 0;
            lastDiscoveryTime = millis();
            discovery.discover();
            audioFeedback(AudioCue::Select);
            break;
          case 2:
          default:
            connectionLogClear();
            logScrollOffset = 0;
            audioFeedback(AudioCue::Select);
            break;
        }
      } else if(!pressed){
        functionButtonLatch[slot] = false;
      }
    }
    return;
  }

  ModuleState* active = getActiveModule();
  if(!active) return;
  for(size_t slot = 0; slot < kMaxFunctionSlots; ++slot){
    bool pressed = digitalRead(pins[slot]) == LOW;
    if(pressed && !functionButtonLatch[slot]){
      functionButtonLatch[slot] = true;
      const FunctionActionOption* action = getAssignedAction(*active, slot);
      if(action && action->invoke){
        action->invoke(*active, slot);
      }
    } else if(!pressed){
      functionButtonLatch[slot] = false;
    }
  }
}

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
      case DISPLAY_MODE_HOME: drawHomeMenu(); break;
      case DISPLAY_MODE_TELEMETRY: drawTelemetryInfo(); break;
      case DISPLAY_MODE_PID: drawPidGraph(); break;
      case DISPLAY_MODE_ORIENTATION: drawOrientationCube(); break;
      case DISPLAY_MODE_PAIRING: drawPairingMenu(); break;
      case DISPLAY_MODE_DASHBOARD: drawDashboard(); break;
      case DISPLAY_MODE_ABOUT: drawAbout(); break;
      case DISPLAY_MODE_PEER_INFO: drawPeerInfo(); break;
      case DISPLAY_MODE_GLOBAL_MENU: drawGlobalMenu(); break;
      case DISPLAY_MODE_LOG: drawConnectionLog(); break;
      case DISPLAY_MODE_PACKET: drawPacketVariables(); break;
      case DISPLAY_MODE_MODES: drawModeSummary(); break;
    }
    appendPidSample();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void commTask(void* pvParameters){
  const TickType_t delay = 10 / portTICK_PERIOD_MS; // 50Hz
  for(;;){
    const uint8_t* payload = nullptr;
    size_t payloadSize = 0;
    ModuleState* active = getActiveModule();
        active->descriptor->updateControl();
        payload = active->descriptor->preparePayload(payloadSize);
      
    if( payload && payloadSize > 0){
      if(esp_now_send(targetAddress, payload, payloadSize)==ESP_OK){
        sent_Status = true;
        // connectionLogAddf("TX payload (%u bytes)", static_cast<unsigned>(payloadSize));
      }else{
        sent_Status = false;
        connectionLogAddf("payload  failed (%u bytes)", static_cast<unsigned>(payloadSize));
      }
    }
  }
        vTaskDelay(delay);
    }



void setup() {

  //Pin directions ===================
  initInput();
  //==================================

  initializeModuleAssignments();

  connectionLogInit();
  connectionLogSetRecordingEnabled(true);
  connectionLogAdd("System boot");

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
  esp_now_register_recv_cb(OnDataRecv); debug("reception callback set \n \n")//Recv Callback Function associated

  // memcpy(bot.peer_addr, targetAddress, 6); //putting the bot ID card into memory ;)
  // bot.channel = 0;     
  // bot.encrypt = false; //data won't be encrypted so we don't waste too much cpu juice

  // if (esp_now_add_peer(&bot) != ESP_OK){
  // debug("What the bot doin ?");
  // while(1){oled.drawStr(0,15,"someting wong wit ya bot ID");  oled.sendBuffer();delay(5000);}
  // }debug("peer added \n")

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
  audioSetup();
  audioPlayStartup();
  oled.clearBuffer();

  //side note about playing the tones as we've designed them, use pointers for efficiency. 
  //==================================
  //Data Config
  //==================================

}

unsigned long lastBtnModeMillis;
bool ispressed;

void loop() {
  uint32_t now = millis();
  static uint32_t lastPairMaintenance = 0;
  static byte lastDisplayMode = 0xFF;
  if(displayMode != lastDisplayMode){
    connectionLogSetRecordingEnabled(displayMode == DISPLAY_MODE_LOG);
    if(displayMode != DISPLAY_MODE_TELEMETRY){
      genericConfigActive = false;
      genericConfigIndex = 0;
    }
    lastDisplayMode = displayMode;
  }
  if(now - lastPairMaintenance >= 200){
  discovery.discover();
  lastPairMaintenance = now;
  }
  ArduinoOTA.handle();
  checkPress();
  audioUpdate();
  processFunctionKeys();

  ModuleState* active = getActiveModule();
  PeerKind kind = active ? active->descriptor->kind : PeerKind::Generic;

  if(connected && now - lastReceiveTime > 3000){
    byte previousMode = displayMode;
    connected = false;
    controlSessionActive = false;
    memcpy(targetAddress, broadcastAddress, sizeof(targetAddress));
    discovery.resetLinkState();
    discovery.discover();
    lastDiscoveryTime = now;
    logReturnMode = previousMode == DISPLAY_MODE_DASHBOARD ? DISPLAY_MODE_HOME : previousMode;
    displayMode = DISPLAY_MODE_LOG;
    logScrollOffset = 0;
    selectedPeer = 0;
    botMotionState = STOP;
    botSpeed = 0;
    genericConfigActive = false;
    genericConfigIndex = 0;
    if(kind == PeerKind::Thegill){
      resetThegillState();
    }
    connectionLogAdd("Link timeout");
  }

  if(kind != PeerKind::Thegill && now - lastBtnModeMillis >= 200){
    if(!ispressed){
      if(digitalRead(joystickBtnA) == LOW){
        btnmode = !btnmode;
        audioFeedback(btnmode ? AudioCue::ToggleOn : AudioCue::ToggleOff);
        ispressed = true;
      }
    } else if(digitalRead(joystickBtnA) == HIGH){
      ispressed = false;
    }
    lastBtnModeMillis = now;
  }

  if(displayMode == DISPLAY_MODE_PAIRING && now - lastDiscoveryTime > 2000){
    discovery.discover();
    lastDiscoveryTime = now;
  }

  int delta = encoderCount - lastEncoderCount;
  if(displayMode == DISPLAY_MODE_HOME){
    size_t moduleCount = getModuleStateCount();
    int totalEntries = static_cast<int>(moduleCount) + 1;
    if(totalEntries <= 0) totalEntries = 1;
    if(delta != 0){
      homeMenuIndex = (homeMenuIndex + delta) % totalEntries;
      if(homeMenuIndex < 0) homeMenuIndex += totalEntries;
      lastEncoderCount = encoderCount;
      audioFeedback(AudioCue::Scroll);
    }
  } else if(displayMode == DISPLAY_MODE_TELEMETRY && kind == PeerKind::Thegill){
    if(delta != 0){
      gillConfigIndex = (gillConfigIndex + delta) % 3;
      if(gillConfigIndex < 0) gillConfigIndex += 3;
      lastEncoderCount = encoderCount;
      audioFeedback(AudioCue::Scroll);
    }
  } else if(displayMode == DISPLAY_MODE_TELEMETRY && kind == PeerKind::Generic && genericConfigActive){
    if(delta != 0){
      const int optionCount = 3;
      genericConfigIndex = (genericConfigIndex + delta) % optionCount;
      if(genericConfigIndex < 0) genericConfigIndex += optionCount;
      lastEncoderCount = encoderCount;
      audioFeedback(AudioCue::Scroll);
    }
  } else if(displayMode == DISPLAY_MODE_PID){
    if(delta != 0){
      pidGraphIndex = (pidGraphIndex + delta) % 3;
      if(pidGraphIndex < 0) pidGraphIndex += 3;
      lastEncoderCount = encoderCount;
      audioFeedback(AudioCue::Scroll);
    }
  } else if(displayMode == DISPLAY_MODE_PAIRING){
    int count = discovery.getPeerCount() + 1;
    if(delta != 0 && count > 0){
      selectedPeer = (selectedPeer + delta) % count;
      if(selectedPeer < 0) selectedPeer += count;
      lastEncoderCount = encoderCount;
      audioFeedback(AudioCue::Scroll);
    }
  } else if(displayMode == DISPLAY_MODE_DASHBOARD){
    if(delta != 0){
      int focusCount = 2;
      if(active){
        focusCount += static_cast<int>(kMaxFunctionSlots);
      }
      if(focusCount <= 0) focusCount = 1;
      dashboardFocusIndex = (dashboardFocusIndex + delta) % focusCount;
      if(dashboardFocusIndex < 0) dashboardFocusIndex += focusCount;
      lastEncoderCount = encoderCount;
      audioFeedback(AudioCue::Scroll);
    }
  } else if(displayMode == DISPLAY_MODE_LOG){
    size_t count = connectionLogGetCount();
    if(delta != 0 && count > 0){
      int maxOffset = getLogMaxScrollOffset();
      logScrollOffset = constrain(logScrollOffset - delta, 0, maxOffset);
      lastEncoderCount = encoderCount;
      audioFeedback(AudioCue::Scroll);
    }
  } else if(displayMode == DISPLAY_MODE_GLOBAL_MENU){
    MenuEntry entries[10];
    int entryCount = buildGlobalMenuEntries(entries, 10);
    if(entryCount <= 0){
      entryCount = 1;
    }
    if(delta != 0){
      globalMenuIndex = (globalMenuIndex + delta) % entryCount;
      if(globalMenuIndex < 0) globalMenuIndex += entryCount;
      lastEncoderCount = encoderCount;
      audioFeedback(AudioCue::Scroll);
    }
  } else if(displayMode == DISPLAY_MODE_ORIENTATION || displayMode == DISPLAY_MODE_ABOUT || displayMode == DISPLAY_MODE_PEER_INFO){
    if(delta != 0){
      homeSelected = !homeSelected;
      lastEncoderCount = encoderCount;
      audioFeedback(AudioCue::Scroll);
    }
  } else if(delta != 0){
    lastEncoderCount = encoderCount;
    audioFeedback(AudioCue::Scroll);
  }

    if(encoderBtnState){
      encoderBtnState = 0;
      if(displayMode == DISPLAY_MODE_DASHBOARD){
      if(dashboardFocusIndex == 0){
        displayMode = DISPLAY_MODE_GLOBAL_MENU;
        globalMenuIndex = 0;
        audioFeedback(AudioCue::Select);
      } else if(dashboardFocusIndex == 1){
        if(active){
          toggleModuleWifi(*active);
        }
      } else if(active){
        size_t slot = static_cast<size_t>(dashboardFocusIndex - 2);
        if(slot < kMaxFunctionSlots){
          const FunctionActionOption* action = getAssignedAction(*active, slot);
          if(action && action->invoke){
            action->invoke(*active, slot);
          }
        }
      }
    } else if(displayMode == DISPLAY_MODE_HOME){
      if(homeMenuIndex == 0){
        autoDashboardEnabled = true;
        if(lastPairedModule){
          setActiveModule(lastPairedModule);
        } else {
          ModuleState* fallback = getActiveModule();
          if(!fallback){
            fallback = getModuleState(0);
          }
          if(fallback){
            setActiveModule(fallback);
          }
        }
        displayMode = DISPLAY_MODE_DASHBOARD;
        dashboardFocusIndex = 0;
        audioFeedback(AudioCue::Select);
      } else {
        ModuleState* selected = getModuleState(static_cast<size_t>(homeMenuIndex - 1));
        if(selected){
          autoDashboardEnabled = false;
          setActiveModule(selected);
          displayMode = DISPLAY_MODE_DASHBOARD;
          dashboardFocusIndex = 0;
          audioFeedback(AudioCue::Select);
        }
      }
    } else if(displayMode == DISPLAY_MODE_PAIRING){
      int count = discovery.getPeerCount();
      if(selectedPeer == count){
        displayMode = DISPLAY_MODE_HOME;
        audioFeedback(AudioCue::Back);
      } else if(count > 0){
        infoPeer = selectedPeer;
        displayMode = DISPLAY_MODE_PEER_INFO;
        audioFeedback(AudioCue::Select);
      }
    } else if(displayMode == DISPLAY_MODE_TELEMETRY && kind == PeerKind::Thegill){
      if(active){
        if(gillConfigIndex == 0){
          actionToggleGillMode(*active, 0);
        } else if(gillConfigIndex == 1){
          actionCycleGillEasing(*active, 1);
        } else {
          displayMode = DISPLAY_MODE_HOME;
          homeMenuIndex = 1;
          audioFeedback(AudioCue::Back);
        }
      }
    } else if(displayMode == DISPLAY_MODE_TELEMETRY && kind == PeerKind::Generic && genericConfigActive){
      if(active){
        if(genericConfigIndex == 0){
          actionTogglePrecision(*active, 2);
        } else if(genericConfigIndex == 1){
          yawCommand = 0;
          audioFeedback(AudioCue::Select);
        } else {
          genericConfigActive = false;
          genericConfigIndex = 0;
          displayMode = DISPLAY_MODE_DASHBOARD;
          dashboardFocusIndex = 0;
          audioFeedback(AudioCue::Back);
        }
      }
    } else if(displayMode == DISPLAY_MODE_PEER_INFO){
      if(homeSelected){
        displayMode = DISPLAY_MODE_PAIRING;
        homeSelected = false;
        audioFeedback(AudioCue::Back);
      } else {
        const uint8_t *mac = discovery.getPeer(infoPeer);
        if(mac){
          memcpy(targetAddress, mac, 6);
        }
        controlSessionActive = true;
        connected = false;
        const char* peerName = discovery.getPeerName(infoPeer);
        ModuleState* module = findModuleByName(peerName);
        setActiveModule(module);
        if(module && module->descriptor->kind == PeerKind::Bulky){
          botMotionState = STOP;
          botSpeed = 0;
        } else if(module && module->descriptor->kind == PeerKind::Thegill){
          resetThegillState();
          gillConfigIndex = 0;
        }
        displayMode = DISPLAY_MODE_DASHBOARD;
        dashboardFocusIndex = 0;
        audioFeedback(AudioCue::Select);
      }
    } else if(displayMode == DISPLAY_MODE_PID){
      displayMode = DISPLAY_MODE_HOME;
      homeMenuIndex = 2;
      audioFeedback(AudioCue::Back);
    } else if(displayMode == DISPLAY_MODE_ORIENTATION || displayMode == DISPLAY_MODE_ABOUT){
      if(homeSelected){
        displayMode = DISPLAY_MODE_HOME;
        homeMenuIndex = 0;
        homeSelected = false;
        audioFeedback(AudioCue::Back);
      }
    } else if(displayMode == DISPLAY_MODE_GLOBAL_MENU){
      MenuEntry entries[10];
      int entryCount = buildGlobalMenuEntries(entries, 10);
      if(entryCount <= 0){
        displayMode = DISPLAY_MODE_DASHBOARD;
        audioFeedback(AudioCue::Back);
      } else if(globalMenuIndex < entryCount){
        GlobalMenuAction action = entries[globalMenuIndex].action;
        switch(action){
          case GlobalMenuAction::Dashboards:
            displayMode = DISPLAY_MODE_HOME;
            homeMenuIndex = 0;
            audioFeedback(AudioCue::Select);
            break;
          case GlobalMenuAction::Pairing:
            displayMode = DISPLAY_MODE_PAIRING;
            selectedPeer = 0;
            audioFeedback(AudioCue::Select);
            break;
          case GlobalMenuAction::Log:
            logReturnMode = DISPLAY_MODE_GLOBAL_MENU;
            displayMode = DISPLAY_MODE_LOG;
            logScrollOffset = 0;
            audioFeedback(AudioCue::Select);
            break;
          case GlobalMenuAction::Configurations:
            if(kind == PeerKind::Generic){
              genericConfigActive = true;
              genericConfigIndex = 0;
              displayMode = DISPLAY_MODE_TELEMETRY;
            } else if(kind == PeerKind::Thegill){
              genericConfigActive = false;
              displayMode = DISPLAY_MODE_TELEMETRY;
              gillConfigIndex = 0;
            } else {
              genericConfigActive = false;
              displayMode = DISPLAY_MODE_TELEMETRY;
            }
            audioFeedback(AudioCue::Select);
            break;
          case GlobalMenuAction::Telemetry:
            if(kind == PeerKind::Generic){
              displayMode = DISPLAY_MODE_ORIENTATION;
            } else {
              displayMode = DISPLAY_MODE_TELEMETRY;
              genericConfigActive = false;
              genericConfigIndex = 0;
            }
            audioFeedback(AudioCue::Select);
            break;
          case GlobalMenuAction::PID:
            displayMode = DISPLAY_MODE_PID;
            pidGraphIndex = 0;
            audioFeedback(AudioCue::Select);
            break;
          case GlobalMenuAction::PacketVariables:
            displayMode = DISPLAY_MODE_PACKET;
            audioFeedback(AudioCue::Select);
            break;
          case GlobalMenuAction::Modes:
            displayMode = DISPLAY_MODE_MODES;
            audioFeedback(AudioCue::Select);
            break;
          case GlobalMenuAction::Back:
          default:
            displayMode = DISPLAY_MODE_DASHBOARD;
            dashboardFocusIndex = 0;
            audioFeedback(AudioCue::Back);
            break;
        }
      }
    } else if(displayMode == DISPLAY_MODE_PACKET || displayMode == DISPLAY_MODE_MODES){
      displayMode = DISPLAY_MODE_DASHBOARD;
      dashboardFocusIndex = 0;
      audioFeedback(AudioCue::Back);
    }
    lastEncoderCount = encoderCount;
  }

  // Display rendering handled in FreeRTOS display task
}
