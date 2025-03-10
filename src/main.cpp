#include <Arduino.h>
<<<<<<< HEAD
#include <U8g2lib.h>
#include <bitset>
#include <STM32FreeRTOS.h> // FreeRTOS for threading support
#include <knob.h>
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
const uint32_t stepSizes[] = {
    static_cast<uint32_t>((phase_accumulator_modulus * 523.25) / fs), // C5
    static_cast<uint32_t>((phase_accumulator_modulus * 554.37) / fs), // C#5/Db5
    static_cast<uint32_t>((phase_accumulator_modulus * 587.33) / fs), // D5
    static_cast<uint32_t>((phase_accumulator_modulus * 622.25) / fs), // D#5/Eb5
    static_cast<uint32_t>((phase_accumulator_modulus * 659.25) / fs), // E5
    static_cast<uint32_t>((phase_accumulator_modulus * 698.46) / fs), // F5
    static_cast<uint32_t>((phase_accumulator_modulus * 739.99) / fs), // F#5/Gb5
    static_cast<uint32_t>((phase_accumulator_modulus * 783.99) / fs), // G5
    static_cast<uint32_t>((phase_accumulator_modulus * 830.61) / fs), // G#5/Ab5
    static_cast<uint32_t>((phase_accumulator_modulus * 880.00) / fs), // A5
    static_cast<uint32_t>((phase_accumulator_modulus * 932.33) / fs), // A#5/Bb5
    static_cast<uint32_t>((phase_accumulator_modulus * 987.77) / fs)  // B5
};

// **Global Variable
volatile uint32_t currentStepSize = 0;
Knob knob3(0, 8);

struct // Struct to Store System State
{
  std::bitset<32> inputs;
  SemaphoreHandle_t mutex;
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
    int localVolume = sysState.volume;

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
        if (!colInputs[col] && keyIndex <= 11 && keyIndex >= 0)
        {
          lastPressedKey = keyIndex;
        }
      }
    }

    // **Knob 3 Decoding (A = localInputs[12], B = localInputs[13])**
    currentKnobState[0] = localInputs[12]; // A
    currentKnobState[1] = localInputs[13]; // B
    Serial.print("Knob State: ");
    Serial.print(currentKnobState[1]);
    Serial.println(currentKnobState[0]);
    knob3.updateRotation(currentKnobState);
    localVolume = knob3.getRotationValue();
    // Update global system state atomically and with mutex(copy the input state method)
    if (xSemaphoreTake(sysState.mutex, portMAX_DELAY) == pdTRUE)
    {
      memcpy(&sysState.inputs, &localInputs, sizeof(sysState.inputs)); // Copy input state
      sysState.volume = localVolume;
      xSemaphoreGive(sysState.mutex);
    }

    // Update currentStepSize atomically
    uint32_t localCurrentStepSize = (lastPressedKey >= 0 && lastPressedKey < 12) ? stepSizes[lastPressedKey] : 0;
    __atomic_store_n(&currentStepSize, localCurrentStepSize, __ATOMIC_RELAXED);
  }
}

void displayUpdateTask(void *pvParameters)
{
  const TickType_t xFrequency = 100 / portTICK_PERIOD_MS;
  TickType_t xLastWakeTime = xTaskGetTickCount();

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
    u8g2.drawStr(2, 10, "Note:");
    u8g2.setCursor(2, 20);

    int lastPressedKey = -1;
    for (int i = 0; i < 12; i++)
    {
      if (!localInputs[i])
      {
        lastPressedKey = i;
      }
    }

    if (lastPressedKey >= 0)
    {
      const char *noteNames[] = {"C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5", "A5", "A#5", "B5"};
      u8g2.print(noteNames[lastPressedKey]);
    }
    else
    {
      u8g2.print("None");
    }

    u8g2.setCursor(2, 30);
    u8g2.print(sysState.inputs.to_ulong(), HEX);
    u8g2.sendBuffer();

    // Toggle LED (Real-time scheduling indicator)
    digitalToggle(LED_BUILTIN);
  }
}
=======
#include "globals.h" // For all our shared globals
#include "can.h"     // For CAN tasks and ISRs
#include "key.h"     // for scanKeysTask(...)
#include "display.h" // For scanKeysTask, displayUpdateTask
#include "pins.h"
#include "isr.h"
#include "sampler.h"
>>>>>>> recovered-sampler

void setup()
{
    // ----------------- Pin Setup -----------------
    pinMode(D3, OUTPUT);
    pinMode(D6, OUTPUT);
    pinMode(D12, OUTPUT);
    pinMode(A5, OUTPUT); // REN
    pinMode(D11, OUTPUT);
    pinMode(A4, OUTPUT);
    pinMode(A3, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);

    pinMode(A2, INPUT);
    pinMode(D9, INPUT);
    pinMode(A6, INPUT);
    pinMode(D1, INPUT);
    pinMode(JOYX_PIN, INPUT);
    pinMode(JOYY_PIN, INPUT);

    // Display setup
    setOutMuxBit(DRST_BIT, LOW);
    delayMicroseconds(2);
    setOutMuxBit(DRST_BIT, HIGH);
    u8g2.begin();
    setOutMuxBit(DEN_BIT, HIGH);

    Serial.begin(9600);
    Serial.println("Hello World");

    // Audio Timer for 22 kHz
    sampleTimer.setOverflow(fs, HERTZ_FORMAT);
    sampleTimer.attachInterrupt(sampleISR);
    sampleTimer.resume();

    // Create RTOS tasks
    xTaskCreate(scanKeysTask, "scanKeys", 256, NULL, 1, &scanKeysHandle);
    xTaskCreate(displayUpdateTask, "displayUpdate", 256, NULL, 2, &displayTaskHandle);

    // Mutex for sysState
    sysState.mutex = xSemaphoreCreateMutex();
    sysState.volume = 4;
    sampler_init();

    // CAN Queues
    msgInQ = xQueueCreate(36, 8);
    msgOutQ = xQueueCreate(36, 8);

    // Init CAN
    CAN_Init(false);
    setCANFilter(0x123, 0x7ff);
    CAN_RegisterRX_ISR(CAN_RX_ISR);
    CAN_RegisterTX_ISR(CAN_TX_ISR);
    CAN_Start();

    // Create decode & transmit tasks
    xTaskCreate(decodeTask, "decodeTask", 128, NULL, 3, NULL);
    xTaskCreate(CAN_TX_Task, "CAN_TX_Task", 128, NULL, 4, NULL);

    if (OCTAVE == 4)
    {
        sampler_init();
        xTaskCreate(samplerTask, "samplerTask", 256, NULL, 5, NULL);
        xTaskCreate(metronomeTask, "metronomeTask", 128, NULL, 6, NULL);
    }

    // Counting semaphore for CAN TX
    CAN_TX_Semaphore = xSemaphoreCreateCounting(3, 3);

    // Start FreeRTOS
    vTaskStartScheduler();
}

void loop()
{
    // Not used; tasks run under FreeRTOS
}
