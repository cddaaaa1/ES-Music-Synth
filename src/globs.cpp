#include "globals.h"
#include "pins.h"
// Define the step-size tables here so they are visible to all files
const uint32_t stepSizes4[12] = {
    51075946, 54113659, 57330980, 60739622,
    64351299, 68178701, 72232564, 76527532,
    81078245, 85899345, 91007428, 96419087};

const uint32_t stepSizes5[12] = {
    102151892, 108227319, 114661960, 121479245,
    128702599, 136357402, 144465129, 153055064,
    162156490, 171798691, 182014857, 192838174};

// Actual global variables
volatile uint32_t currentStepSize = 0;

std::bitset<12> keys4;
std::bitset<12> keys5;

std::bitset<2> prevKnobState = 0;
Knob knob3(0, 8);

QueueHandle_t msgInQ;
QueueHandle_t msgOutQ;
SemaphoreHandle_t CAN_TX_Semaphore;

uint8_t RX_Message[8] = {0};

SystemState sysState;

HardwareTimer sampleTimer(TIM1);
U8G2_SSD1305_128X32_ADAFRUIT_F_HW_I2C u8g2(U8G2_R0);

TaskHandle_t scanKeysHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;

// --------- Shared Helper: setOutMuxBit -----------
void setOutMuxBit(const uint8_t bitIdx, const bool value)
{
    digitalWrite(A5, LOW);            // REN pin
    digitalWrite(D3, bitIdx & 0x01);  // RA0
    digitalWrite(D6, bitIdx & 0x02);  // RA1
    digitalWrite(D12, bitIdx & 0x04); // RA2
    digitalWrite(D11, value);         // OUT pin
    digitalWrite(A5, HIGH);
    delayMicroseconds(2);
    digitalWrite(A5, LOW);
}

// --------- The 22kHz Audio ISR --------------------
void sampleISR()
{
    static uint32_t phaseAcc = 0;
    phaseAcc += currentStepSize;
    int32_t Vout = (phaseAcc >> 24) - 128; // range: -128..127

    int volumeLevel = sysState.volume;
    Vout = Vout >> (8 - volumeLevel);

    // Output on e.g. OUTR_PIN = A3
    analogWrite(A3, Vout + 128); // 0..255 for DAC
}
