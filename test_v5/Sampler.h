#pragma once

#include <Arduino.h>
#include "driver/i2s.h"

// Very simple sampler for PCM1809 @ 192 kHz, 32-bit stereo.
// - begin(): config I2S, flush first 100 ms for ADC settle
// - discardChunk(): read & throw away a small chunk (for continuous flushing)
// - capture(): after trigger, read N frames into dest[] as voltages

class Sampler {
public:
    // bckPin = BCLK, wsPin = LRCLK/FSYNC, dataInPin = DOUT from PCM1809
    Sampler(int bckPin, int wsPin, int dataInPin);

    // Initialize I2S and flush first 100 ms of samples (blocking).
    bool begin();

    // Discard a small chunk of incoming samples (blocking until chunk is read).
    // Call this repeatedly in loop() while waiting for trigger.
    void discardChunk();

    // Capture 'framesRequested' frames into dest (L/R voltages interleaved).
    // Each frame = dest[2*f] = L, dest[2*f+1] = R.
    // Returns number of frames actually captured.
    size_t capture(float* dest, size_t framesRequested);

private:
    // I2S config constants
    static const i2s_port_t I2S_PORT       = I2S_NUM_0;
    static const int        SAMPLE_RATE_HZ = 192000;
    static const int        CHANNELS       = 2;
    static const i2s_bits_per_sample_t BITS_PER_SAMPLE = I2S_BITS_PER_SAMPLE_32BIT;

    // DMA configuration
    static const int DMA_BUF_LEN   = 64;   // frames per DMA buffer
    static const int DMA_BUF_COUNT = 4;    // number of DMA buffers

    // Derived sizes
    static const int BYTES_PER_SAMPLE = (int)BITS_PER_SAMPLE / 8;          // 4
    static const int BYTES_PER_FRAME  = CHANNELS * BYTES_PER_SAMPLE;       // 8

    // Discard chunk size (in frames)
    static const int DISCARD_CHUNK_FRAMES = 32;  // small => low latency (~167 µs at 192 kHz)

    int _bckPin;
    int _wsPin;
    int _dataPin;

    // Helper: convert raw 32-bit PCM1809 code to differential peak voltage.
    float codeToVoltage(int32_t code) const;
};