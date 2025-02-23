#include <Arduino.h>
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

  // **Start RTOS Scheduler**
  vTaskStartScheduler();
}

void loop()
{
}