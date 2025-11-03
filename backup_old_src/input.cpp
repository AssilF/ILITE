#include "input.h"

volatile bool button1State = false;
volatile bool button2State = false;
volatile bool button3State = false;
volatile bool joystickBtnAState = false;
volatile bool encoderBtnState = false;
volatile int encoderCount = 0;
volatile unsigned long button1Millis = 0;
volatile unsigned long button2Millis = 0;
volatile unsigned long button3Millis = 0;
volatile unsigned long joystickBtnAMillis = 0;
volatile unsigned long encoderBtnMillis = 0;

void IRAM_ATTR button1ISR(){
  if(millis() - button1Millis >= deadtime){
    button1State = true;
    button1Millis = millis();
  }
}

void IRAM_ATTR button2ISR(){
  if(millis() - button2Millis >= deadtime){
    button2State = true;
    button2Millis = millis();
  }
}

void IRAM_ATTR button3ISR(){
  if(millis() - button3Millis >= deadtime){
    button3State = true;
    button3Millis = millis();
  }
}

void IRAM_ATTR joystickBtnAISR(){
  if(millis() - joystickBtnAMillis >= deadtime){
    joystickBtnAState = true;
    joystickBtnAMillis = millis();
  }
}

void IRAM_ATTR encoderBtnISR(){
  if(millis() - encoderBtnMillis >= deadtime){
    encoderBtnState = true;
    encoderBtnMillis = millis();
  }
}

void IRAM_ATTR encoderClockISR(){
  if(digitalRead(encoderB) == 1){
    encoderCount++;
  } else {
    encoderCount--;
  }
}

void initInput(){
  pinMode(encoderA, INPUT_PULLUP);
  pinMode(encoderB, INPUT_PULLUP);
  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  pinMode(button3, INPUT_PULLUP);
  pinMode(joystickBtnA, INPUT_PULLUP);
  pinMode(encoderBtn, INPUT_PULLUP);
  pinMode(joystickA_X, INPUT);
  pinMode(joystickA_Y, INPUT);
  pinMode(joystickB_X, INPUT);
  pinMode(joystickB_Y, INPUT);
  pinMode(potA, INPUT);

  // attachInterrupt(joystickBtnA, joystickBtnAISR, FALLING);
  attachInterrupt(encoderBtn, encoderBtnISR, RISING);
  // attachInterrupt(button1, button1ISR, FALLING);
  // attachInterrupt(button2, button2ISR, FALLING);
  // attachInterrupt(button3, button3ISR, FALLING);
  attachInterrupt(encoderA, encoderClockISR, RISING);
}

void checkPress(){
  // placeholder for button state housekeeping
}
