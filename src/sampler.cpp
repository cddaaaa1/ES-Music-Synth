#include "sampler.h"
#include <STM32FreeRTOS.h>
#include <Arduino.h>
#include <string.h>
#include "globals.h"

uint32_t samplerIterations = 0;
TickType_t samplerStartTime = 0;

uint32_t metronomeIterations = 0;
TickType_t metronomeStartTime = 0;

const uint32_t BPM = 100;       // Beats per minute
const uint32_t beatsPerBar = 4; // For a 4/4 time signature
// Compute samplerLoopLength based on BPM (in milliseconds):
const uint32_t samplerLoopLength = (60000UL / BPM) * beatsPerBar*2; // 2400 ms at 100 BPM

const int MAX_EVENTS = 128;
static NoteEvent recordingBuffer[MAX_EVENTS];
static int recordedCount = 0;
static NoteEvent playbackBuffer[MAX_EVENTS];
static int playbackCount = 0;

static SemaphoreHandle_t samplerMutex = NULL;

// This function simulates a key event during playback by updating the external
void simulateKeyEvent(const NoteEvent &event)
{
    if (event.octave == 4)
    {
        if (xSemaphoreTake(localKeyMutex, portMAX_DELAY) == pdTRUE)
        {
            if (event.type == 'P')
            {
                keys4.set(event.noteIndex, true);
            }
            else if (event.type == 'R')
            {
                keys4.set(event.noteIndex, false);
            }
            xSemaphoreGive(localKeyMutex);
        }
    }
    if (event.octave == 5)
    {
        if (xSemaphoreTake(externalKeyMutex, portMAX_DELAY) == pdTRUE)
        {

            if (event.type == 'P')
            {
                keys5.set(event.noteIndex, true);
            }
            else if (event.type == 'R')
            {
                keys5.set(event.noteIndex, false);
            }
            xSemaphoreGive(externalKeyMutex);
        }
    }
    if (event.octave == 6)
    {
        if (xSemaphoreTake(externalKeyMutex, portMAX_DELAY) == pdTRUE)
        {

            if (event.type == 'P')
            {
                keys6.set(event.noteIndex, true);
            }
            else if (event.type == 'R')
            {
                keys6.set(event.noteIndex, false);
            }
            xSemaphoreGive(externalKeyMutex);
        }
    }
}
void sampler_init()
{
    samplerMutex = xSemaphoreCreateMutex();
}

void sampler_recordEvent(char type, uint8_t octave, uint8_t noteIndex)
{
    bool sampler_enabled;
    if (xSemaphoreTake(sysState.mutex, portMAX_DELAY) == pdTRUE)
    {
        sampler_enabled = sysState.knob2.getPress();
        xSemaphoreGive(sysState.mutex);
    }
    if (!sampler_enabled)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
        return;
    }

    uint32_t ts = xTaskGetTickCount() - samplerLoopStartTime;
    NoteEvent event = {ts, type, octave, noteIndex};

    xSemaphoreTake(samplerMutex, portMAX_DELAY);
    if (recordedCount < MAX_EVENTS)
    {
        recordingBuffer[recordedCount++] = event;
    }
    xSemaphoreGive(samplerMutex);
}

void releaseAllNotes()
{
    if (xSemaphoreTake(localKeyMutex, portMAX_DELAY) == pdTRUE)
    {
        for (size_t i = 0; i < keys4.size(); i++)
        {
            if (keys4.test(i))
            {
                keys4.set(i, false);
            }
        }
        xSemaphoreGive(localKeyMutex);
    }
    if (xSemaphoreTake(externalKeyMutex, portMAX_DELAY) == pdTRUE)
    {
        for (size_t i = 0; i < keys5.size(); i++)
        {
            if (keys5.test(i))
            {
                keys5.set(i, false);
            }
        }
        xSemaphoreGive(externalKeyMutex);
    }
    if (xSemaphoreTake(externalKeyMutex, portMAX_DELAY) == pdTRUE)
    {
        for (size_t i = 0; i < keys6.size(); i++)
        {
            if (keys6.test(i))
            {
                keys6.set(i, false);
            }
        }
        xSemaphoreGive(externalKeyMutex);
    }
}

// Reset buffers and counters.
void resetSamplerState()
{
    // Reset buffers and counters.
    releaseAllNotes();
    xSemaphoreTake(samplerMutex, portMAX_DELAY);
    recordedCount = 0;
    playbackCount = 0;
    xSemaphoreGive(samplerMutex);
    samplerLoopStartTime = 0;
}

