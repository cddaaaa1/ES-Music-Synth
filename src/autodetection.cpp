#include "autodetection.h"
#include "globals.h"
#include "pins.h"
void autoDetectHandshake()
{
    // For auto-detection, we force the handshake outputs to be "on"
    digitalWrite(OUT_PIN, HIGH);
    // Allow signals to settle.
    delay(50);

    bool west, east;

    readHandshake(west, east);

    if (west == HIGH && east == LOW)
    {
        moduleOctave = 4;
    }
    else if (east == HIGH && west == LOW)
    {
        moduleOctave = 6;
    }
    else
    {
        // Either both are connected (center module) or both are disconnected (single module).
        moduleOctave = 5;
    }
}

void readHandshake(bool &west, bool &east)
{
    // Row 5: West handshake input
    digitalWrite(REN_PIN, LOW);
    digitalWrite(RA0_PIN, 5 & 0x01);
    digitalWrite(RA1_PIN, 5 & 0x02);
    digitalWrite(RA2_PIN, 5 & 0x04);
    // Force handshake output on for proper latching:
    digitalWrite(OUT_PIN, HIGH);
    digitalWrite(REN_PIN, HIGH);
    delayMicroseconds(3);
    west = digitalRead(C3_PIN);
    digitalWrite(REN_PIN, LOW);

    // Row 6: East handshake input
    digitalWrite(REN_PIN, LOW);
    digitalWrite(RA0_PIN, 6 & 0x01);
    digitalWrite(RA1_PIN, 6 & 0x02);
    digitalWrite(RA2_PIN, 6 & 0x04);
    digitalWrite(OUT_PIN, HIGH);
    digitalWrite(REN_PIN, HIGH);
    delayMicroseconds(3);
    east = digitalRead(C3_PIN);
    digitalWrite(REN_PIN, LOW);
}