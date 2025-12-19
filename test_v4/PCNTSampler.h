#pragma once

#include <Arduino.h>
#include <driver/i2s.h>
#include <driver/pcnt.h>

// Simple PCNT + I2S-based sampler
// - Continuously flushes DMA in IDLE state
// - On trigger: aligns read to trigger frame using PCNT-produced frame index
// - Then captures N frames into an internal buffer

class PCNTSampler
{
public:
    PCNTSampler(int bclkPin,
                int lrclkPin,
                int dataInPin,
                int triggerPin,
                int captureFrames);

    // Initialize I2S + PCNT, start continuous sampling and initial settle discard.
    bool begin();

    // ISR-safe: called from external GPIO ISR when trigger arrives.
    void IRAM_ATTR onTriggerISR();

    // Called frequently from loop() to fold PCNT counts into producedFrames.
    void updateFromPCNT();

    // Run state machine: flush, align, capture.
    // Returns true once a full capture is ready.
    bool service();

    // Access captured data once service() has returned true.
    const float* getBuffer() const { return captureBuffer; }
    int getCapturedFrames() const { return captureFrames; }

private:
    // --- config ---
    int bclkPin;
    int lrclkPin;
    int dataInPin;
    int triggerPin;
    const int captureFrames;

    // --- I2S / DMA config ---
    static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
    static constexpr int SAMPLE_RATE_HZ  = 192000;
    static constexpr i2s_bits_per_sample_t BITS_PER_SAMPLE = I2S_BITS_PER_SAMPLE_32BIT;
    static constexpr int CHANNELS        = 2;
    static constexpr int BYTES_PER_SAMPLE = 4;    // 32-bit
    static constexpr int BYTES_PER_FRAME  = 8;    // 2ch * 4B
    static constexpr int DMA_BUF_LEN      = 256;  // frames per DMA buffer
    static constexpr int DMA_BUF_COUNT    = 6;    // buffers (more headroom)

    // maximum frames we read in one i2s_read call (to keep stack small)
    static constexpr int MAX_FRAMES_PER_READ = 64;

    // --- PCNT config ---
    static constexpr pcnt_unit_t  PCNT_UNIT   = PCNT_UNIT_0;
    static constexpr pcnt_channel_t PCNT_CH   = PCNT_CHANNEL_0;

    // --- state machine ---
    enum State { IDLE, ALIGNING, CAPTURING };
    volatile bool triggerRequested = false;
    State state                    = IDLE;

    // Frame indices in "global frame space"
    volatile uint64_t producedFrames = 0;   // incremented from PCNT
    uint64_t consumedFrames          = 0;   // frames we’ve read from I2S
    uint64_t triggerFrame            = 0;   // frame index at trigger

    // capture buffer
    float* captureBuffer = nullptr;
    int    captureCount  = 0;
    bool   captureDone   = false;

    // internal helpers
    bool setupI2S();
    bool setupPCNT();
    void discardInitialSettle();   // discard ~100 ms at startup

    size_t readFrames(int frames, uint8_t* buf, TickType_t timeoutTicks);

    float codeToVoltage(int32_t code) const;
};
