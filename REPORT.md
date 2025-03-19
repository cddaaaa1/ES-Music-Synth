<!-- REPORT.md -->

# Music Synthesizer
Our project realised the basic functions of keyboard, 

## Table of Contents

- [1. Tasks descriptions](#1-task-descriptions)
- [2. ISR descriptions](#2-isr-descriptions)
- [3. Function descriptions](#3-funtion-descriptions)
- [4. Execution timing analysis](#4-execution-timing-analysis)
- [5. Shared data structure and safely access strategy](#5-shared-data-structure-with-safely-access-strategy)
- [6. Analysis of deadlock](#6-analysis-of-deadlock)


## 1. Task descriptions
1. **Task 1: `scanKeysTask`**  
   - **Priority**: *1*  
   - **Purpose**  
     This FreeRTOS task continuously scans a 7×4 keypad matrix to detect key presses and releases, updates the system’s global state, and communicates any key events through a message queue. It also handles handshake signals and knob inputs, ensuring real-time updates to the module’s operating parameters (e.g., volume, sampling mode, current step size).

   - **Key Operations**  
     1. **Matrix Scanning**: Iterates over 7 rows × 4 columns, toggling row-select pins and reading columns to detect pressed keys.  
     2. **Event Handling**: Sends “pressed” or “released” messages to the queue, updates step sizes, etc.  
     3. **Handshake Detection**: Calls `autoDetectHandshake()` if handshake lines change, and broadcasts a handshake message.  
     4. **Knob Input Updates**: Decodes knob states to adjust volume or enable sampling mode.  
     5. **Global State Synchronization**: Uses a mutex to safely update `sysState`.

   - **Concurrency and Real-Time Considerations**  
     - Typically runs with a 50 ms periodic delay.  
     - Synchronizes with other tasks via FreeRTOS queue (`xQueueSend`) and a mutex.

   - **Parameters**  
     - `void *pvParameters`: Standard FreeRTOS task parameter pointer (unused directly).

2. **Task: `displayUpdateTask()`**
    - **Priority**: *2*  
    - **Purpose**  
    Responsible for periodically updating the system’s display (via the `u8g2` library). It retrieves current global state (pressed keys, volume, sampler status) under mutual exclusion, and renders relevant information (e.g., pressed note names, volume level) on the screen.

    - **Key Operations**   
    2. **Global State Synchronization**:  
        - Acquires a mutex (`sysState.mutex`) to safely copy shared state (`sysState.inputs`, `sysState.volume`, etc.) into local variables.  
        - Releases the mutex after copying to minimize lock duration.  
    3. **Display Rendering**:  
        - Renders different text based on `moduleOctave` (4, 5, or 6).  
        - Shows additional details (like note names) only for octave 4.

    - **Concurrency and Real-Time Considerations**  
    - Runs in its own FreeRTOS task context, allowing the system to update the display independently of other tasks (e.g., input scanning or audio generation).  
    - Uses a semaphore (`xSemaphoreTake()`/`xSemaphoreGive()`) to avoid race conditions when accessing shared data.  
    - A fixed 100 ms loop ensures a stable, user-friendly refresh rate without overburdening the CPU.


3. **Task: `CAN_TX_Task()`**
    - **Priority**: *4*  
    - **Purpose**  
    Handles outgoing CAN messages by retrieving data from `msgOutQ` and invoking `CAN_TX` to transmit the message with the specified identifier (here, `0x123`).

    - **Key Operations**  
    1. **Queue Reception**:  
        - Blocks on `xQueueReceive(msgOutQ, msgOut, portMAX_DELAY)` until a new message is available.  
        - Copies the received message into a local buffer `msgOut`.
    2. **Semaphore for CAN Transmission**:  
        - Acquires `CAN_TX_Semaphore` (`xSemaphoreTake(CAN_TX_Semaphore, portMAX_DELAY)`) to ensure exclusive access to the CAN transmit interface.  
        - Calls `CAN_TX(0x123, msgOut)` to send the message once the semaphore is obtained.
    3. **Loop**:  
        - Repeats indefinitely, waiting for new messages and then transmitting them.

    - **Concurrency and Real-Time Considerations**  
    - The use of a queue (`msgOutQ`) decouples CAN transmission from other tasks, allowing messages to be queued without blocking.  
    - `CAN_TX_Semaphore` ensures thread-safe access to the CAN hardware or driver, preventing multiple tasks from transmitting simultaneously.


4. **Task: `decodeTask()`**
    - **Priority**: *3*  
    - **Implementation**  
    Waits for incoming messages on a queue (`msgInQ`) and updates the system’s global state based on the content of each message.

    - **Purpose**  
    Primarily handles inter-module communication by receiving key press/release events from remote keyboards (octave 5 or octave 6). It then updates the local data structures (`keys5` or `keys6`) to reflect these remote key states, and optionally logs sampler events if sampling is enabled.

    - **Key Operations**  
    1. **Message Reception**:  
        - Uses `xQueueReceive(msgInQ, local_RX_Message, portMAX_DELAY)` to block until a new message arrives.  
        - Copies the received message into a global buffer `RX_Message` under mutex protection, allowing other tasks (e.g., display tasks) to access it safely.
    2. **Message Parsing**:  
        - Extracts `msgType` (press `'P'` or release `'R'`), `msgOct` (4, 5, or 6), and `noteIx` from the received byte array.  
        - If the local module is octave 4 (`moduleOctave == 4`), interprets messages for octave 5 or 6, setting or clearing the corresponding bits in `keys5` or `keys6`.
    3. **Sampler Integration**:  
        - If `samplerEnabled` is `true`, records the incoming event (`'P'` or `'R'`) and note index via `sampler_recordEvent()` for future playback or logging.

    - **Concurrency and Real-Time Considerations**  
    - Runs in its own FreeRTOS task context, waiting on the message queue with `portMAX_DELAY`. This design ensures the task is only active when there are new incoming messages.  
    - Mutex protection guarantees that updates to the shared `RX_Message` buffer do not conflict with other tasks reading or writing the same data.  
    - Atomic operations are used if step sizes or other shared variables need to be updated.


    
5. **Task: `samplerTask(void *pvParameters)`**
    - **Priority**: *5*  
    - **Purpose**  
    Runs a continuous loop that:
    1. Records new events (if `samplerEnabled`),  
    2. Plays back events from the previous cycle,  
    3. Merges newly recorded events into the playback buffer,  
    4. Sorts them by timestamp,  
    5. Repeats at the next loop interval.

    - **Key Operations**  
    1. **Loop Timing**:  
        - Uses `vTaskDelayUntil(&xLastWakeTime, loopTicks)` to enforce a fixed sampler loop length (`samplerLoopLength`).  
    2. **Playback Phase**:  
        - Replays the events in `playbackBuffer` by simulating key presses/releases in chronological order.  
    3. **Buffer Swap**:  
        - After playback, locks `samplerMutex` to append newly recorded events from `recordingBuffer` into `playbackBuffer`.  
        - Resets `recordedCount` to zero, then sorts `playbackBuffer` by timestamp.  
    4. **Sampler Enable/Disable**:  
        - If `samplerEnabled` becomes `false`, calls `resetSamplerState()` and delays briefly to free CPU resources.  
        - Resumes normal operation when `samplerEnabled` returns to `true`.

    - **Concurrency and Real-Time Considerations**  
    - Uses a mutex to protect shared buffers.  
    - Employs blocking delays (`vTaskDelay`).  
    - Sorting the events is a potential source of additional CPU usage, but the maximum number of events (`MAX_EVENTS`) is limited to keep it manageable.


6. **Task: `metronomeTask(void *pvParameters)`**
    - **Priority**: *6*  

    - **Purpose**  
    Provides a simple metronome function by blinking the built-in LED and activating a click sound in `sampleISR()` at each beat interval.

    - **Key Operations**  
    1. **Metronome Click**:  
        - Sets `metronomeActive = true` and initializes `metronomeCounter = TICK_DURATION_SAMPLES`.  
        - `sampleISR()` then mixes this click into the audio output.

    - **Concurrency and Real-Time Considerations**  
    - Uses blocking delays (`vTaskDelay`) but does not hold any mutexes or interact with queues, minimizing deadblock.

  

## 2. ISR descriptions
1. **ISR: `sampleISR()`**
    - **Purpose**  
    Generates the output audio sample by updating multiple phase accumulators, summing their high-order bits, applying volume control, and optionally adding a metronome click. Finally, it writes the result to a DAC (or PWM output) for audio playback.

    - **Key Operations**  
    1. **Phase Accumulation**:  
        - Increments each voice’s phase accumulator (`phaseAcc1` through `phaseAcc5`) by the respective `currentStepSize`.  
        - Extracts the high-order byte (`>> 24`) from each accumulator to determine the current sample for that voice.  
    2. **Metronome Click**:  
        - If `metronomeActive`, adds a constant amplitude (`TICK_AMPLITUDE`) to the output and decrements a counter (`metronomeCounter`) until the click is deactivated.  

    - **Concurrency and Real-Time Considerations**  
    - As an ISR, it must execute quickly and avoid blocking calls (no FreeRTOS APIs that may block).  
    - Manipulates global variables (`phaseAccX`, `metronomeCounter`, etc.) directly; ensure these are only updated in ISRs or protected accordingly in tasks.  
    - The short execution time is crucial for maintaining the required audio sample rate.

   
2. **ISR: `CAN_TX_ISR()`**
    - **Purpose**  
    Signals to the system that it can safely proceed with CAN transmissions by giving a semaphore to any task waiting to send additional messages.

    - **Key Operations**  
    1. **Semaphore Release**:  
        - Calls `xSemaphoreGiveFromISR(CAN_TX_Semaphore, NULL)` to unblock the CAN transmit task.  
    2. **No Other Processing**:  
        - Keeps the ISR minimal by giving decode process to other tasks.

3. **ISR: `CAN_RX_ISR()`**
    - **Purpose**  
    Retrieves the incoming CAN message from the hardware buffer and forwards it to the appropriate FreeRTOS queue (`msgInQ`), ensuring that higher-level tasks can process the data.

    - **Key Operations**  
    1. **Read CAN Message**:  
        - Calls `CAN_RX(ID, RX_Message_ISR)` to retrieve the incoming message and its identifier.  
    2. **Queue Transmission**:  
        - Uses `xQueueSendFromISR(msgInQ, RX_Message_ISR, NULL)` to enqueue the message for processing by a FreeRTOS task.

## 3. Function descriptions
1. **Function: `setStepSizes()`**
    - **Purpose**  
    Collects up to five pressed notes from three keyboards (C4, C5, and C6). For each note, it retrieves the corresponding step size and then atomically updates the global variables (`currentStepSize1` through `currentStepSize5`). 

    - **Concurrency and Real-Time Considerations**  
    - Relies on atomic operations to avoid race conditions when updating shared variables.  
    - Does not use any FreeRTOS mechanisms, so it can be invoked at any time without blocking.  
    - Collecting only up to five pressed notes.


2. **Function: `autoDetectHandshake()`**
    - **Purpose**  
    Automatically detects whether the current module should be configured as octave 4, 5, or 6 by setting a handshake output pin high, allowing signals to settle, then reading handshake inputs from adjacent modules (west and east).

    - **Key Operations**  
    1. **Set Handshake Output**:  
        - Forces the handshake output pin (`OUT_PIN`) to `HIGH`.  
    2. **Read Adjacent States**:  
        - Calls `readHandshake(west, east)` to retrieve the logic levels on the west and east handshake inputs.  
    3. **Determine Octave**:  
        - If only the west input is `HIGH`, set `moduleOctave = 4`.  
        - If only the east input is `HIGH`, set `moduleOctave = 6`.  
        - Otherwise, assume `moduleOctave = 5`.

    - **Concurrency and Real-Time Considerations**  
    - Uses a blocking delay of 50 ms (`delay(50)`), may affect real-time responsiveness if called from time-critical contexts.  
    
3. **Function: `readHandshake(bool &west, bool &east)`**
    - **Purpose**  
    Provides a low-level mechanism to read two separate handshake signals (west and east). It uses row-selection pins (`RA0_PIN`, `RA1_PIN`, `RA2_PIN`) and enables the row-enable pin (`REN_PIN`) to latch the handshake signals, then reads the column pin (`C3_PIN`).
    - **Concurrency and Real-Time Considerations**  
    - Involves microsecond-scale delays (`delayMicroseconds(3)`) which should be relatively small.  

1. **Function: `simulateKeyEvent(const NoteEvent &event)`**
    - **Purpose**  
    Simulates a key press (`'P'`) or release (`'R'`) on the specified octave (4, 5, or 6), allowing the system to respond as if a real key event had occurred.

    - **Concurrency and Real-Time Considerations**  
    - Modifies shared bitsets (`keysX`). If other tasks or ISRs access these bitsets!!!!!!!!!


3. **Function: `sampler_recordEvent(char type, uint8_t octave, uint8_t noteIndex)`**

    - **Purpose**  
    Captures user (or remote) key events, timestamps them, and stores them in a circular or linear buffer (`recordingBuffer`) so they can be replayed in the next sampler loop cycle.

    - **Key Operations**  
    2. **Timestamp Calculation**:  
        - Uses `xTaskGetTickCount()` to compute the event’s timestamp relative to the current sampler loop start (`samplerLoopStartTime`).  
    3. **Mutex-Protected Buffer Write**:  
        - Takes `samplerMutex` before writing to `recordingBuffer`.  
        - Increments `recordedCount` if the buffer is not full.  
        - Releases `samplerMutex` after writing.

    - **Concurrency and Real-Time Considerations**  
    - Protected by `samplerMutex` to avoid race conditions when multiple tasks.
    - Uses a small blocking delay (`vTaskDelay`) if the sampler is disabled, which could impact real-time performance!!!!!!

4. **Function: `releaseAllNotes()`**
    - **Purpose**  
    Ensures that no keys remain “stuck” in a pressed state, particularly useful when resetting the system or clearing the sampler state.
    
## 4. Execution timing analysis
1. **Worst case execution time summery**
    | Task name    | theoratical minimum trigger time |Worst case execution time | Testing method   |
    |----------------|-----------------------|------------------|------------------|
    | scanKeysTask   |      | 350 µs            | Force worst case |
    | displayUpdateTask |      | 19143 µs       | Force worst case               |
    | decodeTask       |      | 69300 µs            | …                |
    | CN_TX_Task      |       | 41444000 µs            | …                |
    | samplerTask      |       | 531300 µs            | …                |
    | CAN_RX_ISR   |       | 531300 µs            | …                |
    | CAN_TX_ISR   |       | 531300 µs            | …                |
    | sample_ISR     |       | 531300 µs            | …                |


**Froce worst case explaination**
By controlling variable, force code trigger all logic 

## 5. Shared data structure with safely access strategy

## 6. Analysis of Deadlock

Our design has refined to maximally avoid the possibility of deadlock by considering the following points.  

1. **ISR can't be locked by mutex:**  
   A thread can be blocked if it does not own a mutex, but ISR can't be blocked.  
   So if an ISR is triggered and has no mutex, the program will be deadlocked in ISR.  

2. **Single mutex shared through all threads:**  
   Only one mutex is owned and released at a time, so no "mutual blocking" case happens.  

3. **Priority Inversion:**  
   This happens when a mutex is owned by a low-priority task, and a high-priority task is waiting for it.  
   However, a moderate-priority task that does not rely on that mutex occupies the CPU.  
   In this case, the high-priority task will be blocked.  
   Our code has been refined to **maximally reduce mutex hold time** by properly using `localvariable` to hold the data, significantly reducing the impact of priority inversion.
