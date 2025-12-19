#pragma once

#include <Arduino.h>
#include <driver/pcnt.h>

class PulseCounter {
public:
    PulseCounter(int gpio, pcnt_unit_t unit = PCNT_UNIT_0);

    bool begin();

    // Read current count value (since last clear)
    int16_t read();

    // Clear the counter back to 0
    void clear();

private:
    int gpio;
    pcnt_unit_t unit;
};