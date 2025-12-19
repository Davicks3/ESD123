// FrameCounter.h
#pragma once

#include <Arduino.h>
#include "driver/pcnt.h"

/**
 * FrameCounter
 *
 * Counts pulses on a given GPIO using PCNT unit 0.
 * When the internal PCNT counter reaches the configured high limit,
 * an interrupt fires and we treat that as an "overflow".
 *
 * The class maintains a 64-bit extended counter:
 *   total_count = overflow_count * (HIGH_LIMIT + 1) + current_pcnt_value
 */
class FrameCounter {
public:
    /**
     * Constructor.
     * You can also skip this and just call begin(pin) directly.
     */
    FrameCounter();

    /**
     * Configure PCNT unit 0 and start counting pulses on pulseGpio.
     *
     * The only input you ever give is the GPIO pin.
     *
     * @param pulseGpio   GPIO where pulses arrive (e.g. GPIO_NUM_4)
     * @return true on success, false on failure
     */
    bool begin(gpio_num_t pulseGpio);

    /**
     * Stop the counter and disable events.
     * (Does NOT uninstall the global PCNT ISR service.)
     */
    void end();

    /**
     * Clear the extended counter (overflow + internal PCNT).
     */
    void clear();

    /**
     * Get the current 64-bit total count.
     * Safe to call from non-ISR context.
     */
    uint64_t get() const;

    /**
     * Static ISR handler for PCNT events on unit 0.
     * Registered with pcnt_isr_handler_add().
     */
    static void IRAM_ATTR isrHandler(void* arg);

    /**
     * Instance method called from the ISR when the high limit event happens.
     * Keep this tiny and ISR-safe.
     */
    void IRAM_ATTR onHighLimitISR();

private:
    // We always use PCNT unit 0
    static constexpr pcnt_unit_t UNIT = PCNT_UNIT_0;

    // Fixed high limit; typical signed 16-bit max. Change if you like.
    static constexpr int16_t HIGH_LIMIT = 32767;

    gpio_num_t _pulseGpio;
    volatile uint32_t _overflowCount;
};
