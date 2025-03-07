#include <Arduino.h>
#include <U8g2lib.h>
#include <bitset>
#include <STM32FreeRTOS.h> // FreeRTOS for threading support
#include "Knob.h"
#include <ES_CAN.h>
#define OCTAVE 5

// Constants
const uint32_t interval = 100;                         // Display update interval
const uint32_t fs = 22000;                             // Sampling frequency (Hz)
const uint64_t phase_accumulator_modulus = 4294967296; // 2^32 using bit shift

// Pin Definitions
const int RA0_PIN = D3, RA1_PIN = D6, RA2_PIN = D12, REN_PIN = A5, C0_PIN = A2, C1_PIN = D9, C2_PIN = A6, C3_PIN = D1, OUT_PIN = D11, OUTL_PIN = A4, OUTR_PIN = A3, JOYY_PIN = A0, JOYX_PIN = A1;

// Output multiplexer bits
const int DEN_BIT = 3;
const int DRST_BIT = 4;
const int HKOW_BIT = 5;
const int HKOE_BIT = 6;

// Step Sizes for Notes C5 to B5
const uint32_t stepSizes5[12] = {
    102151892, // C5
    108227319, // C#5
    114661960, // D5
    121479245, // D#5
    128702599, // E5
    136357402, // F5
    144465129, // F#5
    153055064, // G5
    162156490, // G#5
    171798691, // A5
    182014857, // A#5
    192838174  // B5
};

const uint32_t stepSizes4[12] = {
    51075946, // C4
    54113659, // C#4
    57330980, // D4
    60739622, // D#4
    64351299, // E4
    68178701, // F4
    72232564, // F#4
    76527532, // G4
    81078245, // G#4
    85899345, // A4
    91007428, // A#4
    96419087  // B4
};

// **Global Variable
volatile uint32_t currentStepSize = 0;
std::bitset<2> prevKnobState = 0;
Knob knob3(0, 8);
QueueHandle_t msgInQ;
QueueHandle_t msgOutQ;
SemaphoreHandle_t CAN_TX_Semaphore;
uint8_t RX_Message[8] = {0};
static std::bitset<12> keys4; // local pressed keys
static std::bitset<12> keys5;

struct // Struct to Store System State
{
  std::bitset<32> inputs;
  SemaphoreHandle_t mutex;
  int rotationVariable;
  int volume;
} sysState;

HardwareTimer sampleTimer(TIM1);
U8G2_SSD1305_128X32_ADAFRUIT_F_HW_I2C u8g2(U8G2_R0);

// **Thread Handle**
TaskHandle_t scanKeysHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;

// Function to set outputs using key matrix
void setOutMuxBit(const uint8_t bitIdx, const bool value)
{
  digitalWrite(REN_PIN, LOW);
  digitalWrite(RA0_PIN, bitIdx & 0x01);
  digitalWrite(RA1_PIN, bitIdx & 0x02);
  digitalWrite(RA2_PIN, bitIdx & 0x04);
  digitalWrite(OUT_PIN, value);
  digitalWrite(REN_PIN, HIGH);
  delayMicroseconds(2);
  digitalWrite(REN_PIN, LOW);
}

void CAN_TX_ISR(void)
{
  xSemaphoreGiveFromISR(CAN_TX_Semaphore, NULL);
}

void CAN_RX_ISR(void)
{
  uint8_t RX_Message_ISR[8];
  uint32_t ID = 0X123;
  CAN_RX(ID, RX_Message_ISR);
  xQueueSendFromISR(msgInQ, RX_Message_ISR, NULL);
}

// **Interrupt Service Routine for Audio Generation**
void sampleISR()
{
  static uint32_t phaseAcc = 0;
  phaseAcc += currentStepSize;
  int32_t Vout = (phaseAcc >> 24) - 128; // Scale range: -128 to 127

  int volumeLevel = 8;

  volumeLevel = sysState.volume; // Read volume safely

  Vout = Vout >> (8 - volumeLevel);

  analogWrite(OUTR_PIN, Vout + 128); // Convert to unsigned (0-255) for DAC
}

void decodeTask(void *pvParameters)
{
  uint8_t local_RX_Message[8] = {0};
  while (1)
  {
    xQueueReceive(msgInQ, local_RX_Message, portMAX_DELAY);
    uint32_t localCurrentStepSize;

    if (xSemaphoreTake(sysState.mutex, portMAX_DELAY) == pdTRUE)
    {
      memcpy(RX_Message, local_RX_Message, sizeof(RX_Message));
      xSemaphoreGive(sysState.mutex);
    }
    char msgType = local_RX_Message[0];
    uint8_t msgOctave = local_RX_Message[1];
    uint8_t noteIndex = local_RX_Message[2];

    if (msgType == 'P')
    {
      if (OCTAVE == 4)
      {
        if (msgOctave == 5)
        {
          localCurrentStepSize = stepSizes5[noteIndex];
          keys5.set(noteIndex, true);
        }
      }
    }
    else if (msgType == 'R')
    {
      if (OCTAVE == 4)
      {
        if (msgOctave == 5)
        {
          localCurrentStepSize = 0;
          keys5.set(noteIndex, false);
        }
      }
    }

    if (OCTAVE == 4)
    {
      __atomic_store_n(&currentStepSize, localCurrentStepSize, __ATOMIC_RELAXED);
    }
  }
}

