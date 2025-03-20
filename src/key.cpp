#include "key.h"     // Corresponding header
#include "globals.h" // For global variables, e.g., sysState, keys4, ...
#include "pins.h"    // For pin definitions (RA0_PIN, etc.)
#include "sampler.h"
#include "autodetection.h"
#include <bitset>

#include <vector>
#include <bitset>
#include <stdint.h>
#define MAX_VOICES 5

uint32_t scanKeysIterations = 0;
TickType_t scanKeysStartTime = 0;

enum KeyboardType
{
    KEYBOARD_4,
    KEYBOARD_5,
    KEYBOARD_6
};

struct NoteRef
{
    KeyboardType kb; // Which keyboard: C4, C5, or C6
    uint8_t noteIdx; // Note index (0..11)
};

// This function fills 5 step-size outputs (max voices) from three keyboards.
void setStepSizes()
{
    uint32_t localStepSize1 = 0, localStepSize2 = 0, localStepSize3 = 0, localStepSize4 = 0, localStepSize5 = 0;

    std::vector<NoteRef> finalNotes;
    finalNotes.reserve(MAX_VOICES);

    for (uint8_t i = 0; i < 12; i++)
    {
        if (xSemaphoreTake(localKeyMutex, portMAX_DELAY) == pdTRUE)
        {
            if (keys4.test(i))
            {
                if (finalNotes.size() < MAX_VOICES)
                    finalNotes.push_back({KEYBOARD_4, i});
                else
                    break;
            }
            xSemaphoreGive(localKeyMutex);
        }
    }
    for (uint8_t i = 0; i < 12; i++)
    {
        if (xSemaphoreTake(externalKeyMutex, portMAX_DELAY) == pdTRUE)
        {
            if (keys5.test(i))
            {
                if (finalNotes.size() < MAX_VOICES)
                    finalNotes.push_back({KEYBOARD_5, i});
                else
                    break;
            }
            xSemaphoreGive(externalKeyMutex);
        }
    }
    for (uint8_t i = 0; i < 12; i++)
    {
        if (xSemaphoreTake(externalKeyMutex, portMAX_DELAY) == pdTRUE)
        {
            if (keys6.test(i))
            {
                if (finalNotes.size() < MAX_VOICES)
                    finalNotes.push_back({KEYBOARD_6, i});
                else
                    break;
            }
            xSemaphoreGive(externalKeyMutex);
        }
    }
    if (finalNotes.size() >= 1)
    {
        switch (finalNotes[0].kb)
        {
        case KEYBOARD_4:
            localStepSize1 = stepSizes4[finalNotes[0].noteIdx];
            break;
        case KEYBOARD_5:
            localStepSize1 = stepSizes5[finalNotes[0].noteIdx];
            break;
        case KEYBOARD_6:
            localStepSize1 = stepSizes6[finalNotes[0].noteIdx];
            break;
        }
    }
    if (finalNotes.size() >= 2)
    {
        switch (finalNotes[1].kb)
        {
        case KEYBOARD_4:
            localStepSize2 = stepSizes4[finalNotes[1].noteIdx];
            break;
        case KEYBOARD_5:
            localStepSize2 = stepSizes5[finalNotes[1].noteIdx];
            break;
        case KEYBOARD_6:
            localStepSize2 = stepSizes6[finalNotes[1].noteIdx];
            break;
        }
    }
    if (finalNotes.size() >= 3)
    {
        switch (finalNotes[2].kb)
        {
        case KEYBOARD_4:
            localStepSize3 = stepSizes4[finalNotes[2].noteIdx];
            break;
        case KEYBOARD_5:
            localStepSize3 = stepSizes5[finalNotes[2].noteIdx];
            break;
        case KEYBOARD_6:
            localStepSize3 = stepSizes6[finalNotes[2].noteIdx];
            break;
        }
    }
    if (finalNotes.size() >= 4)
    {
        switch (finalNotes[3].kb)
        {
        case KEYBOARD_4:
            localStepSize4 = stepSizes4[finalNotes[3].noteIdx];
            break;
        case KEYBOARD_5:
            localStepSize4 = stepSizes5[finalNotes[3].noteIdx];
            break;
        case KEYBOARD_6:
            localStepSize4 = stepSizes6[finalNotes[3].noteIdx];
            break;
        }
    }
    if (finalNotes.size() >= 5)
    {
        switch (finalNotes[4].kb)
        {
        case KEYBOARD_4:
            localStepSize5 = stepSizes4[finalNotes[4].noteIdx];
            break;
        case KEYBOARD_5:
            localStepSize5 = stepSizes5[finalNotes[4].noteIdx];
            break;
        case KEYBOARD_6:
            localStepSize5 = stepSizes6[finalNotes[4].noteIdx];
            break;
        }
    }

    // Atomically update the global step-size variables.
    __atomic_store_n(&currentStepSize1, localStepSize1, __ATOMIC_RELAXED);
    __atomic_store_n(&currentStepSize2, localStepSize2, __ATOMIC_RELAXED);
    __atomic_store_n(&currentStepSize3, localStepSize3, __ATOMIC_RELAXED);
    __atomic_store_n(&currentStepSize4, localStepSize4, __ATOMIC_RELAXED);
    __atomic_store_n(&currentStepSize5, localStepSize5, __ATOMIC_RELAXED);
}

