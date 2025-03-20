# **Task Descriptions**

## **1. scanKeysTask**

### **Priority**: _6_

### **Purpose**

Detects key presses/releases from a **7×4 keypad matrix**, updates global state, and handles knob inputs.

### **Key Operations**

1. **Matrix Scanning**: Reads 7 rows × 4 columns to detect key states.
2. **Event Handling**: Sends key events to the queue and updates step sizes.
3. **Knob Input Updates**: Adjusts volume and enables sampling mode.

### **Concurrency & Real-Time**

- **Task Period**: Runs at **5 ms** intervals using `vTaskDelayUntil(&xLastWakeTime, xFrequency)`. High frequency to ensure quick updates
- **Mutex Usage**:
  - Acquires `localKeyMutex` to set bits in `keys4`.
- **Queue Operations**: Uses `xQueueSend` (for message sending).
- **Atomic Updates**: Uses `__atomic_store_n` to update `currentStepSize` and `sysState.volume`, preventing data corruption in multi-task scenarios.

---

## **2. displayUpdateTask**

### **Priority**: _7_

### **Purpose**

Updates the display with pressed notes, volume, and sampler status.

### **Concurrency & Real-Time**

- **Task Period**: Runs at **100 ms** intervals (`vTaskDelayUntil(...)`), significantly slower than `scanKeysTask`.
- **Mutex Usage**:
  - acquires `localKeyMutex` and `externalKeyMutex` to read `keys4`, `keys5`, and `keys6` bitsets when rendering the display.
- **Overall Real-Time**: concurrency impact is small. It mostly blocks on mutex and `vTaskDelayUntil`. The relatively low update rate mitigates real-time concerns.

---

## **3. CAN_TX_Task**

### **Priority**: _4_

### **Purpose**

Sends outgoing CAN messages from `msgOutQ`.

### **Key Operations**

1. **Queue Reception**: Blocks on `xQueueReceive(msgOutQ, msgOut, portMAX_DELAY)`.
2. **Semaphore for CAN TX**: Uses `xSemaphoreTake(CAN_TX_Semaphore, portMAX_DELAY)`.
3. **Concurrency**: Ensures **thread-safe** CAN transmission.

### **Concurrency & Real-Time**

- **Queue-Based**: Waits indefinitely on `msgOutQ` using `xQueueReceive(msgOutQ, ...)`. This is a **blocking** call and suspends the task until a message arrives.
- **Semaphore Usage**: Takes `CAN_TX_Semaphore` before calling `CAN_TX(...)`. This ensures exclusive access to the CAN transmit function/hardware resource. Potentially blocks if the semaphore is held by another task.
- **Overall Real-Time**: Because it waits on a queue, it only wakes when new messages arrive. Small impact on real time performance

---

## **4. decodeTask**

### **Priority**: _5_

### **Purpose**

Processes incoming CAN messages and updates key states.

### **Key Operations**

1. **Queue Reception**: Waits on `xQueueReceive(msgInQ, local_RX_Message, portMAX_DELAY)`.
2. **Message Parsing**: Updates `keys5`/`keys6` based on received data.
3. **Sampler Integration**: If `samplerEnabled`, logs events for playback.

### **Concurrency & Real-Time**

- **Queue-Based**: Blocks on `msgInQ` (`xQueueReceive`), waiting for incoming CAN messages.
- **Mutex Usage**:
  - takes `sysState.mutex` to read sampler state (`knob2.getPress()`).
  - takes `externalKeyMutex` to modify `keys5` and `keys6` if the local module is octave 4.
- **Potential Blocking**:
  - Acquiring `externalKeyMutex` and `sysState.mutex` can block if other tasks hold them.

---

## **5. samplerTask**

### **Priority**: _3_

### **Purpose**

Handles **event recording and playback**.

### **Key Operations**

1. **Loop Timing**: Uses `vTaskDelayUntil()` to maintain a fixed loop interval.
2. **Playback**: Replays events in `playbackBuffer`.
3. **Buffer Merge**: Locks `samplerMutex` to merge `recordingBuffer`.
4. **Sorting**: Uses insertion sort for **timestamp-ordered playback**.

### **Concurrency & Real-Time**

- **Loop Timing**:
  - Runs continuously but checks sampler enable/disable state and performs its own loop cycle using `vTaskDelayUntil` based on `samplerLoopLength`.
  - Uses additional `vTaskDelay` calls between playback events (to match event timestamps).
- **Mutex Usage**:
  - takes `sysState.mutex` to read the sampler state (`knob2.getPress()`).
  - takes `samplerMutex` when merging new recorded events into the `playbackBuffer` and sorting them.
