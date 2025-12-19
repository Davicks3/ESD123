#pragma once

#include <Arduino.h>
#include "driver/pcnt.h"

class LrclkCounter {
public:
    bool begin(gpio_num_t lrclkPulsePin);  // pin that sees LRCLK (e.g. GPIO14)
    void update();                         // extend 16-bit HW counter to 32-bit
    uint32_t read32() const { return hwCount32; }

private:
    pcnt_unit_t unit = PCNT_UNIT_0;
    gpio_num_t  pin;
    int16_t     lastRaw = 0;
    uint32_t    hwCount32 = 0;
};