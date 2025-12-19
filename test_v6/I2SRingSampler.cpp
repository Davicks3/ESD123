// I2SRingSampler.cpp
#include "I2SRingSampler.h"

I2SRingSampler::I2SRingSampler(size_t ringFrames)
: ringSize(ringFrames),
  ringL(nullptr),
  ringR(nullptr),
  writeIndex(0),
  triggerIndex(0)
{
    // Allocate ring buffers in heap
    ringL = (int32_t*)malloc(ringSize * sizeof(int32_t));
    ringR = (int32_t*)malloc(ringSize * sizeof(int32_t));
}

bool I2SRingSampler::begin(int bclkPin, int lrclkPin, int dataInPin)
{
    if (!ringL || !ringR) {
        return false;
    }

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
        .bck_io_num   = bclkPin,
        .ws_io_num    = lrclkPin,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = dataInPin
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

    // ---- ADC settle: discard first ~100 ms of samples ----
    const uint32_t settleFrames = SAMPLE_RATE_HZ / 10;  // 100 ms
    const uint32_t settleBytes  = settleFrames * BYTES_PER_FRAME;

    const size_t CHUNK_BYTES = DMA_BUF_LEN * BYTES_PER_FRAME;
    uint8_t *dummy = (uint8_t*)malloc(CHUNK_BYTES);
    if (!dummy) return false;

    uint32_t discarded = 0;
    while (discarded < settleBytes) {
        size_t toRead = settleBytes - discarded;
        if (toRead > CHUNK_BYTES) toRead = CHUNK_BYTES;

        size_t bytesRead = 0;
        esp_err_t err = i2s_read(I2S_PORT, dummy, toRead, &bytesRead, portMAX_DELAY);
        if (err != ESP_OK || bytesRead == 0) {
            break;
        }
        discarded += (uint32_t)bytesRead;
    }

    free(dummy);

    writeIndex   = 0;
    triggerIndex = 0;

    return true;
}

void I2SRingSampler::service()
{
    // Read as big chunks as possible to minimize overhead.
    const size_t MAX_FRAMES_PER_READ = DMA_BUF_LEN; // same as one DMA buffer
    const size_t BUF_BYTES = MAX_FRAMES_PER_READ * BYTES_PER_FRAME;

    static uint8_t tempBuf[DMA_BUF_LEN * BYTES_PER_FRAME];

    size_t bytesRead = 0;
    esp_err_t err = i2s_read(I2S_PORT,
                             tempBuf,
                             BUF_BYTES,
                             &bytesRead,
                             portMAX_DELAY);
    if (err != ESP_OK || bytesRead == 0) {
        return;
    }

    size_t framesRead = bytesRead / BYTES_PER_FRAME;
    if (framesRead == 0) {
        return;
    }

    int32_t *codes = reinterpret_cast<int32_t*>(tempBuf);

    // Copy into software ring buffer.
    uint32_t baseIndex = writeIndex;

    for (size_t f = 0; f < framesRead; ++f) {
        uint32_t globalFrame = baseIndex + f;
        uint32_t idx = globalFrame % ringSize;

        int32_t codeL = codes[f * 2 + 0];
        int32_t codeR = codes[f * 2 + 1];

        ringL[idx] = codeL;
        ringR[idx] = codeR;
    }

    // Update global write index *once* at the end.
    writeIndex = baseIndex + (uint32_t)framesRead;
}

void I2SRingSampler::markTrigger()
{
    // Just snapshot current writeIndex; any small race is << 1 frame.
    triggerIndex = writeIndex;
}

bool I2SRingSampler::hasPostTriggerWindow(uint32_t framesNeeded) const
{
    // Handle wrap by signed subtraction
    int32_t diff = (int32_t)(writeIndex - triggerIndex);
    return diff >= (int32_t)framesNeeded;
}

bool I2SRingSampler::extractWindow(float* dest, uint32_t framesNeeded) const
{
    if (!dest) return false;
    if (!hasPostTriggerWindow(framesNeeded)) return false;
    if (framesNeeded > ringSize) return false;

    for (uint32_t f = 0; f < framesNeeded; ++f) {
        uint32_t idx = (triggerIndex + f) % ringSize;
        int32_t codeL = ringL[idx];
        int32_t codeR = ringR[idx];

        dest[2 * f + 0] = codeToVoltage(codeL);
        dest[2 * f + 1] = codeToVoltage(codeR);
    }
    return true;
}

// Full-scale: 2 Vrms differential => ~2.828 Vpeak diff
float I2SRingSampler::codeToVoltage(int32_t code) const
{
    const float CODE_FS        = 2147483648.0f;               // 2^31
    const float VFS_DIFF_RMS   = 2.0f;                        // PCM1809 full-scale Vrms
    const float VFS_DIFF_PEAK  = VFS_DIFF_RMS * 1.41421356237f; // ≈ 2.828 V
    return (static_cast<float>(code) / CODE_FS) * VFS_DIFF_PEAK;
}