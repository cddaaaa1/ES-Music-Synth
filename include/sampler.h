#ifndef SAMPLER_H
#define SAMPLER_H

#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include "globals.h" 

struct NoteEvent
{
    uint32_t timestamp; 
    char type;          
    uint8_t octave;
    uint8_t noteIndex;
};

void sampler_init();

void sampler_recordEvent(char type, uint8_t octave, uint8_t noteIndex);

void samplerTask(void *pvParameters);

void metronomeTask(void *pvParameters);
void metronomeFunction(void *pvParameters);
#endif 
