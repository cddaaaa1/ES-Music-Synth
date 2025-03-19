# Worst Case Execution Time (WCET) Analysis

This document contains the code implementations of various tasks and their worst-case execution time (WCET) scenarios. Each function is tested under worst-case conditions isolated to determine its maximum execution time.

## scanKeysFunction

The worst-case scenarios for `scanKeysFunction` are simulated as follows:

#### 1. **All keys pressed simultaneously**  
   - The function simulates the scenario where all 32 keys are pressed at the same time.  
   - This forces the system to process the maximum number of key state changes, increasing computational load.  
   - Implementation: `localInputs.set();` ensures that all key inputs are active.

#### 2. **Executing the heaviest computation function (`setStepSizes`)**  
   - The function `setStepSizes()` is called in the worst-case scenario where it must compute new step sizes for all active keys.  
   - This ensures the system handles the maximum processing demand related to musical key mapping and frequency adjustments.

#### 3. **Reading knob state changes under maximum computational load**  
   - The function also tests the worst-case scenario where the rotary knob state changes while the system is already handling the maximum number of keypresses.  
   - This tests the system’s ability to handle concurrent inputs and compute real-time volume adjustments.  
   - Implementation: `knob3.updateRotation(currentKnobState);` ensures that the knob rotation is processed under high load.

#### Function Implementation

```cpp
void scanKeysFunction(void *pvParameters)
{
    const TickType_t xFrequency = 50 / portTICK_PERIOD_MS;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    if (scanKeysIterations == 0)
    {
        scanKeysStartTime = xTaskGetTickCount();
    }
    while (1)
    {
        scanKeysIterations++;
        std::bitset<32> localInputs;
        int lastPressedKey = -1;
        std::bitset<2> currentKnobState;
        int localRotationVariable = 0;
        int localVolume = sysState.volume;
        std::bitset<32> previousInput = sysState.inputs;

        // Simulating worst-case scenario: all keys are pressed
        localInputs.set();

        for (uint8_t row = 0; row < 7; row++)
        {
            digitalWrite(REN_PIN, LOW);
            digitalWrite(RA0_PIN, row & 0x01);
            digitalWrite(RA1_PIN, row & 0x02);
            digitalWrite(RA2_PIN, row & 0x04);
            digitalWrite(REN_PIN, HIGH);
            delayMicroseconds(3);

            std::bitset<4> colInputs;
            colInputs.set();
            digitalWrite(REN_PIN, LOW);

            for (uint8_t col = 0; col < 4; col++)
            {
                int keyIndex = row * 4 + col;
                localInputs[keyIndex] = colInputs[col];

                if (keyIndex <= 11 && keyIndex >= 0)
                {
                    if (previousInput[keyIndex] && !colInputs[col])
                    {
                        lastPressedKey = keyIndex;
                        if (moduleOctave == 4)
                        {
                            keys4.set(keyIndex, true);
                            __atomic_store_n(&currentStepSize, stepSizes4[lastPressedKey], __ATOMIC_RELAXED);
                        }
                    }
                    if (!previousInput[keyIndex] && colInputs[col])
                    {
                        if (moduleOctave == 4)
                        {
                            keys4.set(keyIndex, false);
                            __atomic_store_n(&currentStepSize, 0, __ATOMIC_RELAXED);
                        }
                    }
                }
            }
        }

        setStepSizes();
        currentKnobState.set();
        knob3.updateRotation(currentKnobState);
        localVolume = knob3.getRotationValue();
    }
}
```


## displayupdateFunction

The worst-case scenarios for `displayUpdateFunction` are simulated as follows:

#### 1. **Displaying all musical notes simultaneously**
   - The function simulates the scenario where every possible musical note across three octaves (`C4` to `B6`) is displayed.
   - This significantly increases the rendering load, requiring the system to handle multiple calls to `u8g2.print()`.
   - **Implementation:** The function iterates through all 12 notes in `keys4`, `keys5`, and `keys6`, printing each one.

#### 2. **Displaying the maximum possible volume value**
   - The display updates to show the volume level at its highest value (`8`), ensuring that the rendering pipeline processes a full numeric update.
   - **Implementation:** `u8g2.print(8);` ensures that the display processes a large numerical rendering operation.

#### 3. **Frequent updates of the sampler status**
   - The function simulates a scenario where the sampler state changes frequently, forcing the display to update at the highest rate.
   - **Implementation:** `u8g2.print("Sampler Enabled");` ensures that a status message is always present on the screen.

#### 4. **Executing the display buffer update**
   - The worst-case scenario includes sending the full display buffer update after rendering all elements.
   - This represents the highest computational and I/O demand for the function.
   - **Implementation:** `u8g2.sendBuffer();` triggers the transfer of all rendered data to the display hardware.

#### Function Implementation

