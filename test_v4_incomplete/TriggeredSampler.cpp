#include "TriggeredSampler.h"

// ---- ctor ----
TriggeredSampler::TriggeredSampler(int bclkPin, int lrclkPin, int dataPin, int lrclkPcntPin)
: bclkPin(bclkPin),
  lrclkPin(lrclkPin),
  dataPin(dataPin),
  lrclkPcntPin(lrclkPcntPin),
  triggerRequested(false),
  state(IDLE),
  producedFrames(0),
  consumedFrames(0),
  triggerFrame(0),
  captured(0),
  captureReady(false)
{}

// ---- public ----
bool TriggeredSampler::begin()
{
    Serial.println("[Sampler] begin()");

    if (!initI2S()) {
        Serial.println("[Sampler] I2S init failed");
        return false;
    }

    pinMode(lrclkPcntPin, INPUT);

    if (!frameCounter.begin((gpio_num_t)lrclkPcntPin)) {
        Serial.println("[Sampler] FrameCounter init failed");
        return false;
    }

    // Let ADC settle by discarding ~100ms of samples.
    settleADC();

    // Reset frame counters after settling so 0 = "start of real operation".
    frameCounter.reset();
    producedFrames  = 0;
    consumedFrames  = 0;
    triggerFrame    = 0;
    captured        = 0;
    captureReady    = false;
    state           = IDLE;

    Serial.println("[Sampler] begin() done");
    return true;
}

void TriggeredSampler::requestTrigger()
{
    // Just a flag; main loop will handle state transition.
    triggerRequested = true;
}

void TriggeredSampler::service()
{
    // Update global producedFrames from PCNT.
    frameCounter.update();
    producedFrames = frameCounter.getTotal();

    switch (state) {
    case IDLE:
        // handle new trigger
        if (triggerRequested) {
            triggerRequested = false;
            triggerFrame     = producedFrames;
            captured         = 0;
            captureReady     = false;
            state            = ALIGNING;

            // Debug:
            // Serial.print("[Sampler] Trigger at producedFrames=");
            // Serial.println(triggerFrame);
        }
        idleStep();
        break;

    case ALIGNING:
        aligningStep();
        break;

    case CAPTURING:
        capturingStep();
        break;
    }
}

// ---- private: I2S + settle ----
bool TriggeredSampler::initI2S()
{
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    cfg.sample_rate = SAMPLE_RATE_HZ;
    cfg.bits_per_sample = BITS_PER_SAMPLE;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = DMA_BUF_COUNT;
    cfg.dma_buf_len = DMA_BUF_LEN;
    cfg.use_apll = true;
    cfg.tx_desc_auto_clear = false;
    cfg.fixed_mclk = 0;

    i2s_pin_config_t pins = {};
    pins.bck_io_num   = bclkPin;
    pins.ws_io_num    = lrclkPin;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num  = dataPin;

    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) {
        return false;
    }
    if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
        return false;
    }
    if (i2s_set_clk(I2S_NUM_0, SAMPLE_RATE_HZ, BITS_PER_SAMPLE, I2S_CHANNEL_STEREO) != ESP_OK) {
        return false;
    }
    return true;
}

void TriggeredSampler::settleADC()
{
    const uint32_t settleFrames = SAMPLE_RATE_HZ / 10;  // ~100ms
    const uint32_t settleBytes  = settleFrames * BYTES_PER_FRAME;

    uint8_t dummy[256];
    uint32_t discarded = 0;

    Serial.println("[Sampler] Settling ADC...");

    while (discarded < settleBytes) {
        size_t toRead = settleBytes - discarded;
        if (toRead > sizeof(dummy)) {
            toRead = sizeof(dummy);
        }

        size_t bytesRead = 0;
        esp_err_t err = i2s_read(I2S_NUM_0,
                                 dummy,
                                 toRead,
                                 &bytesRead,
                                 portMAX_DELAY);
        if (err != ESP_OK || bytesRead == 0) {
            Serial.print("[Sampler] settle i2s_read err=");
            Serial.println(err);
            break;
        }
        discarded += (uint32_t)bytesRead;
    }

    Serial.println("[Sampler] Settling done");
}