void samplerTask(void *pvParameters)
{

    const TickType_t loopTicks = pdMS_TO_TICKS(samplerLoopLength);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    bool sampler_enabled = 0;
    bool prevSamplerEnabled = 0;
    while (1)
    {
        if (xSemaphoreTake(sysState.mutex, portMAX_DELAY) == pdTRUE)
        {
            sampler_enabled = sysState.knob2.getPress();
            xSemaphoreGive(sysState.mutex);
        }
        if (prevSamplerEnabled && !sampler_enabled)
        {
            // when exit samplier mode reset the state.
            resetSamplerState();
        }
        prevSamplerEnabled = sampler_enabled;
        if (!sampler_enabled)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        samplerLoopStartTime = xTaskGetTickCount();

        // Playback events recorded in the previous loop cycle.
        uint32_t lastTimestamp = 0;
        for (int i = 0; i < playbackCount; i++)
        {
            // Delay between events
            uint32_t delayTime = playbackBuffer[i].timestamp - lastTimestamp;
            vTaskDelay(pdMS_TO_TICKS(delayTime));
            lastTimestamp = playbackBuffer[i].timestamp;
            simulateKeyEvent(playbackBuffer[i]);
        }

        vTaskDelayUntil(&xLastWakeTime, loopTicks);

        // Update playbackBuffer only if new events have been recorded.
        xSemaphoreTake(samplerMutex, portMAX_DELAY);
        if (recordedCount > 0)
        {
            if (playbackCount + recordedCount <= MAX_EVENTS)
            {
                memcpy(&playbackBuffer[playbackCount], recordingBuffer, recordedCount * sizeof(NoteEvent));
                playbackCount += recordedCount;
            }
            recordedCount = 0;

            // Use a simple insertion sort
            for (int i = 1; i < playbackCount; i++)
            {
                NoteEvent key = playbackBuffer[i];
                int j = i - 1;
                while (j >= 0 && playbackBuffer[j].timestamp > key.timestamp)
                {
                    playbackBuffer[j + 1] = playbackBuffer[j];
                    j--;
                }
                playbackBuffer[j + 1] = key;
            }
        }
        xSemaphoreGive(samplerMutex);
    }
}

// simple metronome
void metronomeTask(void *pvParameters)
{
    const TickType_t beatDelay = pdMS_TO_TICKS(60000UL / BPM);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    bool sampler_enabled = 0;

    while (1)
    {
        if (xSemaphoreTake(sysState.mutex, portMAX_DELAY) == pdTRUE)
        {
            sampler_enabled = sysState.knob2.getPress();
            xSemaphoreGive(sysState.mutex);
        }

        if (sampler_enabled)
        {
            metronomeActive = true;
            metronomeCounter = TICK_DURATION_SAMPLES;
        }

        vTaskDelayUntil(&xLastWakeTime, beatDelay);
    }
}

void metronomeFunction(void *pvParameters)//WCET test function
{
    const TickType_t beatDelay = pdMS_TO_TICKS(60000UL / 300);

    if (metronomeIterations == 0)
    {
        metronomeStartTime = xTaskGetTickCount();
    }

    while (1)
    {
        metronomeIterations++;

        samplerEnabled = true;

        for (int j = 0; j < 10; j++)
        {
            digitalToggle(LED_BUILTIN);
        }

        metronomeActive = true;
        metronomeCounter = UINT32_MAX;

        TickType_t startTick = xTaskGetTickCount();
        while (xTaskGetTickCount() - startTick < beatDelay)
        {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}


void samplerFunction(void *pvParameters)// WCET test function
{
    const TickType_t loopTicks = pdMS_TO_TICKS(1);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    bool prevSamplerEnabled = true;

    if (samplerIterations == 0)
    {
        samplerStartTime = xTaskGetTickCount();
    }

    while (1)
    {
        samplerIterations++;

        samplerEnabled = true;
        playbackCount = MAX_EVENTS;
        for (int i = 0; i < MAX_EVENTS; i++)
        {
            playbackBuffer[i].timestamp = i * 10;
        }

        uint32_t lastTimestamp = 0;
        for (int i = 0; i < playbackCount; i++)
        {
            uint32_t delayTime = playbackBuffer[i].timestamp - lastTimestamp;
            vTaskDelay(pdMS_TO_TICKS(delayTime));
            lastTimestamp = playbackBuffer[i].timestamp;
            simulateKeyEvent(playbackBuffer[i]);
        }

        vTaskDelayUntil(&xLastWakeTime, loopTicks);

        recordedCount = MAX_EVENTS;
        for (int i = 0; i < MAX_EVENTS; i++)
        {
            recordingBuffer[i].timestamp = MAX_EVENTS - i;
        }

        if (playbackCount + recordedCount <= MAX_EVENTS)
        {
            memcpy(&playbackBuffer[playbackCount], recordingBuffer, recordedCount * sizeof(NoteEvent));
            playbackCount += recordedCount;
        }

        for (int i = 1; i < playbackCount; i++)
        {
            NoteEvent key = playbackBuffer[i];
            int j = i - 1;
            while (j >= 0 && playbackBuffer[j].timestamp > key.timestamp)
            {
                playbackBuffer[j + 1] = playbackBuffer[j];
                j--;
            }
            playbackBuffer[j + 1] = key;
        }
    }
}