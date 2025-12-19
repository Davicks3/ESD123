#pragma once

#include <Arduino.h>
#include "driver/pcnt.h"

class FrameCounter {
public:
    FrameCounter()
    : unit(PCNT_UNIT_0),
      totalFrames(0),
      initialized(false)
    {}

    // pulsePin is the GPIO where LRCLK is connected (e.g. GPIO14).
    bool begin(gpio_num_t pulsePin);

    // Call often (e.g. every loop/service). Accumulates new pulses into totalFrames.
    void update();

    // Reset total to 0 and clear HW counter.
    void reset();

    // Get total frames produced since last reset.
    uint64_t getTotal() const { return totalFrames; }

private:
    pcnt_unit_t unit;
    volatile uint64_t totalFrames;
    bool initialized;
};