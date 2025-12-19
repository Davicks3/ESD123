#pragma once

#include <Arduino.h>
#include "driver/i2s.h"
#include "LrclkCounter.h"

class Sampler {
public:
    // I2S pin config (adjust to your wiring)
    static const int PIN_BCK   = 26;
    static const int PIN_WS    = 25;  // LRCLK (also wired to PCNT pin)
    static const int PIN_DIN   = 33;
    static const int PIN_LRCLK_PCNT = 14;  // wired from PIN_WS

    static const int SAMPLE_RATE = 192000;
    static const i2s_port_t PORT = I2S_NUM_0;

    // Ring config
    static const size_t RING_FRAMES = 4096;   // 4096 frames ≈ 21.3 ms at 192 kHz
    static const size_t CHANNELS    = 2;
    static const size_t BYTES_PER_SAMPLE = 4;
    static const size_t BYTES_PER_FRAME  = CHANNELS * BYTES_PER_SAMPLE;

    Sampler() = default;

    bool begin();
    void service();  // drain I2S into ring, keep mapping updated

    // Called from ISR at trigger time
    void onTriggerISR();

    // Fetch N frames starting at the trigger moment (post-trigger only)
    // dest must hold framesWanted * 2 int32_t (L/R interleaved).
    size_t fetchFromTrigger(int32_t* dest, size_t framesWanted);

    // Helper: convert raw code to volts (differential peak)
    float codeToVoltage(int32_t code) const;

    // Debug accessors
    uint64_t getWriteIndex() const { return writeIndex; }
    uint32_t getHwAtWriteIndex() const { return hwAtWriteIndex; }
    uint32_t getHwTrigger() const { return hwTrigger; }

private:
    // I2S ring of raw codes (L/R interleaved)
    int32_t ring[RING_FRAMES * CHANNELS];

    volatile uint64_t writeIndex      = 0;  // total frames written into ring
    volatile bool     triggered       = false;
    volatile uint32_t hwTrigger       = 0;  // LRCLK count at trigger
    volatile uint32_t hwAtWriteIndex  = 0;  // LRCLK count corresponding to writeIndex

    LrclkCounter lrclk;

    // Internal: blocking settle of first 100 ms
    void settleAdc();
};