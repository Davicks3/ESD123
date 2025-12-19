#pragma once

#include <Arduino.h>
#include <driver/i2s.h>
#include "WritePointer.h"

// I2S / ADC config
static const i2s_port_t I2S_PORT       = I2S_NUM_0;
static const int        SAMPLE_RATE_HZ = 192000;
static const int        CHANNELS       = 2;
static const i2s_bits_per_sample_t BITS_PER_SAMPLE = I2S_BITS_PER_SAMPLE_32BIT;

// DMA config
static const int DMA_BUF_LEN   = 256;  // frames per DMA buffer
static const int DMA_BUF_COUNT = 4;    // number of DMA buffers

// Derived sizes
static const int BYTES_PER_SAMPLE = (int)BITS_PER_SAMPLE / 8;     // 4
static const int BYTES_PER_FRAME  = CHANNELS * BYTES_PER_SAMPLE;  // 8

class Sampler {
public:
    // BClkPin: I2S BCLK
    // LRClkPin: I2S WS / LRCLK
    // DataPin: I2S data in from PCM1809
    // writePtrRef: WritePointer that tracks LRCLK on e.g. GPIO14
    Sampler(int BClkPin, int LRClkPin, int DataPin, WritePointer &writePtrRef);

    // Initialize I2S, discard ~100 ms of samples, and align
    // readIndex with writeIndex.
    bool begin();

    // Mark a trigger event: "from now on I care".
    // This records the writeIndex at trigger time.
    void trigger();

    // Fetch up to framesRequested frames into dest (L,R volts interleaved).
    // On the first fetch after trigger, it discards all frames before
    // triggerIndex (or as many as still exist in the DMA ring).
    size_t fetch(float *dest, size_t framesRequested);

private:
    int bclkPin;
    int lrclkPin;
    int dataPin;

    WritePointer &writePtr;

    uint32_t ringFrames;      // DMA ring capacity in frames
    uint64_t readIndex;       // total frames consumed from I2S
    uint64_t triggerIndex;    // writeIndex at trigger time
    bool     triggered;
    bool     alignedToTrigger;

    float codeToVoltage(int32_t code) const;
};