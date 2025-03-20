#include "globals.h"
#include "can.h"
#include "key.h"
#include "display.h"
#include "pins.h"
#include "isr.h"
#include "sampler.h"
#include "autodetection.h"

// uncomment below to enter testmode

// #define TestMode
// #define STATSTASK
// #define SCAN_KEYS
//  #define DISPLAYTEST
//   #define DECODE
//   #define SAMPLER
//   #define CAN_TX
//   #define CAN_RX_TX
//   #define SAMPLE_ISR

void printTaskStats()
{
  char stats[512];
  vTaskGetRunTimeStats(stats);
  Serial.println(stats);
}

void statsTask(void *pvParameters)
{
  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Print runtime stats
    char stats[512];
    vTaskGetRunTimeStats(stats);
    Serial.println(stats);

    TickType_t currentTime = xTaskGetTickCount();
    if (scanKeysIterations > 0)
    {
      float avgExecutionTime = (float)(currentTime - scanKeysStartTime) / scanKeysIterations;
      Serial.print("scanKeysTask Average Execution Time: ");
      Serial.print(avgExecutionTime);
      Serial.println(" ticks");
    }
    if (displayIterations > 0)
    {
      float avgExecutionTime = (float)(currentTime - displayStartTime) / displayIterations;
      Serial.print("displayUpdateTask Average Execution Time: ");
      Serial.print(avgExecutionTime);
      Serial.println(" ticks");
    }
    if (metronomeIterations > 0)
    {
      float avgExecutionTime = (float)(currentTime - metronomeStartTime) / metronomeIterations;
      Serial.print("metronomeTask Average Execution Time: ");
      Serial.print(avgExecutionTime);
      Serial.println(" ticks");
    }
    if (samplerIterations > 0)
    {
      float avgExecutionTime = (float)(currentTime - samplerStartTime) / samplerIterations;
      Serial.print("samplerTask Average Execution Time: ");
      Serial.print(avgExecutionTime);
      Serial.println(" ticks");
    }
    if (decodeIterations > 0)
    {
      float avgExecutionTime = (float)(currentTime - decodeStartTime) / decodeIterations;
      Serial.print("decodeTask Average Execution Time: ");
      Serial.print(avgExecutionTime);
      Serial.println(" ticks");
    }
    if (CAN_TX_Iterations > 0)
    {
      float avgExecutionTime = (float)(currentTime - CAN_TX_StartTime) / CAN_TX_Iterations;
      Serial.print("CAN_TX_Task Average Execution Time: ");
      Serial.print(avgExecutionTime);
      Serial.println(" ticks");
    }
  }
}

