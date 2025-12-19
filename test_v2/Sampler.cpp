// Sampler.cpp
#include "Sampler.h"

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
        .bck_io_num   = I2S_BCK_PIN,
        .ws_io_num    = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = I2S_DATA_IN
    };

    if (i2s_driver_install(I2S_PORT, &i2s_config, 0, nullptr) != ESP_OK) {
        Serial.println("[Sampler::begin] i2s_driver_install failed");
        return false;
    }
    if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) {
        Serial.println("[Sampler::begin] i2s_set_pin failed");
        return false;
    }
    if (i2s_set_clk(I2S_PORT, SAMPLE_RATE_HZ, BITS_PER_SAMPLE, I2S_CHANNEL_STEREO) != ESP_OK) {
        Serial.println("[Sampler::begin] i2s_set_clk failed");
        return false;
    }

    // Reset tracking state
    tracker.reset();
    triggered        = false;
    triggerIndex     = 0;
    alignedToTrigger = false;

    // Optional: settle ADC by discarding first ~100 ms of samples
    const uint32_t settleFrames = SAMPLE_RATE_HZ / 10;  // 100 ms
    uint32_t actuallySkipped = skipFramesWithI2S(settleFrames);
    tracker.onFramesRead(actuallySkipped);

    return true;
}

void Sampler::trigger()
{
    // Capture current write index (global frame number)
    triggerIndex     = tracker.writeIndex();
    triggered        = true;
    alignedToTrigger = false;
}

// Discard 'framesToSkip' frames using i2s_read into a dummy buffer.
// Returns how many frames were actually skipped.
uint32_t Sampler::skipFramesWithI2S(uint32_t framesToSkip)
{
    const size_t MAX_FR = 32;
    uint8_t dummy[MAX_FR * BYTES_PER_FRAME];
    uint32_t skipped = 0;

    while (framesToSkip > 0) {
        size_t chunk = (framesToSkip > MAX_FR) ? MAX_FR : framesToSkip;
        size_t bytesToRead = chunk * BYTES_PER_FRAME;
        size_t bytesRead   = 0;

        esp_err_t err = i2s_read(I2S_PORT,
                                 dummy,
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

        framesToSkip -= framesRead;
        skipped      += framesRead;
    }

    return skipped;
}

size_t Sampler::fetch(float *dest, size_t framesRequested)
{
    if (!triggered || dest == nullptr || framesRequested == 0) {
        return 0;
    }

    // ---- capture write index at function entry for debug ----
    uint64_t w_start = tracker.writeIndex();

    // ---- Phase A: skip any overrun (bring hwReadIndex up to logicalReadIndex) ----
    uint32_t overrunSkip = tracker.framesToSkipOverrun();
    uint32_t skippedOverrun = 0;
    if (overrunSkip > 0) {
        skippedOverrun = skipFramesWithI2S(overrunSkip);
        tracker.onFramesRead(skippedOverrun);
    }

    // ---- Phase B: discard up to triggerIndex on first fetch after trigger ----
    uint32_t discardRaw   = 0;
    uint32_t discardCapped = 0;
    uint32_t discarded     = 0;

    if (!alignedToTrigger) {
        alignedToTrigger = true;

        // raw difference (can be > ringFrames)
        uint64_t logical = tracker.logicalReadIndex();
        if (triggerIndex > logical) {
            uint64_t diff = triggerIndex - logical;
            discardRaw = (diff > 0xFFFFFFFFULL) ? 0xFFFFFFFFu
                                                : static_cast<uint32_t>(diff);
        } else {
            discardRaw = 0;
        }

        discardCapped = tracker.framesToDiscardToTrigger(triggerIndex);
        if (discardCapped > 0) {
            discarded = skipFramesWithI2S(discardCapped);
            tracker.onFramesRead(discarded);
        }
    }

    // ---- capture write index after discard, before fetch ----
    uint64_t w_after_discard = tracker.writeIndex();

    // ---- Phase C: read the post-trigger frames into dest ----
    size_t totalFramesOut = 0;
    const size_t MAX_FRAMES_PER_READ = 32;
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

        int32_t *codes = reinterpret_cast<int32_t *>(tempBuf);

        for (size_t f = 0; f < framesRead; ++f) {
            int32_t codeL = codes[2 * f + 0];
            int32_t codeR = codes[2 * f + 1];

            dest[2 * (totalFramesOut + f) + 0] = codeToVoltage(codeL);
            dest[2 * (totalFramesOut + f) + 1] = codeToVoltage(codeR);
        }

        totalFramesOut += framesRead;
        tracker.onFramesRead(framesRead);

        if (bytesRead % BYTES_PER_FRAME != 0) {
            break;
        }
    }

    // ---- capture write index after fetch ----
    uint64_t w_end = tracker.writeIndex();

    uint64_t dW_discard = (w_after_discard >= w_start)
                            ? (w_after_discard - w_start)
                            : 0;
    uint64_t dW_fetch   = (w_end >= w_after_discard)
                            ? (w_end - w_after_discard)
                            : 0;

    // ---- print stats AFTER all critical work ----
    Serial.print("[stats] discardRaw=");
    Serial.print((unsigned long)discardRaw);
    Serial.print(" capped=");
    Serial.print((unsigned long)discardCapped);
    Serial.print(" discarded=");
    Serial.print((unsigned long)discarded);
    Serial.print("  dW_discard=");
    Serial.print((unsigned long)dW_discard);
    Serial.print("  dW_fetch=");
    Serial.println((unsigned long)dW_fetch);

    return totalFramesOut;
}

// Convert raw PCM1809 32-bit signed code to differential peak voltage.
// Assumes PCM1809 full scale is 2 Vrms differential => ~2.828 Vpeak.
float Sampler::codeToVoltage(int32_t code) const
{
    const float CODE_FS        = 2147483648.0f;                // 2^31
    const float VFS_DIFF_RMS   = 2.0f;                         // 2 Vrms diff FS
    const float VFS_DIFF_PEAK  = VFS_DIFF_RMS * 1.41421356237f; // ≈ 2.828 Vpeak
    return (static_cast<float>(code) / CODE_FS) * VFS_DIFF_PEAK;
}
