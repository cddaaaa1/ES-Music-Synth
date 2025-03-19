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
        if (moduleOctave == 4)
        {
            if (msgType == 'P' && msgOct == 5)
            {
                localCurrentStepSize = stepSizes5[noteIx];
                keys5.set(noteIx, true);
                if (samplerEnabled)
                {
                    sampler_recordEvent('P', 5, (uint8_t)noteIx);
                }
            }
            else if (msgType == 'R' && msgOct == 5)
            {
                localCurrentStepSize = 0;
                keys5.set(noteIx, false);
                if (samplerEnabled)
                {
                    sampler_recordEvent('R', 5, (uint8_t)noteIx);
                }
            }

            if (msgType == 'P' && msgOct == 6)
            {
                localCurrentStepSize = stepSizes6[noteIx];
                keys6.set(noteIx, true);
                if (samplerEnabled)
                {
                    sampler_recordEvent('P', 6, (uint8_t)noteIx);
                }
            }
            else if (msgType == 'R' && msgOct == 6)
            {
                localCurrentStepSize = 0;
                keys6.set(noteIx, false);
                if (samplerEnabled)
                {
                    sampler_recordEvent('R', 6, (uint8_t)noteIx);
                }
            }
            //__atomic_store_n(&currentStepSize, localCurrentStepSize, __ATOMIC_RELAXED);
        }
        // if (msgType == 'H')
        // {
        //     autoDetectHandshake();
        // }
        // Serial.println(msgType);
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
