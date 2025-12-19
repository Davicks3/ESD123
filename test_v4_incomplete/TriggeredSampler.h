#pragma once

#include <Arduino.h>
#include "driver/i2s.h"
#include "esp_err.h"

#include "FrameCounter.h"

class TriggeredSampler {
public:
    // pins: BCLK, LRCLK, DATA_IN, LRCLK_PCNT (wired to LRCLK)
    TriggeredSampler(int bclkPin, int lrclkPin, int dataPin, int lrclkPcntPin);

    bool begin();                 // init I2S, PCNT, settle ADC
    void service();               // call often from loop()

    // Called from main loop (after ISR sets a flag)
    void requestTrigger();

    bool hasCapture() const { return captureReady; }
    const float* getCapture() const { return captureBuf; }
    void clearCapture() { captureReady = false; }

private:
    enum State {
        IDLE,
        ALIGNING,
        CAPTURING
    };

    // I2S / sampling constants
    static const int SAMPLE_RATE_HZ    = 192000;
    static const int CHANNELS          = 2;
    static const i2s_bits_per_sample_t BITS_PER_SAMPLE = I2S_BITS_PER_SAMPLE_32BIT;

    static const int DMA_BUF_LEN   = 256;  // frames per DMA buffer
    static const int DMA_BUF_COUNT = 8;    // number of DMA buffers

    static const int BYTES_PER_SAMPLE = (int)BITS_PER_SAMPLE / 8;     // 4
    static const int BYTES_PER_FRAME  = CHANNELS * BYTES_PER_SAMPLE;  // 8

    static const int CAPTURE_FRAMES = 1024;
    static const int FLUSH_CHUNK    = 64;   // frames per flush operation
    static const int CAPTURE_CHUNK  = 64;   // frames per capture operation

    int bclkPin;
    int lrclkPin;
    int dataPin;
    int lrclkPcntPin;

    FrameCounter frameCounter;

    volatile bool triggerRequested;
    State state;

    uint64_t producedFrames;      // from PCNT (global)
    uint64_t consumedFrames;      // frames we have read via i2s_read
    uint64_t triggerFrame;        // frame index at trigger
    size_t   captured;            // frames captured into captureBuf

    bool   captureReady;
    float  captureBuf[CAPTURE_FRAMES * 2]; // stereo: L,R per frame

    bool initI2S();
    void settleADC();

    void idleStep();      // continuous flush when idle
    void aligningStep();  // discard until consumedFrames >= triggerFrame
    void capturingStep(); // read into captureBuf

    float codeToVoltage(int32_t code) const;
};