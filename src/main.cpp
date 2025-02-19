#include <Arduino.h>
#include <U8g2lib.h>
#include <bitset>

// Constants
const uint32_t interval = 100;                         // Display update interval
const uint32_t fs = 22000;                             // Sampling frequency (Hz)
const uint64_t phase_accumulator_modulus = 4294967296; // 2^32

// Pin definitions
// Row select and enable
const int RA0_PIN = D3;
const int RA1_PIN = D6;
const int RA2_PIN = D12;
const int REN_PIN = A5;

// Matrix input and output
const int C0_PIN = A2;
const int C1_PIN = D9;
const int C2_PIN = A6;
const int C3_PIN = D1;
const int OUT_PIN = D11;

// Audio analogue out
const int OUTL_PIN = A4;
const int OUTR_PIN = A3;

// Joystick analogue in
const int JOYY_PIN = A0;
const int JOYX_PIN = A1;

// Output multiplexer bits
const int DEN_BIT = 3;
const int DRST_BIT = 4;
const int HKOW_BIT = 5;
const int HKOE_BIT = 6;

// Step Size
// Define step sizes for notes C5 to B5 with explicit casting
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

volatile uint32_t currentStepSize = 0;

HardwareTimer sampleTimer(TIM1);

// Display driver object
U8G2_SSD1305_128X32_ADAFRUIT_F_HW_I2C u8g2(U8G2_R0);

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

void setRow(uint8_t rowIdx)
{
  // Disable row selection to prevent glitches
  digitalWrite(REN_PIN, LOW);

  // Set row address bits
  digitalWrite(RA0_PIN, rowIdx & 0x01);
  digitalWrite(RA1_PIN, rowIdx & 0x02);
  digitalWrite(RA2_PIN, rowIdx & 0x04);

  // Enable row selection
  digitalWrite(REN_PIN, HIGH);
}

std::bitset<4> readCols()
{
  std::bitset<4> result;

  // Set row select address to 0 (RA0, RA1, RA2 low)
  // digitalWrite(RA0_PIN, LOW);
  // digitalWrite(RA1_PIN, LOW);
  // digitalWrite(RA2_PIN, LOW);

  // Enable row selection
  // digitalWrite(REN_PIN, HIGH);
  // delayMicroseconds(2); // Small delay for stability

  // Read column inputs
  result[0] = digitalRead(C0_PIN);
  result[1] = digitalRead(C1_PIN);
  result[2] = digitalRead(C2_PIN);
  result[3] = digitalRead(C3_PIN);

  // Disable row selection
  digitalWrite(REN_PIN, LOW);

  return result;
}

void sampleISR()
{
  static uint32_t phaseAcc = 0; // Phase accumulator (static to retain value)

  // Update phase accumulator
  phaseAcc += currentStepSize;

  // Convert phase accumulator to voltage output (Sawtooth waveform)
  int32_t Vout = (phaseAcc >> 24) - 128; // Scale range: -128 to 127

  // Convert to unsigned value for DAC output (0-255)
  analogWrite(OUTR_PIN, Vout + 128);
}

void setup()
{
  // put your setup code here, to run once:

  // Set pin directions
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

  // Initialise display
  setOutMuxBit(DRST_BIT, LOW); // Assert display logic reset
  delayMicroseconds(2);
  setOutMuxBit(DRST_BIT, HIGH); // Release display logic reset
  u8g2.begin();
  setOutMuxBit(DEN_BIT, HIGH); // Enable display power supply

  // Initialise UART
  Serial.begin(9600);
  Serial.println("Hello World");

  // ISR
  sampleTimer.setOverflow(22000, HERTZ_FORMAT);
  sampleTimer.attachInterrupt(sampleISR);
  sampleTimer.resume();
}

void loop()
{
  // put your main code here, to run repeatedly:
  static uint32_t next = millis();
  static uint32_t count = 0;

  while (millis() < next)
    ; // Wait for next interval

  next += interval;

  std::bitset<32> inputs;
  int lastPressedKey = -1; // Track last pressed key index

  // Scan keys
  for (uint8_t row = 0; row < 3; row++)
  {
    setRow(row);
    delayMicroseconds(3);
    std::bitset<4> colInputs = readCols();

    for (uint8_t col = 0; col < 4; col++)
    {
      int keyIndex = row * 4 + col;
      inputs[keyIndex] = colInputs[col];
      if (!colInputs[col])
      {
        lastPressedKey = keyIndex; // Store the last pressed key
      }
    }
  }

  // Determine current step size based on last pressed key
  if (lastPressedKey >= 0 && lastPressedKey < 12)
  {
    currentStepSize = stepSizes[lastPressedKey];
  }
  else
  {
    currentStepSize = 0;
  }

  // Update display
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(2, 10, "Note:");
  u8g2.setCursor(2, 20);

  if (lastPressedKey >= 0 && lastPressedKey < 12)
  {
    const char *noteNames[] = {"C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5", "A5", "A#5", "B5"};
    u8g2.print(noteNames[lastPressedKey]);
  }
  else
  {
    u8g2.print("None");
  }

  u8g2.setCursor(2, 30);
  u8g2.print(inputs.to_ulong(), HEX);
  u8g2.sendBuffer();

  // Debugging output to Serial Monitor
  // Serial.print("Pressed Key: ");
  // Serial.print(lastPressedKey);
  // Serial.print(" Step Size: ");
  // Serial.println(currentStepSize);
  // Serial.println(inputs.to_string().c_str());

  // Toggle LED
  digitalToggle(LED_BUILTIN);
}