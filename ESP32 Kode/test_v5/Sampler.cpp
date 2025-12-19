#include "Sampler.h"

// ---- Constructor ----
Sampler::Sampler(int bckPin, int wsPin, int dataInPin)
: _bckPin(bckPin), _wsPin(wsPin), _dataPin(dataInPin)
{
}

// ---- Begin: configure I2S, flush first 100 ms ----
bool Sampler::begin()
{
    // I2S configuration
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE_HZ,
        .bits_per_sample = BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,  // L/R interleaved
        .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = true,          // better clocking for audio
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num   = _bckPin,
        .ws_io_num    = _wsPin,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = _dataPin
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

    // ---- Flush first 100 ms for ADC/PLL settle ----
    const uint32_t settleFrames = SAMPLE_RATE_HZ / 10;  // 100 ms
    const uint32_t settleBytes  = settleFrames * BYTES_PER_FRAME;

    uint8_t dummy[256];
    uint32_t discarded = 0;

    Serial.println("[Sampler::begin] Flushing first 100 ms of samples...");

    while (discarded < settleBytes) {
        size_t toRead = settleBytes - discarded;
        if (toRead > sizeof(dummy)) {
            toRead = sizeof(dummy);
        }

        size_t bytesRead = 0;
        esp_err_t err = i2s_read(I2S_PORT,
                                 dummy,
                                 toRead,
                                 &bytesRead,
                                 portMAX_DELAY);
        if (err != ESP_OK || bytesRead == 0) {
            Serial.print("[Sampler::begin] i2s_read during settle failed, err=");
            Serial.println(err);
            break;
        }

        discarded += (uint32_t)bytesRead;
    }

    Serial.println("[Sampler::begin] Settle done.");
    return true;
}

// ---- Discard a small chunk of samples ----
// Call this continuously in loop() while waiting for trigger.
void Sampler::discardChunk()
{
    uint8_t buf[DISCARD_CHUNK_FRAMES * BYTES_PER_FRAME];
    size_t bytesToRead = sizeof(buf);
    size_t bytesRead   = 0;

    // Blocking until this small chunk is available.
    i2s_read(I2S_PORT,
             buf,
             bytesToRead,
             &bytesRead,
             portMAX_DELAY);

    // We don't care about the data; it is thrown away.
}

// ---- Capture N frames after trigger ----
// dest: float array of size at least framesRequested * 2
size_t Sampler::capture(float* dest, size_t framesRequested)
{
    if (!dest || framesRequested == 0) {
        return 0;
    }

    const size_t MAX_FRAMES_PER_READ = 64;  // chunk size
    uint8_t tempBuf[MAX_FRAMES_PER_READ * BYTES_PER_FRAME];

    size_t totalFramesOut = 0;

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
            Serial.print("[Sampler::capture] i2s_read failed or zero, err=");
            Serial.println(err);
            break;
        }

        size_t framesRead = bytesRead / BYTES_PER_FRAME;
        if (framesRead == 0) {
            break;
        }

        // Interpret as interleaved int32 L/R
        int32_t *codes = reinterpret_cast<int32_t *>(tempBuf);

        for (size_t f = 0; f < framesRead; ++f) {
            int32_t codeL = codes[f * 2 + 0];
            int32_t codeR = codes[f * 2 + 1];

            dest[(totalFramesOut + f) * 2 + 0] = codeToVoltage(codeL);
            dest[(totalFramesOut + f) * 2 + 1] = codeToVoltage(codeR);
        }

        totalFramesOut += framesRead;

        if ((bytesRead % BYTES_PER_FRAME) != 0) {
            Serial.println("[Sampler::capture] bytesRead not multiple of BYTES_PER_FRAME; breaking");
            break;
        }
    }

    return totalFramesOut;
}

// ---- Convert raw PCM1809 code (32-bit signed) to differential peak voltage ----
// Assuming 2 Vrms differential full scale => 2.828 Vpeak differential.
float Sampler::codeToVoltage(int32_t code) const
{
    const float CODE_FS        = 2147483648.0f;          // 2^31
    const float VFS_DIFF_RMS   = 2.0f;                   // PCM1809 datasheet full-scale (Vrms diff)
    const float VFS_DIFF_PEAK  = VFS_DIFF_RMS * 1.41421356237f; // ≈ 2.828 Vpeak
    return (static_cast<float>(code) / CODE_FS) * VFS_DIFF_PEAK;
}