#pragma once

#include <Arduino.h>
#include "driver/pcnt.h"

class FrameCounter {
public:
    FrameCounter();
    bool begin(gpio_num_t pulseGpio);
    void end();
    void clear();
    uint64_t get() const;
    static void IRAM_ATTR isrHandler(void* arg);
    void IRAM_ATTR onHighLimitISR();

private:
    // always use PCNT unit 0
    static constexpr pcnt_unit_t UNIT = PCNT_UNIT_0;

    static constexpr int16_t HIGH_LIMIT = 32767;

    gpio_num_t _pulseGpio;
    volatile uint32_t _overflowCount;
};
