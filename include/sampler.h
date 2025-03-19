#ifndef SAMPLER_H
#define SAMPLER_H

#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include "globals.h" // Make sure this file declares your global variables (e.g. currentStepSize, samplerLoopStartTime, samplerEnabled) and any other shared globals.

//------------------------------------------------------------------------------
// Data Structure
//------------------------------------------------------------------------------
// Structure representing a note event with a timestamp.
struct NoteEvent
{
    uint32_t timestamp; // Time in ms relative to the loop start
    char type;          // 'P' for press, 'R' for release
    uint8_t octave;
    uint8_t noteIndex;
};

//------------------------------------------------------------------------------
// Function Prototypes
//------------------------------------------------------------------------------
// Initialize the sampler module (creates the mutex and enables sampler mode).
void sampler_init();

// Record a note event (press or release) if sampler mode is enabled.
// This function should be called from your key scanning task.
void sampler_recordEvent(char type, uint8_t octave, uint8_t noteIndex);

// FreeRTOS task that handles playback of recorded events in a loop.
// Create this task using xTaskCreate in your setup().
void samplerTask(void *pvParameters);

// FreeRTOS task that functions as a metronome by toggling the built-in LED every beat.
// Create this task using xTaskCreate in your setup().
void metronomeTask(void *pvParameters);
void metronomeFunction(void *pvParameters);
#endif // SAMPLER_H
