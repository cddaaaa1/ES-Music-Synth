#include "globals.h"
#include "can.h"
#include "sampler.h"
#include "autodetection.h"

uint32_t decodeIterations = 0;
TickType_t decodeStartTime = 0;

uint32_t CAN_TX_Iterations = 0;
TickType_t CAN_TX_StartTime = 0;

void CAN_TX_ISR(void)
{
    // Give semaphore from ISR
    xSemaphoreGiveFromISR(CAN_TX_Semaphore, NULL);
}

void CAN_RX_ISR(void)
{
    uint8_t RX_Message_ISR[8];
    uint32_t ID = 0x123;
    CAN_RX(ID, RX_Message_ISR);
    xQueueSendFromISR(msgInQ, RX_Message_ISR, NULL);
}

void CAN_RX_ISRTest(void)
{
    uint32_t startTime = micros();

    uint8_t RX_Message_ISR[8];
    uint32_t ID = 0x123;
    CAN_RX(ID, RX_Message_ISR);
    xQueueSendFromISR(msgInQ, RX_Message_ISR, NULL);

    uint32_t endTime = micros();
    uint32_t isrDuration = endTime - startTime; 

    static volatile uint32_t maxIsrTime = 0;
    if (isrDuration > maxIsrTime)
    {
        maxIsrTime = isrDuration;
    }

    Serial.print("CAN_RX_ISR Execution Time: ");
    Serial.print(isrDuration);
    Serial.println(" µs");

    Serial.print("Max CAN_RX_ISR Execution Time: ");
    Serial.print(maxIsrTime);
    Serial.println(" µs");
}

// The decodeTask receives messages from msgInQ
void decodeTask(void *pvParameters)
{
    uint8_t local_RX_Message[8] = {0};
    while (1)
    {
        xQueueReceive(msgInQ, local_RX_Message, portMAX_DELAY);

        char msgType = local_RX_Message[0];   
        uint8_t msgOct = local_RX_Message[1];
        uint8_t noteIx = local_RX_Message[2];

        static uint32_t localCurrentStepSize = 0;
        bool sampler_enabled;
        if (xSemaphoreTake(sysState.mutex, portMAX_DELAY) == pdTRUE)
        {
            sampler_enabled = sysState.knob2.getPress();
            xSemaphoreGive(sysState.mutex);
        }
        if (moduleOctave == 4)
        {
            if (msgType == 'P' && msgOct == 5)
            {
                localCurrentStepSize = stepSizes5[noteIx];
                if (xSemaphoreTake(externalKeyMutex, portMAX_DELAY) == pdTRUE)
                {
                    keys5.set(noteIx, true);
                    xSemaphoreGive(externalKeyMutex);
                }
                if (sampler_enabled)
                {
                    sampler_recordEvent('P', 5, (uint8_t)noteIx);
                }
            }
            else if (msgType == 'R' && msgOct == 5)
            {
                localCurrentStepSize = 0;
                if (xSemaphoreTake(externalKeyMutex, portMAX_DELAY) == pdTRUE)
                {
                    keys5.set(noteIx, false);
                    xSemaphoreGive(externalKeyMutex);
                }
                if (sampler_enabled)
                {
                    sampler_recordEvent('R', 5, (uint8_t)noteIx);
                }
            }

            if (msgType == 'P' && msgOct == 6)
            {
                localCurrentStepSize = stepSizes6[noteIx];

                if (xSemaphoreTake(externalKeyMutex, portMAX_DELAY) == pdTRUE)
                {
                    keys6.set(noteIx, true);
                    xSemaphoreGive(externalKeyMutex);
                }
                if (sampler_enabled)
                {
                    sampler_recordEvent('P', 6, (uint8_t)noteIx);
                }
            }
            else if (msgType == 'R' && msgOct == 6)
            {
                localCurrentStepSize = 0;
                if (xSemaphoreTake(externalKeyMutex, portMAX_DELAY) == pdTRUE)
                {
                    keys6.set(noteIx, false);
                    xSemaphoreGive(externalKeyMutex);
                }
                if (sampler_enabled)
                {
                    sampler_recordEvent('R', 6, (uint8_t)noteIx);
                }
            }
        }
    }
}

void decodeFunction(void *pvParameters)
{
    uint8_t local_RX_Message[8] = {0};

    if (decodeIterations == 0)
    {
        decodeStartTime = xTaskGetTickCount();
    }

    while (1)
    {
        decodeIterations++;

        if (xQueueReceive(msgInQ, local_RX_Message, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            char msgType = local_RX_Message[0];   
            uint8_t msgOct = local_RX_Message[1]; 
            uint8_t noteIx = local_RX_Message[2];

            static uint32_t localCurrentStepSize = 0;

            if (msgType == 'P')
            {
                if (msgOct == 5)
                {
                    localCurrentStepSize = stepSizes5[noteIx];
                    keys5.set(noteIx, true);
                    if (samplerEnabled)
                    {
                        sampler_recordEvent('P', 5, noteIx);
                    }
                }
                else if (msgOct == 6)
                {
                    localCurrentStepSize = stepSizes6[noteIx];
                    keys6.set(noteIx, true);
                    if (samplerEnabled)
                    {
                        sampler_recordEvent('P', 6, noteIx);
                    }
                }
            }
            else if (msgType == 'R') 
            {
                if (msgOct == 5)
                {
                    localCurrentStepSize = 0;
                    keys5.set(noteIx, false);
                    if (samplerEnabled)
                    {
                        sampler_recordEvent('R', 5, noteIx);
                    }
                }
                else if (msgOct == 6)
                {
                    localCurrentStepSize = 0;
                    keys6.set(noteIx, false);
                    if (samplerEnabled)
                    {
                        sampler_recordEvent('R', 6, noteIx);
                    }
                }
            }

            if (msgType == 'H')
            {
                autoDetectHandshake();
            }
        }
        else
        {
        }

        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}

// The CAN TX task pulls messages from msgOutQ and sends them
void CAN_TX_Task(void *pvParameters)
{
    uint8_t msgOut[8];
    while (1)
    {
        xQueueReceive(msgOutQ, msgOut, portMAX_DELAY);
        xSemaphoreTake(CAN_TX_Semaphore, portMAX_DELAY);
        CAN_TX(0x123, msgOut);
    }
}

void CAN_TX_Function(void *pvParameters)
{
    uint8_t msgOut[8];

    if (CAN_TX_Iterations == 0)
    {
        CAN_TX_StartTime = xTaskGetTickCount();
    }

    while (1)
    {
        CAN_TX_Iterations++;

        if (xQueueReceive(msgOutQ, msgOut, pdMS_TO_TICKS(5)) == pdTRUE)
        {
            for (int i = 0; i < 8; i++)
            {
                msgOut[i] = 0xFF;
            }
            CAN_TX(0x123, msgOut);
        }
        else
        {
        }
    }
}