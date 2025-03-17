#ifndef KNOB_H
#define KNOB_H

#include <Arduino.h>
#include <bitset>

class Knob
{
private:
    int rotationValue; // Current rotation value
    int rotationVariable;
    int lowerLimit;           // Minimum limit for rotation
    int upperLimit;           // Maximum limit for rotation
    std::bitset<2> prevState; // Previous state of the quadrature inputs
    bool press;
    bool prevPress;
    enum class pressState
    {
        IDLE,
        PRESSED,
        RELEASED
    };
    pressState currentPressState = pressState::IDLE;

public:
    // Constructor
    Knob(int minLimit = -100, int maxLimit = 100) : rotationValue(0), lowerLimit(minLimit), upperLimit(maxLimit), prevState(0) {}

    // Set limits for rotation value
    void setLimits(int minLimit, int maxLimit)
    {
        lowerLimit = minLimit;
        upperLimit = maxLimit;
    }

    void updatePress(std::bitset<1> currentPress)
    {
        switch (currentPressState)
        {
        case pressState::IDLE:
            if (currentPress == std::bitset<1>("0"))
            {                   // Button is pressed
                press = !press; // Toggle press state
                currentPressState = pressState::PRESSED;
            }
            break;

        case pressState::PRESSED:
            if (currentPress == std::bitset<1>("1"))
            { // Button is released
                currentPressState = pressState::RELEASED;
            }
            break;

        case pressState::RELEASED:
            if (currentPress == std::bitset<1>("0"))
            { // Wait for full release before allowing another press
                currentPressState = pressState::IDLE;
            }
            break;
        }
    }

    // Update the rotation value based on quadrature encoder inputs
    void updateRotation(std::bitset<2> currentState)
    {
        if (prevState != currentState)
        {
            if (prevState == std::bitset<2>("00") && currentState == std::bitset<2>("01"))
            {
                rotationVariable = +1;
            }
            else if (prevState == std::bitset<2>("11") && currentState == std::bitset<2>("10"))
            {
                rotationVariable = +1;
            }
            else if (prevState == std::bitset<2>("10") && currentState == std::bitset<2>("11"))
            {
                rotationVariable = -1;
            }
            else if (prevState == std::bitset<2>("01") && currentState == std::bitset<2>("00"))
            {
                rotationVariable = -1;
            }
            else if ((prevState == std::bitset<2>("00") && currentState == std::bitset<2>("11")) ||
                     (prevState == std::bitset<2>("11") && currentState == std::bitset<2>("00")))
                rotationVariable = 0;
            else
                rotationVariable = 0;
        }
        prevState = currentState;
        rotationValue = constrain(rotationValue + rotationVariable, lowerLimit, upperLimit);
    }

    bool getPress()
    {
        return press;
    }

    // Get the current rotation value
    int getRotationValue() const
    {
        return rotationValue;
    }
};

#endif // KNOB_H