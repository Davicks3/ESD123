#pragma once

#include <Arduino.h>
#include <driver/pcnt.h>

// Tracks a monotonically increasing frame counter using PCNT on LRCLK.
// Each call to update() reads how many LRCLK pulses occurred since the
// last update and adds them to extCount.
//
// REQUIREMENT: call update() frequently enough so that the number of
// pulses between calls is < 32767. At 192 kHz LRCLK, calling update()
// every 10 ms gives ~1920 pulses, which is totally safe.
class WritePointer {
public:
    WritePointer(int gpio, pcnt_unit_t unit = PCNT_UNIT_0);

    bool begin();   // config PCNT and clear counter
    void update();  // accumulate "new pulses" into extCount

    uint64_t get() const { return extCount; }

private:
    int         gpio;
    pcnt_unit_t unit;

    uint64_t extCount;  // total frames since begin()
};