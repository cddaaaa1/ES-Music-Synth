#include "display.h"
#include "globals.h" // for sysState, RX_Message, keys4, keys5, u8g2, etc.
#include "pins.h"    // if you need pin definitions, e.g. LED pins
#include <U8g2lib.h> // for u8g2
#include <bitset>

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
        // if (xSemaphoreTake(sysState.mutex, portMAX_DELAY) == pdTRUE)
        // {
        //     memcpy(&localInputs, &sysState.inputs, sizeof(localInputs));
        //     xSemaphoreGive(sysState.mutex);
        // }

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

        if (moduleOctave == 4)
        {
            u8g2.drawStr(2, 10, "Notes:");

            // Build a string of currently pressed local and remote keys
            // For local keys (C4 range) and remote keys (C5 range)
            int cursorx = 40;

            // For local keys:
            // which bit = true? Show that note name with "C4" style
            for (int i = 0; i < 12; i++)
            {
                if (xSemaphoreTake(localKeyMutex, portMAX_DELAY) == pdTRUE)
                {
                    if (keys4[i])
                    {
                        const char *localNoteNames[12] =
                            {"C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4", "A#4", "B4"};
                        u8g2.setCursor(cursorx, 10);
                        u8g2.print(localNoteNames[i]);
                        cursorx += 15;
                    }
                    xSemaphoreGive(localKeyMutex);
                }
            }

            for (int i = 0; i < 12; i++)
            {
                if (keys5[i])
                {
                    // remote note names for C5
                    const char *remoteNoteNames[12] =
                        {"C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5", "A5", "A#5", "B5"};
                    u8g2.setCursor(cursorx, 10);
                    u8g2.print(remoteNoteNames[i]);
                    cursorx += 15;
                }
            }
            for (int i = 0; i < 12; i++)
            {
                if (keys6[i])
                {
                    const char *sixthNoteNames[12] =
                        {"C6", "C#6", "D6", "D#6", "E6", "F6", "F#6", "G6", "G#6", "A6", "A#6", "B6"};
                    u8g2.setCursor(cursorx, 10);
                    u8g2.print(sixthNoteNames[i]);
                    cursorx += 15;
                }
            }
            u8g2.setCursor(2, 20);
            u8g2.print("Volume:");
            u8g2.setCursor(50, 20);
            u8g2.print(sysState.volume);

            u8g2.setCursor(2, 30);
            if (knob2.getPress())
            {
                u8g2.print("Sampler Enabled");
            }
            else
            {
                u8g2.print("Sampler Disabled");
            }
        }
        if (moduleOctave == 5)
        {
            u8g2.drawStr(2, 10, "Octave 5");
        }
        if (moduleOctave == 6)
        {
            u8g2.drawStr(2, 10, "Octave 6");
        }
        u8g2.sendBuffer();
    }
}
