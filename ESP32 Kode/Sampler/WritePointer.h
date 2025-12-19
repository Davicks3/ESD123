#pragma once
#include <driver/pcnt.h>

class WritePointer {
public:
    WritePointer(int PCPin, pcnt_unit_t unit = PCNT_UNIT_0);

    bool begin();          // Initialize PCNT + ISR
    uint64_t get() const;  // Get global write pointer (frames)
    void clear();          // Reset both hardware + software counters

private:
    int PCPin;
    pcnt_unit_t unit;

    // Overflow pages (software extension of 16-bit hardware counter)
    static void IRAM_ATTR isrHandler(void *arg);
    volatile uint32_t overflowPages = 0;
};