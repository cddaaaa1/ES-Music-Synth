#include "globals.h"
#include "can.h"
#include "sampler.h"
#include "autodetection.h"
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

// The decodeTask receives messages from msgInQ
void decodeTask(void *pvParameters)
{
    uint8_t local_RX_Message[8] = {0};
    while (1)
    {
        xQueueReceive(msgInQ, local_RX_Message, portMAX_DELAY);

        // Copy message into global if you want it for display
        // if (xSemaphoreTake(sysState.mutex, portMAX_DELAY) == pdTRUE)
        // {
        //     memcpy(RX_Message, local_RX_Message, sizeof(RX_Message));
        //     xSemaphoreGive(sysState.mutex);
        // }

        char msgType = local_RX_Message[0];   // 'P'/'R'
        uint8_t msgOct = local_RX_Message[1]; // 4 or 5
        uint8_t noteIx = local_RX_Message[2];

        static uint32_t localCurrentStepSize = 0;
        // If we are the C4 node, handle remote C5 messages
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
            //__atomic_store_n(&currentStepSize, localCurrentStepSize, __ATOMIC_RELAXED);
        }
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
