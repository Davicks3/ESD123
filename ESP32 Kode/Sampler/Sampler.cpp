#include "Sampler.h"

#include <Arduino.h>
#include "driver/i2s.h"
#include "esp_err.h"

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

// Helper: update software read pointer to simulate DMA overwrite.
// Ensures readIndex is never older than the oldest frame that can still exist.
static void syncReadPointer(uint64_t &readIndex, const uint64_t writeIndex, int ringFrames)
{
    if (writeIndex > readIndex + (uint64_t)ringFrames) {
        readIndex = writeIndex - (uint64_t)ringFrames;
    }
}

Sampler::Sampler(const int BClkPin, const int LRClkPin, const int DataPin)
: BClkPin(BClkPin),
  LRClkPin(LRClkPin),
  DataPin(DataPin),
  writePtr(14, PCNT_UNIT_0)
{}

bool Sampler::begin()
{
    // ---- I2S configuration ----
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE_HZ,
        .bits_per_sample = BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = true,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num   = BClkPin,
        .ws_io_num    = LRClkPin,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = DataPin
    };

    if (i2s_driver_install(I2S_PORT, &i2s_config, 0, nullptr) != ESP_OK) {
        return false;
    }
    if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) {
        return false;
    }
    if (i2s_set_clk(I2S_PORT, SAMPLE_RATE_HZ, BITS_PER_SAMPLE, I2S_CHANNEL_STEREO) != ESP_OK) {
        return false;
    }

    // Mirror I2S LRCLK (WS) to GPIO14 so PCNT can count frames.
    gpio_reset_pin(GPIO_NUM_14);
    gpio_set_direction(GPIO_NUM_14, GPIO_MODE_OUTPUT);
    // Route internal I2S0 WS (LRCLK) signal out to GPIO14
    gpio_matrix_out(GPIO_NUM_14, I2S0O_WS_OUT_IDX, false, false);

    // Compute total DMA capacity in frames.
    ringFrames         = DMA_BUF_LEN * DMA_BUF_COUNT;
    readIndex          = 0;
    triggered          = false;
    alignedAfterTrigger = false;
    triggerFrame       = 0;

    // ---- Initialize write pointer (PCNT on LRCLK) ----
    if (!writePtr.begin()) {
        return false;
    }

    // ---- ADC settle: discard first ~100 ms of samples ----
    const uint32_t settleFrames = SAMPLE_RATE_HZ / 10;  // 100 ms = 1/10 s
    const uint32_t settleBytes  = settleFrames * BYTES_PER_FRAME;

    uint8_t  dummy[256];
    uint32_t discarded = 0;
    while (discarded < settleBytes) {
        size_t toRead = settleBytes - discarded;
        if (toRead > sizeof(dummy)) {
            toRead = sizeof(dummy);
        }

        size_t bytesRead = 0;
        esp_err_t err = i2s_read(I2S_PORT, dummy, toRead, &bytesRead, portMAX_DELAY);
        if (err != ESP_OK || bytesRead == 0) {
            break;
        }

        discarded += (uint32_t)bytesRead;

        // Advance software read pointer to reflect consumed frames.
        uint32_t framesRead = bytesRead / BYTES_PER_FRAME;
        readIndex += framesRead;
    }

    return true;
}

void Sampler::trigger()
{
    // Mark trigger frame in global write index space.
    triggerFrame        = writePtr.get();
    triggered           = true;
    alignedAfterTrigger = false;
}

