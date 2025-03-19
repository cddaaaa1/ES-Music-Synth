#include "globals.h" // For all our shared globals
#include "can.h"     // For CAN tasks and ISRs
#include "key.h"     // for scanKeysTask(...)
#include "display.h" // For scanKeysTask, displayUpdateTask
#include "pins.h"
#include "isr.h"
#include "sampler.h"
#include "autodetection.h"

// #define TestMode
// #define SCAN_KEYS
// #define STATSTASK
//  #define DECODE
//  #define METRONOME
//  #define SAMPLER
//  #define CAN_TX
//  #define SAMPLE_ISR

void printTaskStats()
{
  char stats[512]; // 存储任务执行时间统计数据
  vTaskGetRunTimeStats(stats);
  Serial.println(stats); // 通过串口输出结果
}

void statsTask(void *pvParameters)
{
  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(5000)); // Run every 5 seconds

    // Print runtime stats
    char stats[512];
    vTaskGetRunTimeStats(stats);
    Serial.println(stats);

    // Calculate average execution time for scanKeysTask
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
  // ----------------- Pin Setup -----------------
  pinMode(D3, OUTPUT);
  pinMode(D6, OUTPUT);
  pinMode(D12, OUTPUT);
  pinMode(A5, OUTPUT); // REN
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

  // Display setup
  setOutMuxBit(DRST_BIT, LOW);
  delayMicroseconds(2);
  setOutMuxBit(DRST_BIT, HIGH);
  u8g2.begin();
  setOutMuxBit(DEN_BIT, HIGH);

  Serial.begin(9600);
  Serial.println("Hello World");

#ifndef TestMode
  // Audio Timer for 22 kHz
  sampleTimer.setOverflow(fs, HERTZ_FORMAT);
  sampleTimer.attachInterrupt(sampleISR);
  sampleTimer.resume();

  // handshake to determine moduleOctave
  autoDetectHandshake();
  Serial.print("Detected module octave: ");
  Serial.println(moduleOctave);

  // Create RTOS tasks
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
    // xTaskCreate(metronomeTask, "metronomeTask", 128, NULL, 2, NULL);
  }

  // Counting semaphore for CAN TX
  CAN_TX_Semaphore = xSemaphoreCreateCounting(3, 3);

  // Start FreeRTOS
  vTaskStartScheduler();

#else
  // // Test mode
  // noInterrupts();

  // handshake to determine moduleOctave
  autoDetectHandshake();
  Serial.print("Detected module octave: ");
  Serial.println(moduleOctave);
#ifdef STATSTASK
  // Create RTOS tasks
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
  CAN_Init(false);
  setCANFilter(0x123, 0x7ff);
  CAN_RegisterRX_ISR(CAN_RX_ISRTest);
  CAN_RegisterTX_ISR(CAN_TX_ISR);
  CAN_Start();

// Create decode & transmit tasks
#ifdef DECODE
  xTaskCreate(decodeFunction, "decodeTest", 128, NULL, 3, NULL);
  // **预填充队列**
  uint8_t testMsg[8] = {'P', 5, 3, 0, 0, 0, 0, 0};
  for (int i = 0; i < 384; i++)
  {
    xQueueSend(msgInQ, testMsg, portMAX_DELAY);
  }
#endif

#ifdef CAN_TX
  xTaskCreate(CAN_TX_Function, "CAN_TX_Test", 128, NULL, 2, NULL);
  // **预填充队列**
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

#ifdef METRONOME
    xTaskCreate(metronomeFunction, "metronomeTest", 128, NULL, 2, NULL);
#endif
  }

#ifdef SAMPLE_ISR
  // interrupts();
  sampleTimer.setOverflow(fs, HERTZ_FORMAT);
  sampleTimer.attachInterrupt(sampleISRTest);
  sampleTimer.resume();
#endif

  // Counting semaphore for CAN TX
  CAN_TX_Semaphore = xSemaphoreCreateCounting(3, 3);

  // Start FreeRTOS
  vTaskStartScheduler();
#endif
}

void loop()
{
  // Not used; tasks run under FreeRTOS
}