```cpp
void displayUpdateFunction(void *pvParameters)
{
    const TickType_t xFrequency = 100 / portTICK_PERIOD_MS;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    if (displayIterations == 0)
    {
        displayStartTime = xTaskGetTickCount();
    }

    while (1)
    {
        displayIterations++;

        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);

        u8g2.drawStr(2, 10, "Notes:");

        int cursorx = 40;
        for (int i = 0; i < 12; i++)
        {
            const char *localNoteNames[12] =
                {"C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4", "A#4", "B4"};
            u8g2.setCursor(cursorx, 10);
            u8g2.print(localNoteNames[i]);
            cursorx += 15;

            const char *remoteNoteNames[12] =
                {"C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5", "A5", "A#5", "B5"};
            u8g2.setCursor(cursorx, 10);
            u8g2.print(remoteNoteNames[i]);
            cursorx += 15;

            const char *sixthNoteNames[12] =
                {"C6", "C#6", "D6", "D#6", "E6", "F6", "F#6", "G6", "G#6", "A6", "A#6", "B6"};
            u8g2.setCursor(cursorx, 10);
            u8g2.print(sixthNoteNames[i]);
            cursorx += 15;
        }

        u8g2.setCursor(2, 20);
        u8g2.print("Volume:");
        u8g2.setCursor(50, 20);
        u8g2.print(8); 

        u8g2.setCursor(2, 30);
        u8g2.print("Sampler Enabled");

        u8g2.drawStr(2, 40, "Octave 4");

        u8g2.sendBuffer();
    }
}
```

## decodeFunction

The worst-case scenarios for `decodeFunction` are simulated as follows:

#### 1. **Processing maximum queue input load**  
- The function continuously reads from `msgInQ` which is pre-setted to be on the maximum load, ensuring that every available message is processed without skipping.  
- This simulates high message traffic, forcing the system to handle the worst-case queue processing scenario.

#### 2. **Simulating all keys being pressed**  
- The function assumes every possible key press message (`'P'` type) is received, processing the corresponding note activation.
- Both `keys5` and `keys6` are updated under full load conditions.

#### 3. **Simulating all keys being released**  
- The function also handles the worst-case scenario where all keys (`'R'` type) are released.
- This ensures the system processes the maximum number of key-off events, requiring updates to `keys5` and `keys6` states.

#### 4. **Triggering handshake messages continuously**  
- The function simulates the reception of multiple handshake (`'H'` type) messages, forcing `autoDetectHandshake()` to execute under high-load conditions.

#### Function Implementation

```cpp
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

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
```

## CAN\_TX\_Function

The worst-case scenarios for `CAN_TX_Function` are simulated as follows:

#### 1. **Processing maximum queue input load**

- The function continuously checks `msgOutQ` and processes all available messages.
- This simulates a high transmission rate scenario.

#### 2. **Sending the maximum data payload**

- The function ensures that the largest possible CAN message (8 bytes filled with `0xFF`) is sent every time.
- This increases bus traffic and system load, forcing the function to handle the worst-case data transmission scenario.


#### Function Implementation

```cpp
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
    }
}
```

## samplerFunction

The worst-case scenarios for `samplerFunction` are simulated as follows:

#### 1. **Minimizing loop delay to maximize CPU load**

- The function is set to the shortest loop period (`1ms`), forcing high CPU utilization.

#### 2. **Ensuring continuous sampler activation**

- `samplerEnabled` remains `true` at all times, keeping the sampler always running.

#### 3. **Filling the playback buffer completely**

- The function fills the entire `playbackBuffer` with events to ensure maximum processing load.

#### 4. **Simulating playback of all recorded events**

- The function iterates through all playback events, simulating maximum delay between each step.

#### 5. **Avoiding idle waiting to maximize CPU use**

- The function continuously runs instead of waiting for the next scheduling cycle.

#### 6. **Recording and merging a full buffer of new events**

- The function fills `recordingBuffer` with events in reverse order, forcing worst-case sorting performance.

#### 7. **Sorting with the most computationally expensive path**

- The insertion sort algorithm is used to sort all recorded events, ensuring that the function experiences its worst-case execution time.


#### Function Implementation

```cpp
void samplerFunction(void *pvParameters)
{
    const TickType_t loopTicks = pdMS_TO_TICKS(1);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    bool prevSamplerEnabled = true;

    if (samplerIterations == 0)
    {
        samplerStartTime = xTaskGetTickCount();
    }

    while (1)
    {
        samplerIterations++;

        samplerEnabled = true;
        playbackCount = MAX_EVENTS;
        for (int i = 0; i < MAX_EVENTS; i++)
        {
            playbackBuffer[i].timestamp = i * 10;
        }

        uint32_t lastTimestamp = 0;
        for (int i = 0; i < playbackCount; i++)
        {
            uint32_t delayTime = playbackBuffer[i].timestamp - lastTimestamp;
            vTaskDelay(pdMS_TO_TICKS(delayTime));
            lastTimestamp = playbackBuffer[i].timestamp;
            simulateKeyEvent(playbackBuffer[i]);
        }

        vTaskDelayUntil(&xLastWakeTime, loopTicks);

        recordedCount = MAX_EVENTS;
        for (int i = 0; i < MAX_EVENTS; i++)
        {
            recordingBuffer[i].timestamp = MAX_EVENTS - i;
        }

        if (playbackCount + recordedCount <= MAX_EVENTS)
        {
            memcpy(&playbackBuffer[playbackCount], recordingBuffer, recordedCount * sizeof(NoteEvent));
            playbackCount += recordedCount;
        }

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
}
```


