#ifndef KNOB_H
#define KNOB_H

#include <Arduino.h>
#include <bitset>

class Knob
{
private:
    int rotationValue; // Current rotation value
    int lastUpdateValue = 0;
    int lowerLimit;           // Minimum limit for rotation
    int upperLimit;           // Maximum limit for rotation
    std::bitset<2> prevState; // Previous state of the quadrature inputs

public:
    // Constructor
    Knob(int minLimit = -100, int maxLimit = 100) : rotationValue(0), lowerLimit(minLimit), upperLimit(maxLimit), prevState(0) {}

    // Set limits for rotation value
    void setLimits(int minLimit, int maxLimit)
    {
        lowerLimit = minLimit;
        upperLimit = maxLimit;
    }

    // Update the rotation value based on quadrature encoder inputs
    void updateRotation(std::bitset<2> currentState)
    {
        if (prevState != currentState)
        {
            if (prevState == std::bitset<2>("00") && currentState == std::bitset<2>("01"))
            {
                rotationValue = constrain(rotationValue + 1, lowerLimit, upperLimit);
                lastUpdateValue = 1;
            }
            else if (prevState == std::bitset<2>("11") && currentState == std::bitset<2>("10"))
            {
                rotationValue = constrain(rotationValue + 1, lowerLimit, upperLimit);
                lastUpdateValue = 1;
            }
            else if (prevState == std::bitset<2>("10") && currentState == std::bitset<2>("11"))
            {
                rotationValue = constrain(rotationValue - 1, lowerLimit, upperLimit);
                lastUpdateValue = -1;
            }
            else if (prevState == std::bitset<2>("01") && currentState == std::bitset<2>("00"))
            {
                rotationValue = constrain(rotationValue - 1, lowerLimit, upperLimit);
                lastUpdateValue = -1;
            }
            else if ((prevState == std::bitset<2>("00") && currentState == std::bitset<2>("11")) ||
                     (prevState == std::bitset<2>("11") && currentState == std::bitset<2>("00")))
            {
                rotationValue = constrain(rotationValue + lastUpdateValue, lowerLimit, upperLimit);
            }
        }
        prevState = currentState;
    }

    // Get the current rotation value
    int getRotationValue() const
    {
        return rotationValue;
    }
};

#endif // KNOB_H