void CAN_TX_Task(void *pvParameters)
{
  uint8_t msgOut[8];
  while (1)
  {
    xQueueReceive(msgOutQ, msgOut, portMAX_DELAY);
    xSemaphoreTake(CAN_TX_Semaphore, portMAX_DELAY);
    CAN_TX(0x123, msgOut);
  }
}

// **Key Scanning Task (Runs in a Separate Thread)**
void scanKeysTask(void *pvParameters)
{
  const TickType_t xFrequency = 50 / portTICK_PERIOD_MS;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (1)
  {
    std::bitset<32> localInputs;
    int lastPressedKey = -1;
    std::bitset<2> currentKnobState;
    int localRotationVariable = 0;
    int localVolume = sysState.volume;
    std::bitset<32> previousInput = sysState.inputs;
    uint8_t TX_Message[8] = {0};

    for (uint8_t row = 0; row < 4; row++)
    {
      // Select row
      digitalWrite(REN_PIN, LOW);
      digitalWrite(RA0_PIN, row & 0x01);
      digitalWrite(RA1_PIN, row & 0x02);
      digitalWrite(RA2_PIN, row & 0x04);
      digitalWrite(REN_PIN, HIGH);
      delayMicroseconds(3);

      // Read columns
      std::bitset<4> colInputs;
      colInputs[0] = digitalRead(C0_PIN);
      colInputs[1] = digitalRead(C1_PIN);
      colInputs[2] = digitalRead(C2_PIN);
      colInputs[3] = digitalRead(C3_PIN);
      digitalWrite(REN_PIN, LOW);

      for (uint8_t col = 0; col < 4; col++)
      {
        int keyIndex = row * 4 + col;
        localInputs[keyIndex] = colInputs[col];

        // scan key
        if (keyIndex <= 11 && keyIndex >= 0)
        {
          if (!colInputs[col])
          {
            lastPressedKey = keyIndex;
            if (OCTAVE == 4)
            {
              keys4.set(keyIndex, true);
              __atomic_store_n(&currentStepSize, stepSizes4[lastPressedKey], __ATOMIC_RELAXED);
            }
            else
            {
              TX_Message[0] = 'P';
              TX_Message[1] = OCTAVE;
              TX_Message[2] = lastPressedKey;
              xQueueSend(msgOutQ, TX_Message, portMAX_DELAY);
            }
          }
          if (!previousInput[keyIndex] && colInputs[col])
          {
            if (OCTAVE == 4)
            {
              keys4.set(keyIndex, false);
              __atomic_store_n(&currentStepSize, 0, __ATOMIC_RELAXED);
            }
            else
            {
              TX_Message[0] = 'R';
              TX_Message[1] = OCTAVE;
              TX_Message[2] = keyIndex;
              xQueueSend(msgOutQ, TX_Message, portMAX_DELAY);
            }
          }
        }
      }
    }
    // **Knob 3 Decoding (A = localInputs[12], B = localInputs[13])**
    currentKnobState[0] = localInputs[12]; // A
    currentKnobState[1] = localInputs[13]; // B

    knob3.updateRotation(currentKnobState);
    localVolume = knob3.getRotationValue();

    // Update global system state atomically and with mutex(copy the input state method)
    if (xSemaphoreTake(sysState.mutex, portMAX_DELAY) == pdTRUE)
    {
      memcpy(&sysState.inputs, &localInputs, sizeof(sysState.inputs)); // Copy input state
      sysState.rotationVariable = localRotationVariable;
      sysState.volume = localVolume;
      xSemaphoreGive(sysState.mutex);
    }

    // Update `currentStepSize` atomically
    // if (RX_Message[0] == 'R')
    // {
    //   uint32_t localCurrentStepSize = (lastPressedKey >= 0 && lastPressedKey < 12) ? stepSizes[lastPressedKey] : 0;
    //   __atomic_store_n(&currentStepSize, localCurrentStepSize, __ATOMIC_RELAXED);
    // }
  }
}

