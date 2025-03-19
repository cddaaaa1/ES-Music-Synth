# **Interrupt Service Routines (ISR)**

## **1. sampleISR**
### **Purpose**  
Handles real-time audio synthesis, mixing multiple oscillators, and applying metronome ticks.

### **Key Operations**  
1. **Phase Accumulator Update**: Increments phase for each oscillator.  
2. **Audio Mixing**: Combines multiple waveform sources.  
3. **Volume Scaling**: Adjusts `Vout` based on system volume.  
4. **Metronome Clicks**: Adds ticks when `metronomeActive` is set.  
5. **Output to DAC**: Uses `analogWrite()` to send data to the DAC.

### **WCET Measurement**  
- Uses `micros()` at entry/exit to track worst-case execution time (WCET).  
- Logs `maxIsrTime` to monitor peak duration.  
- Outputs WCET every 1000 iterations.

---

## **2. CAN_RX_ISR**
### **Purpose**  
Handles incoming CAN messages and queues them for processing.

### **Key Operations**  
1. **Read CAN Frame**: Retrieves data from the hardware buffer.  
2. **Queue Message**: Stores message in `msgInQ` for `decodeTask`.  
3. **Interrupt Clearing**: Clears hardware interrupt flag.

### **Concurrency Considerations**  
- Operates at high priority, minimizing delay.  
- Uses a queue to decouple CAN processing.

---

## **3. CAN_TX_ISR**
### **Purpose**  
Handles CAN message transmission completion.

### **Key Operations**  
1. **Confirm Transmission**: Verifies message was sent successfully.  
2. **Release Semaphore**: Frees `CAN_TX_Semaphore` for the next task.  
3. **Interrupt Clearing**: Resets the CAN TX flag.

### **Concurrency Considerations**  
- Ensures only one task transmits at a time using a semaphore.  
- Runs with minimal processing to avoid delays.