void setup()
{
  pinMode(D3, OUTPUT);
  pinMode(D6, OUTPUT);
  pinMode(D12, OUTPUT);
  pinMode(A5, OUTPUT);
  pinMode(D11, OUTPUT);
  pinMode(A4, OUTPUT);
  pinMode(A3, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(A2, INPUT);
  pinMode(D9, INPUT);
  pinMode(A6, INPUT);
  pinMode(D1, INPUT);
  pinMode(JOYX_PIN, INPUT);
  pinMode(JOYY_PIN, INPUT);

  setOutMuxBit(DRST_BIT, LOW);
  delayMicroseconds(2);
  setOutMuxBit(DRST_BIT, HIGH);
  u8g2.begin();
  setOutMuxBit(DEN_BIT, HIGH);

  Serial.begin(9600);
  Serial.println("Hello World");

#ifndef TestMode
  sampleTimer.setOverflow(fs, HERTZ_FORMAT);
  sampleTimer.attachInterrupt(sampleISR);
  sampleTimer.resume();

  autoDetectHandshake();
  Serial.print("Detected module octave: ");
  Serial.println(moduleOctave);

  xTaskCreate(scanKeysTask, "scanKeys", 256, NULL, 6, &scanKeysHandle);
  xTaskCreate(displayUpdateTask, "displayUpdate", 256, NULL, 7, &displayTaskHandle);

  // Mutex
  localKeyMutex = xSemaphoreCreateMutex();
  externalKeyMutex = xSemaphoreCreateMutex();
  sysState.mutex = xSemaphoreCreateMutex();
  sysState.volume = 4;
  sampler_init();

  // CAN Queues
  msgInQ = xQueueCreate(36, 8);
  msgOutQ = xQueueCreate(36, 8);

  // Init CAN
  CAN_Init(false);
  setCANFilter(0x123, 0x7ff);
  CAN_RegisterRX_ISR(CAN_RX_ISR);
  CAN_RegisterTX_ISR(CAN_TX_ISR);
  CAN_Start();

  // Create decode & transmit tasks
  xTaskCreate(decodeTask, "decodeTask", 128, NULL, 5, NULL);
  xTaskCreate(CAN_TX_Task, "CAN_TX_Task", 128, NULL, 4, NULL);

  if (moduleOctave == 4)
  {
    sampler_init();
    xTaskCreate(samplerTask, "samplerTask", 256, NULL, 3, NULL);
    xTaskCreate(metronomeTask, "metronomeTask", 128, NULL, 2, NULL);
  }

  // Counting semaphore for CAN TX
  CAN_TX_Semaphore = xSemaphoreCreateCounting(3, 3);

  vTaskStartScheduler();

#else
  // // Test mode
  // noInterrupts();

  autoDetectHandshake();
  Serial.print("Detected module octave: ");
  Serial.println(moduleOctave);
#ifdef STATSTASK
  xTaskCreate(statsTask, "StatsTask", 256, NULL, 1, NULL);
#endif
#ifdef SCAN_KEYS
  xTaskCreate(scanKeysFunction, "scanKeysTest", 256, NULL, 1, NULL);
#endif

#ifdef DISPLAYTEST
  xTaskCreate(displayUpdateFunction, "displayTest", 256, NULL, 2, NULL);
#endif

  // Mutex for sysState
  sysState.mutex = xSemaphoreCreateMutex();
  sysState.volume = 4;
  sampler_init();

  // // CAN Queues
  msgInQ = xQueueCreate(384, 8);
  msgOutQ = xQueueCreate(384, 8);

// Init CAN
#ifdef CAN_RX_TX
  CAN_Init(false);
  setCANFilter(0x123, 0x7ff);
  CAN_RegisterRX_ISR(CAN_RX_ISRTest);
  CAN_RegisterTX_ISR(CAN_TX_ISR);
  CAN_Start();
#endif

// Create decode & transmit tasks
#ifdef DECODE
  xTaskCreate(decodeFunction, "decodeTest", 128, NULL, 3, NULL);
  uint8_t testMsg[8] = {'P', 5, 3, 0, 0, 0, 0, 0};
  for (int i = 0; i < 384; i++)
  {
    xQueueSend(msgInQ, testMsg, portMAX_DELAY);
  }
#endif

#ifdef CAN_TX
  xTaskCreate(CAN_TX_Function, "CAN_TX_Test", 128, NULL, 2, NULL);
  uint8_t testMsg[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};

  for (int i = 0; i < 384; i++)
  {
    xQueueSend(msgOutQ, testMsg, portMAX_DELAY);
  }
#endif

  if (moduleOctave == 4)
  {
    sampler_init();
#ifdef SAMPLER
    xTaskCreate(samplerFunction, "samplerTest", 256, NULL, 5, NULL);
#endif

    // #ifdef METRONOME
    //     xTaskCreate(metronomeFunction, "metronomeTest", 128, NULL, 2, NULL);
    // #endif
  }

#ifdef SAMPLE_ISR
  sampleTimer.setOverflow(fs, HERTZ_FORMAT);
  sampleTimer.attachInterrupt(sampleISRTest);
  sampleTimer.resume();
#endif

  CAN_TX_Semaphore = xSemaphoreCreateCounting(3, 3);

  vTaskStartScheduler();
#endif
}

void loop()
{
  // Not used
}