// LRCLKCounter.h
#pragma once

#include <Arduino.h>
#include "driver/pcnt.h"

// Simple wrapper around PCNT to count LRCLK edges on a GPIO.
// We clear the counter after each read, so wrap is not an issue
// as long as interval * freq < 32767 (which is true for 192kHz at 50 ms).
class LRCLKCounter {
public:
    LRCLKCounter()
        : _unit(PCNT_UNIT_0) {}

    bool begin(int gpioPin);

    // Read current count and clear the counter.
    // Returns number of pulses since last call.
    int16_t readAndClear();

private:
    pcnt_unit_t _unit;
};