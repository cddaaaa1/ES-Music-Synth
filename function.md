# **Function Descriptions**

## **1. setStepSizes()**
### **Purpose**  
Collects up to five pressed notes from three keyboards (C4, C5, and C6). For each note, it retrieves the corresponding step size and atomically updates global variables (`currentStepSize1` through `currentStepSize5`).

### **Concurrency and Real-Time Considerations**  
- Uses **atomic operations** to update shared variables safely.  
- Does not rely on FreeRTOS mechanisms, making it **non-blocking**.  
- Processes **up to five** pressed notes at a time.

---

## **2. autoDetectHandshake()**
### **Purpose**  
Determines whether the module operates as **octave 4, 5, or 6** by manipulating and reading handshake pins.

### **Key Operations**  
1. **Set Handshake Output**: Forces the handshake output pin (`OUT_PIN`) high.  
2. **Read Adjacent Modules**: Calls `readHandshake(west, east)` to check west/east connections.  
3. **Determine Octave**:
   - **West HIGH** → `moduleOctave = 4`  
   - **East HIGH** → `moduleOctave = 6`  
   - **Both LOW** → `moduleOctave = 5`  

### **Concurrency and Real-Time Considerations**  
- Uses a **blocking delay (`delay(50)`)**, which may **impact real-time responsiveness**.  
- Should not be called from **time-critical** tasks.

---

## **3. readHandshake(bool &west, bool &east)**
### **Purpose**  
Reads handshake signals from **west and east** modules using row/column selection pins.

### **Concurrency and Real-Time Considerations**  
- Uses **microsecond-scale delays (`delayMicroseconds(3)`)**, which should be minimal.  
- Safe to call in most **non-time-critical** contexts.

---

## **4. simulateKeyEvent(const NoteEvent &event)**
### **Purpose**  
Simulates a **key press (`'P'`) or release (`'R'`)** in octave **4, 5, or 6**.

### **Concurrency and Real-Time Considerations**  
- **Modifies shared bitsets (`keysX`)**, which may be accessed by **tasks or ISRs**.  
- Potential **race conditions** if not handled properly.

---

## **5. sampler_recordEvent(char type, uint8_t octave, uint8_t noteIndex)**
### **Purpose**  
Records key events (user or remote) with timestamps and stores them in `recordingBuffer` for playback.

### **Key Operations**  
1. **Timestamping**: Uses `xTaskGetTickCount()` to mark events.  
2. **Mutex-Protected Buffer Write**:  
   - Takes `samplerMutex` before writing to `recordingBuffer`.  
   - Increments `recordedCount` if space is available.  
   - Releases `samplerMutex` after writing.

### **Concurrency and Real-Time Considerations**  
- **Protected by `samplerMutex`** to prevent race conditions.  
- Uses a **blocking delay (`vTaskDelay`)** if the sampler is disabled, which **may impact real-time performance**.

---

## **6. releaseAllNotes()**
### **Purpose**  
Ensures that no keys remain **stuck in a pressed state**, particularly useful during **system reset** or **sampler clearing**.

