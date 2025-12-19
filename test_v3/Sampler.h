#pragma once

#include <Arduino.h>
#include "driver/i2s.h"
#include "esp_err.h"

class Sampler {
public:
    // I2S port
    static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;

    // Pins – adjust if needed
    static constexpr int BCK_PIN      = 26;  // BCLK
    static constexpr int WS_PIN       = 25;  // LRCLK / FSYNC
    static constexpr int DATA_IN_PIN  = 33;  // DOUT from PCM1809

    // Audio config
    static constexpr int SAMPLE_RATE_HZ = 192000;
    static constexpr int CHANNELS       = 2;
    static constexpr i2s_bits_per_sample_t BITS_PER_SAMPLE = I2S_BITS_PER_SAMPLE_32BIT;

    // DMA config: ring = DMA_BUF_LEN * DMA_BUF_COUNT = 256 * 4 = 1024 frames
    static constexpr int DMA_BUF_LEN   = 256;  // frames per DMA buffer
    static constexpr int DMA_BUF_COUNT = 4;

    // Derived sizes
    static constexpr int BYTES_PER_SAMPLE = 4; // 32-bit
    static constexpr int BYTES_PER_FRAME  = CHANNELS * BYTES_PER_SAMPLE; // 8
    static constexpr int RING_FRAMES      = DMA_BUF_LEN * DMA_BUF_COUNT; // 1024

    // Fixed capture length (frames)
    static constexpr size_t CAPTURE_FRAMES = 1024;

    // Initialize I2S and do one-time warm-up flush
    bool begin();

    // On each trigger:
    //  1) flush RING_FRAMES from RX (best-effort)
    //  2) read CAPTURE_FRAMES frames into dest
    //
    // dest must have space for CAPTURE_FRAMES * 2 floats (L,R).
    // Returns actual frames captured (should be CAPTURE_FRAMES if all goes well).
    size_t capture1024(float *dest);

private:
    void flushRing(); // discard RING_FRAMES frames
    static float codeToVoltage(int32_t code);
};