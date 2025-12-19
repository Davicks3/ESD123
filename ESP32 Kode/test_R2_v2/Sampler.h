#ifndef SAMPLER_H
#define SAMPLER_H

#include <Arduino.h>
#include "driver/i2s.h"
#include "esp_err.h"

class Sampler {
public:
    // Constructor: provide I2S pins
    Sampler(int bclkPin, int lrclkPin, int dataPin);

    // Init I2S, settle ADC (~100 ms discard)
    bool begin();

    // Must be called often in loop() to keep the ring buffer filled.
    void update();

    // Call from ISR on trigger edge.
    void onTriggerISR();

    // Trigger state helpers
    bool hasTrigger() const        { return _haveTrigger; }
    void clearTrigger()            { _haveTrigger = false; }

    // Frames written since trigger (for "is it time to capture yet?")
    uint32_t framesSinceTrigger() const;

    // Set threshold for marker (negative value, e.g. -0.2 V)
    void setMarkerThreshold(float t) { _markerThreshold = t; }

    // Copy framesToCopy frames starting from the last marker before trigger
    // into dest as voltages [L,R,L,R,...]. Returns frames actually copied.
    size_t copyFromAlignedMarker(float *dest, size_t framesToCopy);

private:
    // ---- I2S configuration ----
    static constexpr i2s_port_t I2S_PORT          = I2S_NUM_0;
    static constexpr int        SAMPLE_RATE_HZ    = 192000;
    static constexpr int        CHANNELS         = 2;
    static constexpr i2s_bits_per_sample_t BITS_PER_SAMPLE =
        I2S_BITS_PER_SAMPLE_32BIT;

    static constexpr int DMA_BUF_LEN   = 256;  // frames per DMA buffer
    static constexpr int DMA_BUF_COUNT = 4;    // number of DMA buffers

    static constexpr int BYTES_PER_SAMPLE = 4; // 32-bit
    static constexpr int BYTES_PER_FRAME  = CHANNELS * BYTES_PER_SAMPLE; // 8

    // Software ring buffer capacity (in frames)
    static constexpr int RING_FRAMES = 4096;   // ~21 ms at 192 kHz

    // Marker search window (max frames to look back before trigger)
    static constexpr uint64_t MARKER_SEARCH_MAX_FRAMES = 512; // ~2.7 ms

    // I2S pins
    int _bclkPin;
    int _lrclkPin;
    int _dataPin;

    // Ring buffer: interleaved L,R codes
    // Size: 2 * RING_FRAMES * 4 bytes = 32 kB
    int32_t _ring[2 * RING_FRAMES];

    // Monotonic global frame index of next frame to be written
    volatile uint64_t _writeIndex;

    // Trigger info
    volatile bool     _haveTrigger;
    volatile uint64_t _triggerIndex;

    // Marker detection threshold (volts)
    float _markerThreshold;

    // Map global frame index -> ring index
    inline int ringPos(uint64_t globalIndex) const {
        return static_cast<int>(globalIndex % RING_FRAMES);
    }

    // Find global frame index of last marker (L < threshold) before trigger
    uint64_t findLastMarkerBeforeTrigger(uint64_t searchMaxFrames) const;

    // Convert raw PCM1809 code (32-bit signed) to differential peak voltage.
    float codeToVoltage(int32_t code) const;
};

#endif // SAMPLER_H