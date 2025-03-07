#include "globals.h"
#include "tasks.h"
#include "pins.h"
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
