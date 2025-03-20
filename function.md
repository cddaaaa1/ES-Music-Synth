# **Function Descriptions**

---

## **1. `setStepSizes()`**

### **Purpose**

Collects up to five pressed notes from the three keyboards (**C4**, **C5**, **C6**). Each detected note’s “step size” is fetched from the relevant lookup array (`stepSizes4`, `stepSizes5`, `stepSizes6`) and stored in local variables. The function then **atomically** updates global variables (`currentStepSize1` through `currentStepSize5`), ensuring consistent step-size values for the audio engine.

### **Concurrency & Real-Time**

- Uses **`localKeyMutex`** only when reading **C4**’s bitset (`keys4`).
- **`externalKeyMutex`** used for **C5** or **C6** bitsets
- Final updates to global step-size variables use **atomic operations** (`__atomic_store_n`) for thread safety.
- Processes **up to five** notes simultaneously.

---

## **2. `autoDetectHandshake()`**

### **Purpose**

Determines the module’s operating octave (**4**, **5**, or **6**) by driving a handshake pin (`OUT_PIN`) high, then reading adjacent modules (**west/east**) via `readHandshake(west, east)`.

---

## **3. `readHandshake(bool &west, bool &east)`**

### **Purpose**

Reads handshake signals from neighboring modules (**west** and **east**) using row/column pins (`RA0_PIN`, `RA1_PIN`, `RA2_PIN`) and the shared line (`C3_PIN`).

---

## **4. `simulateKeyEvent(const NoteEvent &event)`**

### **Purpose**

Mimics a **key press** (`'P'`) or **release** (`'R'`) on either **octave 4**, **5**, or **6** by updating the corresponding bitset (`keys4`, `keys5`, or `keys6`).

### **Concurrency & Real-Time**

- Uses `localKeyMutex` or `externalKeyMutex` to safely modify shared bitsets.
- Prevents simultaneous writes to the same bitset from multiple tasks or ISRs.

---

## **5. `sampler_recordEvent(char type, uint8_t octave, uint8_t noteIndex)`**

### **Purpose**

Records a note event (press or release) with a timestamp for later playback. If the sampler is disabled, the function delays briefly and returns without recording.

### **Key Operations**

1. **Check Sampler State**: use `sysState.mutex` to see if the sampler is enabled.
2. **Blocking Delay**: If disabled, calls `vTaskDelay(50)`.
3. **Timestamp & Record**: Uses `xTaskGetTickCount()` to timestamp the event, then appends to `recordingBuffer` (protected by `samplerMutex`).

### **Concurrency & Real-Time**

- Protected by **mutexes** (`sysState.mutex`, `samplerMutex`).

---

## **6. `releaseAllNotes()`**

### **Purpose**

Clears any pressed keys on all keyboards (**C4**, **C5**, and **C6**). Useful on system reset or whenever an “all-notes-off” state is required.

### **Concurrency & Real-Time**

- Sequentially obtains **`localKeyMutex`** (for `keys4`) and **`externalKeyMutex`** (for `keys5` and `keys6`) before modifying each bitset.
- Each operation can **block** if the mutex is held by another task, but typically for a short duration.