void displayUpdateTask(void *pvParameters)
{
  const TickType_t xFrequency = 100 / portTICK_PERIOD_MS;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  uint32_t ID = 0x123;
  while (1)
  {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    std::bitset<32> localInputs;

    // Lock mutex briefly to copy global state
    if (xSemaphoreTake(sysState.mutex, portMAX_DELAY) == pdTRUE)
    {
      memcpy(&localInputs, &sysState.inputs, sizeof(localInputs));
      xSemaphoreGive(sysState.mutex);
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    // u8g2.drawStr(2, 10, "Note:");
    // u8g2.setCursor(2, 20);

    // int lastPressedKey = -1;
    // for (int i = 0; i < 12; i++)
    // {
    //   if (!localInputs[i])
    //   {
    //     lastPressedKey = i;
    //   }
    // }

    // if (lastPressedKey >= 0)
    // {
    //   const char *noteNames[] = {"C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5", "A5", "A#5", "B5"};
    //   u8g2.print(noteNames[lastPressedKey]);
    // }
    // else
    // {
    //   u8g2.print("None");
    // }

    if (OCTAVE == 4)
    {
      u8g2.drawStr(2, 10, "Notes:");

      // Build a string of currently pressed local and remote keys
      // For local keys (C4 range) and remote keys (C5 range)
      int cursorx = 0;

      // For local keys:
      // which bit = true? Show that note name with "C4" style
      for (int i = 0; i < 12; i++)
      {
        if (keys4[i])
        {
          const char *localNoteNames[12] =
              {"C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4", "A#4", "B4"};
          u8g2.setCursor(cursorx, 20);
          u8g2.print(localNoteNames[i]);
          cursorx += 20;
        }
      }

      for (int i = 0; i < 12; i++)
      {
        if (keys5[i])
        {
          // remote note names for C5
          const char *remoteNoteNames[12] =
              {"C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5", "A5", "A#5", "B5"};
          u8g2.setCursor(cursorx, 20);
          u8g2.print(remoteNoteNames[i]);
          cursorx += 20;
        }
      }
    }
    u8g2.setCursor(2, 30);
    u8g2.print(sysState.inputs.to_ulong(), HEX);

    u8g2.setCursor(66, 30);
    u8g2.print((char)RX_Message[0]);
    u8g2.print(RX_Message[1]);
    u8g2.print(RX_Message[2]);

    u8g2.setCursor(66, 10);
    u8g2.print(sysState.volume);
    u8g2.sendBuffer();

    // Toggle LED (Real-time scheduling indicator)
    // digitalToggle(LED_BUILTIN);
  }
}

void setup()
{
  // **Initialize Pins**
  pinMode(RA0_PIN, OUTPUT);
  pinMode(RA1_PIN, OUTPUT);
  pinMode(RA2_PIN, OUTPUT);
  pinMode(REN_PIN, OUTPUT);
  pinMode(OUT_PIN, OUTPUT);
  pinMode(OUTL_PIN, OUTPUT);
  pinMode(OUTR_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(C0_PIN, INPUT);
  pinMode(C1_PIN, INPUT);
  pinMode(C2_PIN, INPUT);
  pinMode(C3_PIN, INPUT);
  pinMode(JOYX_PIN, INPUT);
  pinMode(JOYY_PIN, INPUT);

  // **Initialize Display**
  setOutMuxBit(DRST_BIT, LOW); // Assert display logic reset
  delayMicroseconds(2);
  setOutMuxBit(DRST_BIT, HIGH); // Release display logic reset
  u8g2.begin();
  setOutMuxBit(DEN_BIT, HIGH); // Enable display power supply

  // **Initialize Serial**
  Serial.begin(9600);
  Serial.println("Hello World");

  // **Setup Timer Interrupt for 22kHz Sampling**
  sampleTimer.setOverflow(22000, HERTZ_FORMAT);
  sampleTimer.attachInterrupt(sampleISR);
  sampleTimer.resume();

  xTaskCreate(scanKeysTask, "scanKeys", 128, NULL, 1, &scanKeysHandle);
  xTaskCreate(displayUpdateTask, "displayUpdate", 256, NULL, 2, &displayTaskHandle);

  // add mutux for multithreading synchronization
  sysState.mutex = xSemaphoreCreateMutex();

  // setup volum initial
  sysState.volume = 4;

  // message queue
  msgInQ = xQueueCreate(36, 8);
  msgOutQ = xQueueCreate(36, 8);

  // CAN bus initialization
  CAN_Init(false);
  setCANFilter(0x123, 0x7ff);
  CAN_RegisterRX_ISR(CAN_RX_ISR);
  CAN_RegisterTX_ISR(CAN_TX_ISR); // transmit ISR
  CAN_Start();

  // decode message thread
  xTaskCreate(decodeTask, "decodeTask", 128, NULL, 3, NULL);

  // transmit thread
  xTaskCreate(CAN_TX_Task, "CAN_TX_Task", 128, NULL, 4, NULL);

  // transmit
  CAN_TX_Semaphore = xSemaphoreCreateCounting(3, 3);

  // **Start RTOS Scheduler**
  vTaskStartScheduler();
}

void loop()
{
}