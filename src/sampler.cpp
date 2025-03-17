#include "sampler.h"
#include <STM32FreeRTOS.h>
#include <Arduino.h>
#include <string.h>
#include "globals.h"

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
        if (event.type == 'P')
        {
            keys4.set(event.noteIndex, true);
        }
        else if (event.type == 'R')
        {
            keys4.set(event.noteIndex, false);
        }
    }
    if (event.octave == 5)
    {
        if (event.type == 'P')
        {
            keys5.set(event.noteIndex, true);
        }
        else if (event.type == 'R')
        {
            keys5.set(event.noteIndex, false);
        }
    }
    if (event.octave == 6)
    {
        if (event.type == 'P')
        {
            keys6.set(event.noteIndex, true);
        }
        else if (event.type == 'R')
        {
            keys6.set(event.noteIndex, false);
        }
    }
}

//------------------------------------------------------------------------------
// Function: sampler_init
//------------------------------------------------------------------------------
void sampler_init()
{
    samplerMutex = xSemaphoreCreateMutex();
    samplerEnabled = true;
}

//------------------------------------------------------------------------------
// Function: sampler_recordEvent
//------------------------------------------------------------------------------
void sampler_recordEvent(char type, uint8_t octave, uint8_t noteIndex)
{
    if (!samplerEnabled)
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

void resetSamplerState()
{
    // Reset buffers and counters.
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
    bool prevSamplerEnabled = samplerEnabled;
    while (1)
    {
        if (prevSamplerEnabled && !samplerEnabled)
        {
            // When the flag goes from 1 to 0, reset the state.
            resetSamplerState();
        }
        prevSamplerEnabled = samplerEnabled;
        if (!samplerEnabled)
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
    // Calculate the delay between beats in ticks (60000 ms / BPM)
    const TickType_t beatDelay = pdMS_TO_TICKS(60000UL / BPM);
    while (1)
    {
        if (!samplerEnabled)
        {
            // Option 1: Simply delay a short period to yield CPU
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        // if (!samplerEnabled)
        //     return;
        // Toggle the LED for a visual metronome cue.
        digitalToggle(LED_BUILTIN);
        // Trigger the tick sound:
        metronomeActive = true;
        metronomeCounter = TICK_DURATION_SAMPLES;
        vTaskDelay(beatDelay);
    }
}
