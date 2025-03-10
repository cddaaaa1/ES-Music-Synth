#include "globals.h" // For all our shared globals
#include "can.h"     // For CAN tasks and ISRs
#include "key.h"     // for scanKeysTask(...)
#include "display.h" // For scanKeysTask, displayUpdateTask
#include "pins.h"
#include "isr.h"
#include "sampler.h"

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

  // Audio Timer for 22 kHz
  sampleTimer.setOverflow(fs, HERTZ_FORMAT);
  sampleTimer.attachInterrupt(sampleISR);
  sampleTimer.resume();

  // Create RTOS tasks
  xTaskCreate(scanKeysTask, "scanKeys", 256, NULL, 1, &scanKeysHandle);
  xTaskCreate(displayUpdateTask, "displayUpdate", 256, NULL, 2, &displayTaskHandle);

  // Mutex for sysState
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
  xTaskCreate(decodeTask, "decodeTask", 128, NULL, 3, NULL);
  xTaskCreate(CAN_TX_Task, "CAN_TX_Task", 128, NULL, 4, NULL);

  if (OCTAVE == 4)
  {
    sampler_init();
    xTaskCreate(samplerTask, "samplerTask", 256, NULL, 5, NULL);
    xTaskCreate(metronomeTask, "metronomeTask", 128, NULL, 6, NULL);
  }

  // Counting semaphore for CAN TX
  CAN_TX_Semaphore = xSemaphoreCreateCounting(3, 3);

  // Start FreeRTOS
  vTaskStartScheduler();
}

void loop()
{
  // Not used; tasks run under FreeRTOS
}