## metronomeFunction

The worst-case scenarios for `metronomeFunction` are simulated as follows:

#### 1. **Maximum BPM to minimize idle time**

- The function is set to 300 BPM, reducing `vTaskDelay()` and maximizing CPU usage.
- This ensures that the function runs at its highest activation frequency.

#### 2. **Ensuring continuous sampler activation**

- The function keeps `samplerEnabled = true` at all times, ensuring continuous workload on the sampler.

#### 3. **High-frequency I/O operations**

- The function performs rapid toggling of `LED_BUILTIN`, increasing CPU and I/O bus activity.

#### 4. **Maximizing computational workload**

- `metronomeCounter` is set to `UINT32_MAX`, forcing the function to handle the heaviest computational burden.

#### 5. **Ensuring task execution over idle waiting**

- Instead of immediately sleeping, the function executes a time-based loop to keep CPU utilization high.


#### Function Implementation

```cpp
void metronomeFunction(void *pvParameters)
{
    const TickType_t beatDelay = pdMS_TO_TICKS(60000UL / 300);

    if (metronomeIterations == 0)
    {
        metronomeStartTime = xTaskGetTickCount();
    }

    while (1)
    {
        metronomeIterations++;

        samplerEnabled = true;

        for (int j = 0; j < 10; j++)
        {
            digitalToggle(LED_BUILTIN);
        }

        metronomeActive = true;
        metronomeCounter = UINT32_MAX;

        TickType_t startTick = xTaskGetTickCount();
        while (xTaskGetTickCount() - startTick < beatDelay)
        {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}
```


## sampleISRTest

The worst-case scenarios for `sampleISRTest`  are simulated as follows, this one is testing using `micros()` instead of `vTaskGetRunTimeStats(stats)`:

### 1. **Executing maximum phase accumulator updates**

- The function updates five phase accumulators simultaneously (`phaseAcc1` to `phaseAcc5`), ensuring that all waveform computations are performed at the same time.

### 2. **Computing the highest possible output voltage**

- The ISR computes the final output voltage (`Vout`) using the maximum number of phase accumulations and shifts.
- Volume scaling is also applied to adjust the amplitude dynamically.

### 3. **Handling metronome activity**

- The function simulates a scenario where the metronome is always active.
- This ensures the additional `TICK_AMPLITUDE` modification and counter decrement is executed in every ISR call.

### 4. **Writing to DAC under real-time constraints**

- The final computed voltage is sent to the DAC using `analogWrite(A3, Vout + 128);`, ensuring that the ISR handles real-time audio processing without missing updates.

### 5. **Measuring worst-case ISR execution time**

- The ISR execution time is measured and recorded to track its worst-case duration.
- A running maximum is maintained to capture the highest ISR execution time.

### 6. **Logging execution time every 1000 ISR calls**

- To monitor timing performance, the ISR prints execution times every 1000 calls, ensuring that worst-case conditions are tracked over extended periods.

#### Function Implementation

```cpp
void sampleISRTest()
{
    uint32_t startTime = micros();

    phaseAcc1 += currentStepSize1;
    phaseAcc2 += currentStepSize2;
    phaseAcc3 += currentStepSize3;
    phaseAcc4 += currentStepSize4;
    phaseAcc5 += currentStepSize5;
    int32_t Vout = ((phaseAcc1 >> 24) + (phaseAcc2 >> 24) + (phaseAcc3 >> 24) + (phaseAcc4 >> 24) + (phaseAcc5 >> 24)) / 5 - 128;

    int volumeLevel = sysState.volume;
    Vout = Vout >> (8 - volumeLevel);

    if (metronomeActive)
    {
        Vout += TICK_AMPLITUDE;
        if (metronomeCounter > 0)
            metronomeCounter--;
        else
            metronomeActive = false;
    }

    analogWrite(A3, Vout + 128);

    uint32_t endTime = micros();
    uint32_t isrDuration = endTime - startTime;

    static volatile uint32_t maxIsrTime = 0;
    if (isrDuration > maxIsrTime)
    {
        maxIsrTime = isrDuration;
    }

    static volatile uint32_t isrCounter = 0;
    if (++isrCounter % 1000 == 0)
    {
        Serial.print("ISR Execution Time: ");
        Serial.print(isrDuration);
        Serial.println(" µs");
    }
}
```


