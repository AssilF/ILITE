#pragma once
#ifndef INPUT_H
#define INPUT_H
#include <Arduino.h>

constexpr uint8_t encoderA = 16; // clock
constexpr uint8_t encoderB = 17; // data
constexpr uint8_t encoderBtn = 18;
constexpr uint8_t joystickBtnA = 19;
constexpr uint8_t joystickBtnB = 13;
constexpr uint8_t joystickA_X = 35;
constexpr uint8_t joystickA_Y = 34;
constexpr uint8_t joystickB_X = 39;
constexpr uint8_t joystickB_Y = 36;
constexpr uint8_t button1 = 23;
constexpr uint8_t button2 = 25;
constexpr uint8_t button3 = 27;

// Human-friendly aliases for face buttons
constexpr uint8_t buttonLeftAction  = button1;  ///< Primary action button
constexpr uint8_t buttonShift       = button2;  ///< Shift/auxiliary modifier
constexpr uint8_t buttonRightAction = button3;  ///< Secondary action button
constexpr uint8_t potA = 32;
constexpr uint8_t batteryPin = 33;  // ADC1_CH5 for battery voltage (divided by 2)

constexpr unsigned long deadtime = 50; // ms

extern volatile bool button1State;
extern volatile bool button2State;
extern volatile bool button3State;
extern volatile bool joystickBtnAState;
extern volatile bool encoderBtnState;
extern volatile int encoderCount;
extern volatile unsigned long button1Millis;
extern volatile unsigned long button2Millis;
extern volatile unsigned long button3Millis;
extern volatile unsigned long joystickBtnAMillis;
extern volatile unsigned long encoderBtnMillis;

void initInput();
void checkPress();
#endif