// ---- private: state steps ----
void TriggeredSampler::idleStep()
{
    // Keep DMA mostly empty by discarding any pending frames.
    uint64_t pending = producedFrames - consumedFrames;
    if (pending == 0) return;

    uint64_t framesToFlush = pending;
    if (framesToFlush > FLUSH_CHUNK) {
        framesToFlush = FLUSH_CHUNK;
    }

    size_t bytesToRead = (size_t)(framesToFlush * BYTES_PER_FRAME);
    uint8_t dummy[FLUSH_CHUNK * BYTES_PER_FRAME];
    if (bytesToRead > sizeof(dummy)) {
        bytesToRead = sizeof(dummy);
    }

    size_t bytesRead = 0;
    esp_err_t err = i2s_read(I2S_NUM_0,
                             dummy,
                             bytesToRead,
                             &bytesRead,
                             0); // non-blocking-ish: 0 timeout
    if (err != ESP_OK || bytesRead == 0) {
        return;
    }
    size_t framesRead = bytesRead / BYTES_PER_FRAME;
    consumedFrames += framesRead;
}

void TriggeredSampler::aligningStep()
{
    uint64_t pending = producedFrames - consumedFrames;

    int64_t toTrigger = (int64_t)triggerFrame - (int64_t)consumedFrames;
    if (toTrigger <= 0) {
        // We're at or past trigger frame.
        state = CAPTURING;
        return;
    }

    if (pending == 0) {
        // Nothing to discard yet.
        return;
    }

    uint64_t framesToDiscard = (uint64_t)toTrigger;
    if (framesToDiscard > pending) {
        framesToDiscard = pending;
    }
    if (framesToDiscard > FLUSH_CHUNK) {
        framesToDiscard = FLUSH_CHUNK;
    }

    size_t bytesToRead = (size_t)(framesToDiscard * BYTES_PER_FRAME);
    uint8_t dummy[FLUSH_CHUNK * BYTES_PER_FRAME];
    if (bytesToRead > sizeof(dummy)) {
        bytesToRead = sizeof(dummy);
    }

    size_t bytesRead = 0;
    esp_err_t err = i2s_read(I2S_NUM_0,
                             dummy,
                             bytesToRead,
                             &bytesRead,
                             0);
    if (err != ESP_OK || bytesRead == 0) {
        return;
    }

    size_t framesRead = bytesRead / BYTES_PER_FRAME;
    consumedFrames += framesRead;

    // If we just consumed past trigger, next step will flip to CAPTURING.
}

void TriggeredSampler::capturingStep()
{
    if (captured >= CAPTURE_FRAMES) {
        // Shouldn't happen; just safety.
        state = IDLE;
        captureReady = true;
        return;
    }

    uint64_t pending   = producedFrames - consumedFrames;
    uint64_t remaining = CAPTURE_FRAMES - captured;

    if (pending == 0) {
        // Nothing new to read yet.
        return;
    }

    uint64_t framesToRead = pending;
    if (framesToRead > remaining) {
        framesToRead = remaining;
    }
    if (framesToRead > CAPTURE_CHUNK) {
        framesToRead = CAPTURE_CHUNK;
    }

    size_t bytesToRead = (size_t)(framesToRead * BYTES_PER_FRAME);
    uint8_t temp[CAPTURE_CHUNK * BYTES_PER_FRAME];
    if (bytesToRead > sizeof(temp)) {
        bytesToRead = sizeof(temp);
    }

    size_t bytesRead = 0;
    esp_err_t err = i2s_read(I2S_NUM_0,
                             temp,
                             bytesToRead,
                             &bytesRead,
                             0);
    if (err != ESP_OK || bytesRead == 0) {
        return;
    }

    size_t framesRead = bytesRead / BYTES_PER_FRAME;
    if (framesRead == 0) {
        return;
    }

    int32_t *codes = reinterpret_cast<int32_t *>(temp);

    for (size_t f = 0; f < framesRead; ++f) {
        int32_t codeL = codes[f * 2 + 0];
        int32_t codeR = codes[f * 2 + 1];

        size_t idx = (captured + f) * 2;
        captureBuf[idx + 0] = codeToVoltage(codeL);
        captureBuf[idx + 1] = codeToVoltage(codeR);
    }

    captured       += framesRead;
    consumedFrames += framesRead;

    if (captured >= CAPTURE_FRAMES) {
        state        = IDLE;
        captureReady = true;
    }
}

float TriggeredSampler::codeToVoltage(int32_t code)
{
    // PCM1809: 2 Vrms diff → ~2.828 Vpeak diff
    const float CODE_FS        = 2147483648.0f;          // 2^31
    const float VFS_DIFF_RMS   = 2.0f;
    const float VFS_DIFF_PEAK  = VFS_DIFF_RMS * 1.41421356237f;
    return (static_cast<float>(code) / CODE_FS) * VFS_DIFF_PEAK;
}