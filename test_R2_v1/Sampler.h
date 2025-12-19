#pragma once

#include <Arduino.h>
#include <driver/i2s.h>
#include "esp_err.h"

class Sampler {
public:
    // Pins for I2S + marker output
    Sampler(int bckPin, int wsPin, int dataPin, int markerPin);

    bool begin();
    void update();          // Call often from loop() to drain I2S into ring

    // Call from ISR when trigger happens
    void onTriggerISR();

    // Copy 'frames' frames starting at triggerIndex into dest[2*frames]
    // Returns number of frames actually copied (0 if not enough data yet).
    size_t copyFromTrigger(float *dest, size_t frames);

    // For debugging: how many frames have been captured since trigger
    uint32_t framesSinceTrigger() const;

private:
    // ---- Config constants ----
    static constexpr i2s_port_t I2S_PORT       = I2S_NUM_0;
    static constexpr int        SAMPLE_RATE_HZ = 192000;
    static constexpr int        CHANNELS       = 2;
    static constexpr i2s_bits_per_sample_t BITS_PER_SAMPLE =
        I2S_BITS_PER_SAMPLE_32BIT;

    // Software ring size in frames (stereo frames)
    static constexpr uint32_t RING_FRAMES = 4096; // ~21.3 ms at 192 kHz

    // I2S DMA config
    static constexpr int DMA_BUF_LEN   = 256; // frames per DMA buffer
    static constexpr int DMA_BUF_COUNT = 4;

    static constexpr int BYTES_PER_SAMPLE = 4;                // 32-bit
    static constexpr int BYTES_PER_FRAME  = BYTES_PER_SAMPLE * CHANNELS; // 8

    // Simple frame struct for ring buffer
    struct Frame {
        int32_t L;
        int32_t R;
    };

    // ---- Members ----
    int _bckPin;
    int _wsPin;
    int _dataPin;
    int _markerPin;

    Frame   _ring[RING_FRAMES];
    volatile uint32_t _writeIndex;      // monotonically increasing (wraps)
    volatile uint32_t _triggerIndex;    // writeIndex at trigger time
    volatile bool     _triggered;

    // marker pulse timing
    volatile bool     _markerActive;
    volatile uint32_t _markerEndMs;     // millis() when we turn marker off

    // ---- Helpers ----
    void    _i2sConfig();
    float   _codeToVoltage(int32_t code) const;
    uint32_t _framesAvailableSince(uint32_t index) const;
    uint32_t _indexDiff(uint32_t newer, uint32_t older) const;
};