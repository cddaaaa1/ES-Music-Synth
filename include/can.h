#ifndef CAN_H
#define CAN_H

// Declarations for CAN-related functions/tasks
void CAN_TX_ISR(void);
void CAN_RX_ISR(void);

void decodeTask(void *pvParameters);
void CAN_TX_Task(void *pvParameters);

#endif