size_t Sampler::fetch(float *dest, size_t framesRequested)
{
    if (!triggered || dest == nullptr || framesRequested == 0) {
        return 0;
    }

    const uint64_t writeIndex = writePtr.get();
    // Make sure readIndex reflects possible DMA overwrites.
    syncReadPointer(readIndex, writeIndex, ringFrames);

    // On the first fetch after trigger, discard any pre-trigger frames
    // still in the DMA ring so that the next frame we return is >= triggerFrame.
    if (!alignedAfterTrigger) {
        uint64_t R_effective      = readIndex;
        uint64_t preTriggerUnread = 0;

        if (triggerFrame > R_effective) {
            preTriggerUnread = triggerFrame - R_effective;
            // Never discard more than ringFrames; if trigger is too old,
            // earliest post-trigger data is already gone anyway.
            if (preTriggerUnread > (uint64_t)ringFrames) {
                preTriggerUnread = (uint64_t)ringFrames;
            }
        }

        // Discard preTriggerUnread frames, if any.
        uint64_t framesToDiscard = preTriggerUnread;
        uint8_t  discardBuf[128];  // multiple of BYTES_PER_FRAME preferred but not required

        while (framesToDiscard > 0) {
            uint64_t maxFramesThisRead = sizeof(discardBuf) / BYTES_PER_FRAME;
            if (maxFramesThisRead == 0) {
                break;
            }

            uint64_t framesNow   = (framesToDiscard < maxFramesThisRead) ? framesToDiscard : maxFramesThisRead;
            size_t   bytesToRead = (size_t)(framesNow * BYTES_PER_FRAME);
            size_t   bytesRead   = 0;

            esp_err_t err = i2s_read(I2S_PORT, discardBuf, bytesToRead, &bytesRead, portMAX_DELAY);
            if (err != ESP_OK || bytesRead == 0) {
                break;  // stop on error
            }

            uint64_t framesRead = bytesRead / BYTES_PER_FRAME;
            readIndex += framesRead;
            if (framesRead > framesToDiscard) {
                framesToDiscard = 0;
            } else {
                framesToDiscard -= framesRead;
            }
        }

        alignedAfterTrigger = true;
    }

    // ---- Now return up to framesRequested frames into dest as voltages ----
    size_t totalFramesOut = 0;

    // We read in small chunks of raw I2S data into a temp buffer, then convert.
    // Each frame = 2 * int32_t = 8 bytes
    const size_t MAX_FRAMES_PER_READ = 32;  // tune as you like
    uint8_t tempBuf[MAX_FRAMES_PER_READ * BYTES_PER_FRAME];

    while (totalFramesOut < framesRequested) {
        size_t framesToRead = framesRequested - totalFramesOut;
        if (framesToRead > MAX_FRAMES_PER_READ) {
            framesToRead = MAX_FRAMES_PER_READ;
        }

        size_t bytesToRead = framesToRead * BYTES_PER_FRAME;
        size_t bytesRead   = 0;

        esp_err_t err = i2s_read(I2S_PORT,
                                 tempBuf,
                                 bytesToRead,
                                 &bytesRead,
                                 portMAX_DELAY);
        if (err != ESP_OK || bytesRead == 0) {
            break;
        }

        size_t framesRead = bytesRead / BYTES_PER_FRAME;
        if (framesRead == 0) {
            break;
        }

        // Interpret tempBuf as interleaved int32_t L/R samples
        int32_t *codes = reinterpret_cast<int32_t *>(tempBuf);

        for (size_t f = 0; f < framesRead; ++f) {
            int32_t codeL = codes[f * 2 + 0];
            int32_t codeR = codes[f * 2 + 1];

            dest[(totalFramesOut + f) * 2 + 0] = codeToVoltage(codeL);
            dest[(totalFramesOut + f) * 2 + 1] = codeToVoltage(codeR);
        }

        readIndex      += framesRead;
        totalFramesOut += framesRead;

        // If bytesRead was not an exact multiple of BYTES_PER_FRAME, stop to avoid misalignment.
        size_t remainder = bytesRead % BYTES_PER_FRAME;
        if (remainder != 0) {
            break;
        }
    }

    return totalFramesOut;
}

// Convert raw PCM1809 code (32-bit signed) to differential peak voltage.
// Assumes PCM1809 full scale is 2 Vrms differential => 2.828 Vpeak diff.
float Sampler::codeToVoltage(int32_t code) const
{
    const float CODE_FS        = 2147483648.0f;          // 2^31
    const float VFS_DIFF_RMS   = 2.0f;                   // PCM1809 full-scale differential Vrms
    const float VFS_DIFF_PEAK  = VFS_DIFF_RMS * 1.41421356237f; // ≈ 2.828 V peak differential
    return (static_cast<float>(code) / CODE_FS) * VFS_DIFF_PEAK;
}