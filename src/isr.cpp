#include "isr.h"
#include "globals.h"

// --------- The 22kHz Audio ISR --------------------
void sampleISR()
{
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
        // Here, we simply add a constant amplitude. For a more natural click,
        // you could apply a decaying envelope.
        Vout += TICK_AMPLITUDE;
        // Decrement the tick counter:
        if (metronomeCounter > 0)
            metronomeCounter--;
        else
            metronomeActive = false;
    }
    // Output on e.g. OUTR_PIN = A3
    analogWrite(A3, Vout + 128); // 0..255 for DAC
}
