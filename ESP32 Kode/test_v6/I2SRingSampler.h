// I2SRingSampler.h
#pragma once

#include <Arduino.h>
#include "driver/i2s.h"

// ---- I2S configuration ----
static const i2s_port_t I2S_PORT       = I2S_NUM_0;
static const int        SAMPLE_RATE_HZ = 192000;
static const int        CHANNELS       = 2;
static const i2s_bits_per_sample_t BITS_PER_SAMPLE = I2S_BITS_PER_SAMPLE_32BIT;

static const int DMA_BUF_LEN   = 256;  // frames per DMA buffer
static const int DMA_BUF_COUNT = 8;    // number of DMA buffers

// ---- Derived sizes ----
static const int BYTES_PER_SAMPLE = (int)BITS_PER_SAMPLE / 8;    // 4
static const int BYTES_PER_FRAME  = CHANNELS * BYTES_PER_SAMPLE; // 8

class I2SRingSampler {
public:
    // ringFrames = number of frames stored in software ring buffer
    I2SRingSampler(size_t ringFrames);

    // Configure I2S, settle ADC by discarding first ~100 ms
    bool begin(int bclkPin, int lrclkPin, int dataInPin);

    // Call this very often in loop() to keep ring buffer filled
    void service();

    // Mark trigger at current write index (frame index)
    void markTrigger();

    // Get the frame index where the last trigger was marked
    uint32_t getTriggerIndex() const { return triggerIndex; }

    // Global frame index of write pointer (monotonic, wraps at 2^32)
    uint32_t getWriteIndex() const { return writeIndex; }

    // Check if we have at least framesNeeded after triggerIndex
    bool hasPostTriggerWindow(uint32_t framesNeeded) const;

    // Extract framesNeeded frames starting at triggerIndex into dest[]
    // dest must be size 2*framesNeeded (L,R interleaved).
    // Returns true on success.
    bool extractWindow(float* dest, uint32_t framesNeeded) const;

private:
    size_t  ringSize;   // in frames
    int32_t *ringL;     // ring buffer left channel
    int32_t *ringR;     // ring buffer right channel

    volatile uint32_t writeIndex;   // global frame counter
    volatile uint32_t triggerIndex; // frame index at trigger time

    // Convert raw PCM1809 code (32-bit signed) to differential peak voltage.
    float codeToVoltage(int32_t code) const;
};