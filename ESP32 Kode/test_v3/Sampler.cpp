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
        .bck_io_num   = BCK_PIN,
        .ws_io_num    = WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = DATA_IN_PIN
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

    // Start once and keep it running
    i2s_start(I2S_PORT);

    // One-time warm-up: discard first ~100 ms (PLL, filters, etc.)
    const size_t warmupFrames = SAMPLE_RATE_HZ / 10;  // ~100 ms
    const size_t warmupBytes  = warmupFrames * BYTES_PER_FRAME;

    Serial.println("[Sampler::begin] warmup flush...");
    uint8_t dummy[256];
    size_t discarded = 0;
    while (discarded < warmupBytes) {
        size_t toRead = warmupBytes - discarded;
        if (toRead > sizeof(dummy)) {
            toRead = sizeof(dummy);
        }

        size_t bytesRead = 0;
        esp_err_t err = i2s_read(I2S_PORT, dummy, toRead, &bytesRead, portMAX_DELAY);
        if (err != ESP_OK || bytesRead == 0) {
            Serial.print("[Sampler::begin] warmup i2s_read failed, err=");
            Serial.println(err);
            break;
        }
        discarded += bytesRead;
    }

    Serial.println("[Sampler::begin] I2S initialized and warmed up");
    return true;
}

void Sampler::flushRing()
{
    // Discard exactly RING_FRAMES frames (best-effort)
    const size_t flushFrames = RING_FRAMES;
    const size_t flushBytes  = flushFrames * BYTES_PER_FRAME;

    // Use a reasonably large buffer to minimize overhead
    // 128 frames * 8 bytes = 1024 bytes per read
    const size_t FRAMES_PER_READ = 128;
    uint8_t dummy[FRAMES_PER_READ * BYTES_PER_FRAME];

    size_t framesRemaining = flushFrames;

    while (framesRemaining > 0) {
        size_t framesThis = (framesRemaining > FRAMES_PER_READ)
                            ? FRAMES_PER_READ
                            : framesRemaining;

        size_t bytesToRead = framesThis * BYTES_PER_FRAME;
        size_t bytesRead   = 0;

        esp_err_t err = i2s_read(
            I2S_PORT,
            dummy,
            bytesToRead,
            &bytesRead,
            portMAX_DELAY
        );
        if (err != ESP_OK || bytesRead == 0) {
            Serial.print("[Sampler::flushRing] i2s_read failed, err=");
            Serial.println(err);
            break;
        }

        size_t framesRead = bytesRead / BYTES_PER_FRAME;
        if (framesRead == 0) {
            break;
        }

        if (framesRead > framesRemaining) {
            framesRead = framesRemaining;
        }
        framesRemaining -= framesRead;
    }
}

size_t Sampler::capture1024(float *dest)
{
    if (!dest) return 0;

    // 1) Flush current ring contents (anything that existed at trigger time)
    flushRing();

    // 2) Capture exactly CAPTURE_FRAMES frames after the flush
    const size_t framesNeeded = CAPTURE_FRAMES;
    const size_t framesPerRead = 128;  // 128 frames per chunk
    uint8_t tempBuf[framesPerRead * BYTES_PER_FRAME];

    size_t framesCaptured = 0;

    while (framesCaptured < framesNeeded) {
        size_t framesThis = framesNeeded - framesCaptured;
        if (framesThis > framesPerRead) {
            framesThis = framesPerRead;
        }

        size_t bytesToRead = framesThis * BYTES_PER_FRAME;
        size_t bytesRead   = 0;

        esp_err_t err = i2s_read(
            I2S_PORT,
            tempBuf,
            bytesToRead,
            &bytesRead,
            portMAX_DELAY
        );
        if (err != ESP_OK || bytesRead == 0) {
            Serial.print("[Sampler::capture1024] i2s_read failed/zero, err=");
            Serial.println(err);
            break;
        }

        size_t framesRead = bytesRead / BYTES_PER_FRAME;
        if (framesRead == 0) {
            break;
        }

        int32_t *codes = reinterpret_cast<int32_t *>(tempBuf);

        for (size_t f = 0; f < framesRead; ++f) {
            int32_t codeL = codes[2*f + 0];
            int32_t codeR = codes[2*f + 1];

            dest[2*(framesCaptured + f) + 0] = codeToVoltage(codeL);
            dest[2*(framesCaptured + f) + 1] = codeToVoltage(codeR);
        }

        framesCaptured += framesRead;
    }

    return framesCaptured;
}

float Sampler::codeToVoltage(int32_t code)
{
    // PCM1809: 2 Vrms diff → ~2.828 Vpeak diff
    const float CODE_FS        = 2147483648.0f;          // 2^31
    const float VFS_DIFF_RMS   = 2.0f;
    const float VFS_DIFF_PEAK  = VFS_DIFF_RMS * 1.41421356237f;
    return (static_cast<float>(code) / CODE_FS) * VFS_DIFF_PEAK;
}