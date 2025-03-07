#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <bitset>
#include <STM32FreeRTOS.h>
#include <U8g2lib.h>
#include <ES_CAN.h>
#include "Knob.h"

// ---------------------- CONFIG ----------------------
#define OCTAVE 5 // or 4, depending on the board

// Audio sampling frequency
static const uint32_t fs = 22000;

// Step size arrays for C4 & C5
extern const uint32_t stepSizes4[12];
extern const uint32_t stepSizes5[12];

// ------------------ Global Variables ---------------
extern volatile uint32_t currentStepSize;

// Pressed-key tracking
extern std::bitset<12> keys4;
extern std::bitset<12> keys5;

extern std::bitset<2> prevKnobState;
extern Knob knob3;

extern QueueHandle_t msgInQ;
extern QueueHandle_t msgOutQ;
extern SemaphoreHandle_t CAN_TX_Semaphore;

extern uint8_t RX_Message[8];

// A struct to hold system-wide state
struct SystemState
{
    std::bitset<32> inputs;
    SemaphoreHandle_t mutex;
    int rotationVariable;
    int volume;
};

extern SystemState sysState;

// The hardware timer for 22 kHz
extern HardwareTimer sampleTimer;
extern U8G2_SSD1305_128X32_ADAFRUIT_F_HW_I2C u8g2;

// inside globals.h
extern TaskHandle_t scanKeysHandle;
extern TaskHandle_t displayTaskHandle;

// --------------- Function Prototypes ---------------
void setOutMuxBit(const uint8_t bitIdx, const bool value);
void sampleISR();

#endif
