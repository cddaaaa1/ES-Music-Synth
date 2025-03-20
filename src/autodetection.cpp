#include "autodetection.h"
#include "globals.h"
#include "pins.h"
void autoDetectHandshake()
{
    digitalWrite(OUT_PIN, HIGH);
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
        moduleOctave = 5;
    }
}

void readHandshake(bool &west, bool &east)
{
    digitalWrite(REN_PIN, LOW);
    digitalWrite(RA0_PIN, 5 & 0x01);
    digitalWrite(RA1_PIN, 5 & 0x02);
    digitalWrite(RA2_PIN, 5 & 0x04);
    digitalWrite(OUT_PIN, HIGH);
    digitalWrite(REN_PIN, HIGH);
    delayMicroseconds(3);
    west = digitalRead(C3_PIN);
    digitalWrite(REN_PIN, LOW);

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