void scanKeysTask(void *pvParameters)
{
    const TickType_t xFrequency = 5 / portTICK_PERIOD_MS;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1)
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        std::bitset<32> localInputs;
        int lastPressedKey = -1;
        std::bitset<2> currentKnobState;
        int localRotationVariable = 0;
        int localVolume = sysState.volume;
        std::bitset<32> previousInput = sysState.inputs;
        uint8_t TX_Message[8] = {0};
        bool west, east;
        bool sampler_enabled = sysState.knob2.getPress();
        for (uint8_t row = 0; row < 7; row++)
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
                    if (previousInput[keyIndex] && !colInputs[col])
                    {
                        lastPressedKey = keyIndex;
                        if (moduleOctave == 4)
                        {
                            if (xSemaphoreTake(localKeyMutex, portMAX_DELAY) == pdTRUE)
                            {
                                keys4.set(keyIndex, true);
                                xSemaphoreGive(localKeyMutex);
                            }
                            __atomic_store_n(&currentStepSize, stepSizes4[lastPressedKey], __ATOMIC_RELAXED);
                            if (sampler_enabled)
                            {
                                sampler_recordEvent('P', moduleOctave, (uint8_t)keyIndex);
                            }
                        }
                        else
                        {
                            TX_Message[0] = 'P';
                            TX_Message[1] = moduleOctave;
                            TX_Message[2] = lastPressedKey;
                            xQueueSend(msgOutQ, TX_Message, portMAX_DELAY);
                        }
                    }
                    if (!previousInput[keyIndex] && colInputs[col])
                    {
                        if (moduleOctave == 4)
                        {
                            if (xSemaphoreTake(localKeyMutex, portMAX_DELAY) == pdTRUE)
                            {
                                keys4.set(keyIndex, false);
                                xSemaphoreGive(localKeyMutex);
                            }
                            __atomic_store_n(&currentStepSize, 0, __ATOMIC_RELAXED);
                            if (sampler_enabled)
                            {
                                sampler_recordEvent('R', moduleOctave, (uint8_t)keyIndex);
                            }
                        }
                        else
                        {
                            TX_Message[0] = 'R';
                            TX_Message[1] = moduleOctave;
                            TX_Message[2] = keyIndex;
                            xQueueSend(msgOutQ, TX_Message, portMAX_DELAY);
                        }
                    }
                }
            }
        }
        setStepSizes();
        readHandshake(west, east);

        // If either handshake input has changed, trigger auto-detection
        if (west != prevWest || east != prevEast)
        {
            autoDetectHandshake();
            TX_Message[0] = 'H';
            TX_Message[1] = moduleOctave;
            xQueueSend(msgOutQ, TX_Message, portMAX_DELAY);
        }
        prevWest = west;
        prevEast = east;

        // Knob 3 for volume adjust
        currentKnobState[0] = localInputs[12]; // A
        currentKnobState[1] = localInputs[13]; // B
        sysState.knob3.updateRotation(currentKnobState);
        localVolume = sysState.knob3.getRotationValue();

        // knob 2 for deciding whether sampleing
        std::bitset<1> currentPressKnob2;
        currentPressKnob2[0] = localInputs[20];

        // Update global system state atomically and with mutex
        if (xSemaphoreTake(sysState.mutex, portMAX_DELAY) == pdTRUE)
        {
            memcpy(&sysState.inputs, &localInputs, sizeof(sysState.inputs));
            sysState.knob2.updatePress(currentPressKnob2);
            sysState.knob3.updateRotation(currentKnobState);
            localVolume = sysState.knob3.getRotationValue();
            xSemaphoreGive(sysState.mutex);
        }
        sysState.rotationVariable = localRotationVariable;
        __atomic_store_n(&sysState.volume, localVolume, __ATOMIC_RELAXED);
    }
}

