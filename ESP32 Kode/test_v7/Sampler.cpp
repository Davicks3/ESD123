#include "Sampler.h"

bool Sampler::begin()
{
    // ---- I2S config ----
    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    cfg.sample_rate          = SAMPLE_RATE;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_I2S;
    cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count        = 8;     // 8 * 128 frames = 1024 frames in DMA
    cfg.dma_buf_len          = 128;   // frames per DMA buffer
    cfg.use_apll             = true;
    cfg.tx_desc_auto_clear   = false;
    cfg.fixed_mclk           = 0;

    i2s_pin_config_t pins = {};
    pins.bck_io_num   = PIN_BCK;
    pins.ws_io_num    = PIN_WS;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num  = PIN_DIN;

    if (i2s_driver_install(PORT, &cfg, 0, nullptr) != ESP_OK) return false;
    if (i2s_set_pin(PORT, &pins) != ESP_OK) return false;
    if (i2s_set_clk(PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_STEREO) != ESP_OK) return false;

    // ---- LRCLK PCNT ----
    if (!lrclk.begin((gpio_num_t)PIN_LRCLK_PCNT)) return false;

    // ---- Settle ADC: discard first 100 ms ----
    settleAdc();

    writeIndex     = 0;
    triggered      = false;
    hwTrigger      = 0;
    hwAtWriteIndex = lrclk.read32();
    return true;
}

void Sampler::settleAdc()
{
    const uint32_t settleFrames = SAMPLE_RATE / 10;    // 100 ms
    const uint32_t settleBytes  = settleFrames * BYTES_PER_FRAME;

    uint8_t dummy[512];
    uint32_t discarded = 0;

    while (discarded < settleBytes) {
        size_t toRead = settleBytes - discarded;
        if (toRead > sizeof(dummy)) toRead = sizeof(dummy);

        size_t bytesRead = 0;
        esp_err_t err = i2s_read(PORT, dummy, toRead, &bytesRead, portMAX_DELAY);
        if (err != ESP_OK || bytesRead == 0) break;

        discarded += (uint32_t)bytesRead;

        // Keep LRCLK count in sync
        lrclk.update();
    }
}

void Sampler::service()
{
    // Read as much as possible in reasonably large chunks to reduce overhead.
    // Here: up to 256 frames (≈ 1.33 ms).
    const size_t MAX_FRAMES = 256;
    uint8_t buf[MAX_FRAMES * BYTES_PER_FRAME];

    size_t bytesRead = 0;
    esp_err_t err = i2s_read(PORT, buf, sizeof(buf), &bytesRead, 0);  // non-blocking
    if (err != ESP_OK || bytesRead == 0) {
        return;
    }

    size_t framesRead = bytesRead / BYTES_PER_FRAME;
    if (framesRead == 0) return;

    // Interpreting as interleaved int32_t L/R
    int32_t* src = reinterpret_cast<int32_t*>(buf);

    // Copy into ring with wrap
    uint64_t wIdx = writeIndex;
    for (size_t f = 0; f < framesRead; ++f) {
        size_t ringIdx = (wIdx + f) % RING_FRAMES;
        ring[ringIdx * 2 + 0] = src[f * 2 + 0];  // L
        ring[ringIdx * 2 + 1] = src[f * 2 + 1];  // R
    }

    // Update writeIndex atomically-ish
    writeIndex = wIdx + framesRead;

    // Update LRCLK counter and latch mapping
    lrclk.update();
    hwAtWriteIndex = lrclk.read32();
}

void Sampler::onTriggerISR()
{
    // Called from GPIO interrupt; keep it tiny
    lrclk.update();              // bring hwCount32 up to date
    hwTrigger = lrclk.read32();  // snapshot at trigger
    triggered = true;
}

// Fetch frames starting at the trigger moment.
// Returns number of frames actually copied (0 on error).
size_t Sampler::fetchFromTrigger(int32_t* dest, size_t framesWanted)
{
    if (!triggered || dest == nullptr || framesWanted == 0) {
        return 0;
    }

    // Snapshot volatile members with interrupts disabled for consistency
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    taskENTER_CRITICAL(&mux);
    uint64_t wIdx          = writeIndex;
    uint32_t hwWrite       = hwAtWriteIndex;
    uint32_t hwTrig        = hwTrigger;
    taskEXIT_CRITICAL(&mux);

    // How many frames between trigger and current write position in HW time?
    // (signed to handle minor wraparound, but ring is small anyway)
    int32_t framesBetween = (int32_t)(hwWrite - hwTrig);

    if (framesBetween < 0) {
        // Trigger "in the future" relative to our last mapping → not enough data yet
        // You can choose to spin/wait in user code until this becomes >= 0.
        return 0;
    }

    // If trigger is too old and has been overwritten by ring wrap, we can't recover it.
    if ((uint32_t)framesBetween >= RING_FRAMES) {
        // Missed trigger; data overwritten
        return 0;
    }

    // Software frame index corresponding to trigger
    uint64_t triggerFrameIndex = wIdx - (uint32_t)framesBetween;

    // Now copy out framesWanted starting at triggerFrameIndex
    if (framesWanted > RING_FRAMES) {
        framesWanted = RING_FRAMES;  // cap to ring size
    }

    for (size_t i = 0; i < framesWanted; ++i) {
        size_t ringIdx = (triggerFrameIndex + i) % RING_FRAMES;
        dest[i * 2 + 0] = ring[ringIdx * 2 + 0];
        dest[i * 2 + 1] = ring[ringIdx * 2 + 1];
    }

    // Optional: clear triggered flag if it's a one-shot
    // triggered = false;

    return framesWanted;
}

float Sampler::codeToVoltage(int32_t code) const
{
    // PCM1809: 2 Vrms diff FS ≈ 2.828 Vpeak differential
    const float CODE_FS       = 2147483648.0f;          // 2^31
    const float VFS_DIFF_RMS  = 2.0f;
    const float VFS_DIFF_PEAK = VFS_DIFF_RMS * 1.41421356237f;
    return (static_cast<float>(code) / CODE_FS) * VFS_DIFF_PEAK;
}
