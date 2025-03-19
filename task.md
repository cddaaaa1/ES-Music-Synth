# **Task Descriptions**

## **1. scanKeysTask**
### **Priority**: *1*
### **Purpose**  
Detects key presses/releases from a **7×4 keypad matrix**, updates global state, and handles knob inputs.

### **Key Operations**  
1. **Matrix Scanning**: Reads 7 rows × 4 columns to detect key states.  
2. **Event Handling**: Sends key events to the queue and updates step sizes.  
3. **Handshake Detection**: Calls `autoDetectHandshake()` when handshake lines change.  
4. **Knob Input Updates**: Adjusts volume and enables sampling mode.  
5. **Global State Synchronization**: Uses a mutex to update `sysState`.

### **Concurrency Considerations**  
- Runs every **50 ms**.  
- Uses `xQueueSend()` and a mutex for synchronization.

---

## **2. displayUpdateTask**
### **Priority**: *2*
### **Purpose**  
Updates the display with pressed notes, volume, and sampler status.

### **Key Operations**  
1. **State Synchronization**: Uses `sysState.mutex` to copy shared data.  
2. **Display Rendering**: Shows pressed note names and octave information.  
3. **Concurrency**: Uses `xSemaphoreTake()` for safe data access.

---

## **3. CAN_TX_Task**
### **Priority**: *4*
### **Purpose**  
Sends outgoing CAN messages from `msgOutQ`.

### **Key Operations**  
1. **Queue Reception**: Blocks on `xQueueReceive(msgOutQ, msgOut, portMAX_DELAY)`.  
2. **Semaphore for CAN TX**: Uses `xSemaphoreTake(CAN_TX_Semaphore, portMAX_DELAY)`.  
3. **Concurrency**: Ensures **thread-safe** CAN transmission.

---

## **4. decodeTask**
### **Priority**: *3*
### **Purpose**  
Processes incoming CAN messages and updates key states.

### **Key Operations**  
1. **Queue Reception**: Waits on `xQueueReceive(msgInQ, local_RX_Message, portMAX_DELAY)`.  
2. **Message Parsing**: Updates `keys5`/`keys6` based on received data.  
3. **Sampler Integration**: If `samplerEnabled`, logs events for playback.

### **Concurrency Considerations**  
- Uses a **mutex** to protect `RX_Message` from concurrent access.  
- Atomic operations update step sizes.

---

## **5. samplerTask**
### **Priority**: *5*
### **Purpose**  
Handles **event recording and playback**.

### **Key Operations**  
1. **Loop Timing**: Uses `vTaskDelayUntil()` to maintain a fixed loop interval.  
2. **Playback**: Replays events in `playbackBuffer`.  
3. **Buffer Merge**: Locks `samplerMutex` to merge `recordingBuffer`.  
4. **Sorting**: Uses insertion sort for **timestamp-ordered playback**.

### **Concurrency Considerations**  
- Uses a **mutex** for buffer protection.  
- Employs blocking delays (`vTaskDelay()`).

---

## **6. metronomeTask**
### **Priority**: *6*
### **Purpose**  
Generates **metronome clicks** and LED blinks.

### **Key Operations**  
1. **Click Generation**: Sets `metronomeActive = true`, initializing `metronomeCounter`.  
2. **Concurrency**: Uses `vTaskDelay()` to control beat timing.