void setStepSizesFunction()
{

    uint32_t localStepSize1 = 0, localStepSize2 = 0, localStepSize3 = 0, localStepSize4 = 0, localStepSize5 = 0;

    std::vector<NoteRef> finalNotes;
    finalNotes.reserve(MAX_VOICES);

    for (uint8_t i = 0; i < 12; i++)
    {
        if (keys4.test(i))
        {
            if (finalNotes.size() < MAX_VOICES)
                finalNotes.push_back({KEYBOARD_4, i});
            else
                break;
        }
    }
    for (uint8_t i = 0; i < 12; i++)
    {
        if (keys5.test(i))
        {
            if (finalNotes.size() < MAX_VOICES)
                finalNotes.push_back({KEYBOARD_5, i});
            else
                break;
        }
    }
    for (uint8_t i = 0; i < 12; i++)
    {
        if (keys6.test(i))
        {
            if (finalNotes.size() < MAX_VOICES)
                finalNotes.push_back({KEYBOARD_6, i});
            else
                break;
        }
    }

    if (finalNotes.size() >= 1)
    {
        switch (finalNotes[0].kb)
        {
        case KEYBOARD_4:
            localStepSize1 = stepSizes4[finalNotes[0].noteIdx];
            break;
        case KEYBOARD_5:
            localStepSize1 = stepSizes5[finalNotes[0].noteIdx];
            break;
        case KEYBOARD_6:
            localStepSize1 = stepSizes6[finalNotes[0].noteIdx];
            break;
        }
    }
    if (finalNotes.size() >= 2)
    {
        switch (finalNotes[1].kb)
        {
        case KEYBOARD_4:
            localStepSize2 = stepSizes4[finalNotes[1].noteIdx];
            break;
        case KEYBOARD_5:
            localStepSize2 = stepSizes5[finalNotes[1].noteIdx];
            break;
        case KEYBOARD_6:
            localStepSize2 = stepSizes6[finalNotes[1].noteIdx];
            break;
        }
    }
    if (finalNotes.size() >= 3)
    {
        switch (finalNotes[2].kb)
        {
        case KEYBOARD_4:
            localStepSize3 = stepSizes4[finalNotes[2].noteIdx];
            break;
        case KEYBOARD_5:
            localStepSize3 = stepSizes5[finalNotes[2].noteIdx];
            break;
        case KEYBOARD_6:
            localStepSize3 = stepSizes6[finalNotes[2].noteIdx];
            break;
        }
    }
    if (finalNotes.size() >= 4)
    {
        switch (finalNotes[3].kb)
        {
        case KEYBOARD_4:
            localStepSize4 = stepSizes4[finalNotes[3].noteIdx];
            break;
        case KEYBOARD_5:
            localStepSize4 = stepSizes5[finalNotes[3].noteIdx];
            break;
        case KEYBOARD_6:
            localStepSize4 = stepSizes6[finalNotes[3].noteIdx];
            break;
        }
    }
    if (finalNotes.size() >= 5)
    {
        switch (finalNotes[4].kb)
        {
        case KEYBOARD_4:
            localStepSize5 = stepSizes4[finalNotes[4].noteIdx];
            break;
        case KEYBOARD_5:
            localStepSize5 = stepSizes5[finalNotes[4].noteIdx];
            break;
        case KEYBOARD_6:
            localStepSize5 = stepSizes6[finalNotes[4].noteIdx];
            break;
        }
    }

    // Atomically update the global step-size variables.
    __atomic_store_n(&currentStepSize1, localStepSize1, __ATOMIC_RELAXED);
    __atomic_store_n(&currentStepSize2, localStepSize2, __ATOMIC_RELAXED);
    __atomic_store_n(&currentStepSize3, localStepSize3, __ATOMIC_RELAXED);
    __atomic_store_n(&currentStepSize4, localStepSize4, __ATOMIC_RELAXED);
    __atomic_store_n(&currentStepSize5, localStepSize5, __ATOMIC_RELAXED);
}

void scanKeysFunction(void *pvParameters)
{
    const TickType_t xFrequency = 50 / portTICK_PERIOD_MS;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    if (scanKeysIterations == 0)
    {
        scanKeysStartTime = xTaskGetTickCount();
    }
    while (1)
    {
        scanKeysIterations++;
        std::bitset<32> localInputs;
        int lastPressedKey = -1;
        std::bitset<2> currentKnobState;
        int localRotationVariable = 0;
        int localVolume = sysState.volume;
        std::bitset<32> previousInput = sysState.inputs;

        localInputs.set();

        for (uint8_t row = 0; row < 7; row++)
        {

            digitalWrite(REN_PIN, LOW);
            digitalWrite(RA0_PIN, row & 0x01);
            digitalWrite(RA1_PIN, row & 0x02);
            digitalWrite(RA2_PIN, row & 0x04);
            digitalWrite(REN_PIN, HIGH);
            delayMicroseconds(3);

            std::bitset<4> colInputs;
            colInputs.set();
            digitalWrite(REN_PIN, LOW);

            for (uint8_t col = 0; col < 4; col++)
            {
                int keyIndex = row * 4 + col;
                localInputs[keyIndex] = colInputs[col];

                if (keyIndex <= 11 && keyIndex >= 0)
                {
                    if (previousInput[keyIndex] && !colInputs[col])
                    {
                        lastPressedKey = keyIndex;
                        if (moduleOctave == 4)
                        {
                            keys4.set(keyIndex, true);
                            __atomic_store_n(&currentStepSize, stepSizes4[lastPressedKey], __ATOMIC_RELAXED);
                        }
                    }
                    if (!previousInput[keyIndex] && colInputs[col])
                    {
                        if (moduleOctave == 4)
                        {
                            keys4.set(keyIndex, false);
                            __atomic_store_n(&currentStepSize, 0, __ATOMIC_RELAXED);
                        }
                    }
                }
            }
        }

        setStepSizesFunction();

        currentKnobState.set();
        knob3.updateRotation(currentKnobState);
        localVolume = knob3.getRotationValue();
    }
}
