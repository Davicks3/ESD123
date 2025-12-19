// Sampler.h
#pragma once

#include <Arduino.h>
#include <driver/i2s.h>
#include "FrameRingTracker.h"

// ---- I2S configuration constants ----
static const i2s_port_t I2S_PORT       = I2S_NUM_0;
static const int        SAMPLE_RATE_HZ = 192000;
static const int        CHANNELS       = 2;
static const i2s_bits_per_sample_t BITS_PER_SAMPLE = I2S_BITS_PER_SAMPLE_32BIT;

// ---- DMA configuration (tune as needed) ----
static const int DMA_BUF_LEN   = 256;  // frames per DMA buffer
static const int DMA_BUF_COUNT = 4;    // number of DMA buffers

// ---- Derived sizes ----
static const int BYTES_PER_SAMPLE = (int)BITS_PER_SAMPLE / 8;     // 4
static const int BYTES_PER_FRAME  = CHANNELS * BYTES_PER_SAMPLE;  // 8

// I2S pins - ADAPT to your wiring
static const int I2S_BCK_PIN   = 26;  // BCLK
static const int I2S_WS_PIN    = 25;  // LRCLK / FSYNC
static const int I2S_DATA_IN   = 33;  // SDOUT from PCM1809

class Sampler {
public:
    Sampler()
        : tracker(DMA_BUF_LEN * DMA_BUF_COUNT),
          triggered(false),
          triggerIndex(0),
          alignedToTrigger(false) {}

    bool begin();

    // Called when the external trigger happens (e.g., via ISR)
    void trigger();

    // Called from LRCLK/PCNT logic to inform how many new frames were produced
    inline void onFramesProduced(uint32_t frames) {
        tracker.onFramesProduced(frames);
    }

    // Fetch up to framesRequested frames AFTER trigger.
    // dest must have space for 2 * framesRequested floats (L/R).
    size_t fetch(float *dest, size_t framesRequested);

private:
    FrameRingTracker tracker;
    bool             triggered;
    uint64_t         triggerIndex;
    bool             alignedToTrigger;

    // Returns how many frames were actually skipped
    uint32_t skipFramesWithI2S(uint32_t framesToSkip);

    float codeToVoltage(int32_t code) const;
};
