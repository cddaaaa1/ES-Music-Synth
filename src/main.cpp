#include <Arduino.h>
#include <U8g2lib.h>
#include <bitset>
#include <STM32FreeRTOS.h> // FreeRTOS for threading support

// Constants
const uint32_t interval = 100;                           // Display update interval
const uint32_t fs = 22000;                               // Sampling frequency (Hz)
const uint64_t phase_accumulator_modulus = (1ULL << 32); // 2^32 using bit shift

// Struct to Store System State
struct
{
  std::bitset<32> inputs; // Stores the key states
} sysState;

// Pin Definitions
const int RA0_PIN = D3, RA1_PIN = D6, RA2_PIN = D12, REN_PIN = A5, C0_PIN = A2, C1_PIN = D9, C2_PIN = A6, C3_PIN = D1, OUT_PIN = D11, OUTL_PIN = A4, OUTR_PIN = A3, JOYY_PIN = A0, JOYX_PIN = A1;

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

// **Global Variable Updated Atomically**
volatile uint32_t currentStepSize = 0;

HardwareTimer sampleTimer(TIM1);
U8G2_SSD1305_128X32_ADAFRUIT_F_HW_I2C u8g2(U8G2_R0);

// **Thread Handle**
TaskHandle_t scanKeysHandle = NULL;

// **Interrupt Service Routine for Audio Generation**
void sampleISR()
{
  static uint32_t phaseAcc = 0;
  phaseAcc += currentStepSize;
  int32_t Vout = (phaseAcc >> 24) - 128; // Scale range: -128 to 127
  analogWrite(OUTR_PIN, Vout + 128);     // Convert to unsigned (0-255) for DAC
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

    for (uint8_t row = 0; row < 3; row++)
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
        if (!colInputs[col])
        {
          lastPressedKey = keyIndex;
        }
      }
    }

    // Update global system state atomically
    sysState.inputs = localInputs;

    // Update `currentStepSize` atomically
    uint32_t localCurrentStepSize = (lastPressedKey >= 0 && lastPressedKey < 12) ? stepSizes[lastPressedKey] : 0;
    __atomic_store_n(&currentStepSize, localCurrentStepSize, __ATOMIC_RELAXED);

    // vTaskDelay(pdMS_TO_TICKS(5)); // Add slight delay to prevent excessive CPU usage
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
  u8g2.begin();

  // **Initialize Serial**
  Serial.begin(9600);
  Serial.println("Hello World");

  // **Setup Timer Interrupt for 22kHz Sampling**
  sampleTimer.setOverflow(22000, HERTZ_FORMAT);
  sampleTimer.attachInterrupt(sampleISR);
  sampleTimer.resume();

  // **Create RTOS Task for Key Scanning**
  xTaskCreate(
      scanKeysTask,   // Task function
      "scanKeys",     // Name
      64,             // Stack size in words
      NULL,           // Task parameter
      1,              // Priority
      &scanKeysHandle // Handle
  );

  // **Start RTOS Scheduler**
  // vTaskStartScheduler();
}

void loop()
{
  static uint32_t next = millis();
  static uint32_t count = 0;

  while (millis() < next)
    ; // Wait for next interval

  next += interval;
  // **Update Display (Runs in Main Loop)**
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(2, 10, "Note:");
  u8g2.setCursor(2, 20);

  int lastPressedKey = -1;
  for (int i = 0; i < 12; i++)
  {
    if (sysState.inputs[i])
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
  digitalToggle(LED_BUILTIN);
}
