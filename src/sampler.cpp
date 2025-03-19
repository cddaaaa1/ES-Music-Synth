#include "sampler.h"
#include <STM32FreeRTOS.h>
#include <Arduino.h>
#include <string.h>
#include "globals.h"

uint32_t samplerIterations = 0;
TickType_t samplerStartTime = 0;

uint32_t metronomeIterations = 0;
TickType_t metronomeStartTime = 0;

//------------------------------------------------------------------------------
// Metronome & Sampler Timing Constants
//------------------------------------------------------------------------------
// Define BPM and beats per bar for the metronome:
const uint32_t BPM = 100;       // Beats per minute
const uint32_t beatsPerBar = 4; // For a 4/4 time signature
// Compute samplerLoopLength based on BPM (in milliseconds):
const uint32_t samplerLoopLength = (60000UL / BPM) * beatsPerBar; // 2400 ms at 100 BPM

//------------------------------------------------------------------------------
// Maximum number of events that can be recorded per loop
//------------------------------------------------------------------------------
const int MAX_EVENTS = 128;

//------------------------------------------------------------------------------
// Buffers for recording and playback of note events
//------------------------------------------------------------------------------
static NoteEvent recordingBuffer[MAX_EVENTS];
static int recordedCount = 0;
static NoteEvent playbackBuffer[MAX_EVENTS];
static int playbackCount = 0;

//------------------------------------------------------------------------------
// Mutex to protect sampler buffers
//------------------------------------------------------------------------------
static SemaphoreHandle_t samplerMutex = NULL;

//------------------------------------------------------------------------------
// Helper Function: simulateKeyEvent
//------------------------------------------------------------------------------
// This function simulates a key event during playback by updating the external
// state (keys4 and currentStepSize) to mimic a key press or release.
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
//------------------------------------------------------------------------------
// Function: sampler_init
//------------------------------------------------------------------------------
void sampler_init()
{
    samplerMutex = xSemaphoreCreateMutex();
}

//------------------------------------------------------------------------------
// Function: sampler_recordEvent
//------------------------------------------------------------------------------
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
        // Option 1: Simply delay a short period to yield CPU
        vTaskDelay(pdMS_TO_TICKS(50));
        return;
    }

    // Calculate timestamp relative to the start of the current loop
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
    // For octave 4:
    if (xSemaphoreTake(localKeyMutex, portMAX_DELAY) == pdTRUE)
    {
        for (size_t i = 0; i < keys4.size(); i++)
        {
            if (keys4.test(i))
            {
                NoteEvent event = {0, 'R', 4, static_cast<uint8_t>(i)};
                simulateKeyEvent(event);
            }
        }
        xSemaphoreGive(localKeyMutex);
    }
    // For octave 5:
    if (xSemaphoreTake(externalKeyMutex, portMAX_DELAY) == pdTRUE)
    {
        for (size_t i = 0; i < keys5.size(); i++)
        {
            if (keys5.test(i))
            {
                NoteEvent event = {0, 'R', 5, static_cast<uint8_t>(i)};
                simulateKeyEvent(event);
            }
        }
        xSemaphoreGive(externalKeyMutex);
    }
    // For octave 6:
    if (xSemaphoreTake(externalKeyMutex, portMAX_DELAY) == pdTRUE)
    {
        for (size_t i = 0; i < keys6.size(); i++)
        {
            if (keys6.test(i))
            {
                NoteEvent event = {0, 'R', 6, static_cast<uint8_t>(i)};
                simulateKeyEvent(event);
            }
        }
        xSemaphoreGive(externalKeyMutex);
    }
}

void resetSamplerState()
{
    // Reset buffers and counters.
    releaseAllNotes();
    xSemaphoreTake(samplerMutex, portMAX_DELAY);
    recordedCount = 0;
    playbackCount = 0;
    xSemaphoreGive(samplerMutex);

    // Reset the loop start time if needed.
    samplerLoopStartTime = 0;
}

//------------------------------------------------------------------------------
// Function: samplerTask
//------------------------------------------------------------------------------
void samplerTask(void *pvParameters)
{

    // Convert loop length to ticks
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
            // When the flag goes from 1 to 0, reset the state.
            resetSamplerState();
        }
        prevSamplerEnabled = sampler_enabled;
        if (!sampler_enabled)
        {
            // Delay to yield CPU.
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        // Mark the beginning of the new loop cycle
        samplerLoopStartTime = xTaskGetTickCount();

        // ----- Playback Phase -----
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

        // Wait until the end of the loop cycle
        vTaskDelayUntil(&xLastWakeTime, loopTicks);

        // ----- Buffer Swap -----
        // Update playbackBuffer only if new events have been recorded.
        xSemaphoreTake(samplerMutex, portMAX_DELAY);
        if (recordedCount > 0)
        {
            // Check if there is enough space to append new events.
            if (playbackCount + recordedCount <= MAX_EVENTS)
            {
                memcpy(&playbackBuffer[playbackCount], recordingBuffer, recordedCount * sizeof(NoteEvent));
                playbackCount += recordedCount;
            }
            // Clear the recording buffer.
            recordedCount = 0;

            // --- Sort playbackBuffer by timestamp ---
            // Use a simple insertion sort because MAX_EVENTS is relatively small.
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

//------------------------------------------------------------------------------
// Function: metronomeTask
//------------------------------------------------------------------------------
// This task toggles the built-in LED every beat as a simple metronome.
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

        // Delay until the next precise interval
        vTaskDelayUntil(&xLastWakeTime, beatDelay);
    }
}

void metronomeFunction(void *pvParameters)
{
    // 设定最大 BPM 值，以减少 vTaskDelay()，增加 CPU 占用
    const TickType_t beatDelay = pdMS_TO_TICKS(60000UL / 300); // 300 BPM

    if (metronomeIterations == 0)
    {
        metronomeStartTime = xTaskGetTickCount();
    }

    while (1)
    {
        metronomeIterations++;

        // **最坏情况: 确保 samplerEnabled 始终为 true**
        samplerEnabled = true;

        // **高频 I/O 操作**
        for (int j = 0; j < 10; j++)
        {
            digitalToggle(LED_BUILTIN);
        }

        // **最坏情况: metronomeCounter 执行最大计算**
        metronomeActive = true;
        metronomeCounter = UINT32_MAX; // 设为最大值，模拟计算最重路径

        // **确保任务持续执行，而不是立即进入等待**
        TickType_t startTick = xTaskGetTickCount();
        while (xTaskGetTickCount() - startTick < beatDelay)
        {
            vTaskDelay(pdMS_TO_TICKS(1)); // 让出 CPU，避免独占
        }
    }
}