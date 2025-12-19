#include "PCNTSampler.h"

// Uncomment to see debug on Serial
//#define SAMPLER_DEBUG 1

PCNTSampler::PCNTSampler(int bclkPin,
                         int lrclkPin,
                         int dataInPin,
                         int triggerPin,
                         int captureFrames)
    : bclkPin(bclkPin),
      lrclkPin(lrclkPin),
      dataInPin(dataInPin),
      triggerPin(triggerPin),
      captureFrames(captureFrames)
{
    captureBuffer = (float*)malloc(sizeof(float) * captureFrames * 2);
}

// --- ISR: just mark trigger requested ---
void IRAM_ATTR PCNTSampler::onTriggerISR()
{
    triggerRequested = true;
}

bool PCNTSampler::setupI2S()
{
    i2s_config_t cfg = {
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

    i2s_pin_config_t pins = {
        .bck_io_num   = bclkPin,
        .ws_io_num    = lrclkPin,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = dataInPin
    };

    if (i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) != ESP_OK) {
        return false;
    }
    if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) {
        return false;
    }
    if (i2s_set_clk(I2S_PORT, SAMPLE_RATE_HZ, BITS_PER_SAMPLE, I2S_CHANNEL_STEREO) != ESP_OK) {
        return false;
    }

    i2s_start(I2S_PORT);
    return true;
}

bool PCNTSampler::setupPCNT()
{
    pcnt_config_t pcntConfig = {};
    pcntConfig.pulse_gpio_num = 14;
    pcntConfig.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    pcntConfig.channel        = PCNT_CH;
    pcntConfig.unit           = PCNT_UNIT;
    pcntConfig.pos_mode       = PCNT_COUNT_INC;
    pcntConfig.neg_mode       = PCNT_COUNT_DIS;
    pcntConfig.lctrl_mode     = PCNT_MODE_KEEP;
    pcntConfig.hctrl_mode     = PCNT_MODE_KEEP;
    pcntConfig.counter_h_lim  = 32767;
    pcntConfig.counter_l_lim  = 0;

    if (pcnt_unit_config(&pcntConfig) != ESP_OK) {
        return false;
    }
    if (pcnt_counter_clear(PCNT_UNIT) != ESP_OK) {
        return false;
    }
    if (pcnt_counter_resume(PCNT_UNIT) != ESP_OK) {
        return false;
    }

    return true;
}

bool PCNTSampler::begin()
{
    if (!captureBuffer) return false;

    pinMode(triggerPin, INPUT);

    if (!setupI2S()) return false;
    if (!setupPCNT()) return false;

    // Reset counters
    producedFrames = 0;
    consumedFrames = 0;
    triggerFrame   = 0;
    state          = IDLE;
    captureDone    = false;
    captureCount   = 0;
    triggerRequested = false;

    // Allow everything to stabilize and discard ~100 ms of garbage
    discardInitialSettle();

#ifdef SAMPLER_DEBUG
    Serial.println("[Sampler::begin] done, ready for triggers");
#endif

    return true;
}

void PCNTSampler::discardInitialSettle()
{
    const uint32_t settleFrames = SAMPLE_RATE_HZ / 10; // 100 ms
    const uint32_t settleBytes  = settleFrames * BYTES_PER_FRAME;

    uint32_t discardedBytes = 0;
    uint8_t buf[MAX_FRAMES_PER_READ * BYTES_PER_FRAME];

    while (discardedBytes < settleBytes) {
        size_t toFrames = (settleBytes - discardedBytes) / BYTES_PER_FRAME;
        if (toFrames > MAX_FRAMES_PER_READ) toFrames = MAX_FRAMES_PER_READ;
        if (toFrames == 0) break;

        size_t rdFrames = readFrames((int)toFrames, buf, pdMS_TO_TICKS(50));
        if (rdFrames == 0) break;

        discardedBytes += rdFrames * BYTES_PER_FRAME;
        consumedFrames += rdFrames;
    }

#ifdef SAMPLER_DEBUG
    Serial.print("[Sampler::discardInitialSettle] discardedBytes=");
    Serial.print(discardedBytes);
    Serial.print(" consumedFrames=");
    Serial.println((unsigned long long)consumedFrames);
#endif
}

void PCNTSampler::updateFromPCNT()
{
    int16_t count = 0;
    pcnt_get_counter_value(PCNT_UNIT, &count);
    if (count != 0) {
        pcnt_counter_clear(PCNT_UNIT);
        // Each LRCLK pulse = 1 frame (stereo)
        producedFrames += (int16_t)count;
    }
}

// helper around i2s_read - returns frames actually read
size_t PCNTSampler::readFrames(int frames, uint8_t* buf, TickType_t timeoutTicks)
{
    if (frames <= 0) return 0;
    size_t bytesToRead = (size_t)frames * BYTES_PER_FRAME;
    size_t bytesRead   = 0;
    esp_err_t err = i2s_read(I2S_PORT, buf, bytesToRead, &bytesRead, timeoutTicks);
    if (err != ESP_OK || bytesRead == 0) return 0;
    return bytesRead / BYTES_PER_FRAME;
}

float PCNTSampler::codeToVoltage(int32_t code) const
{
    const float CODE_FS       = 2147483648.0f;            // 2^31
    const float VFS_DIFF_RMS  = 2.0f;                     // 2 Vrms differential
    const float VFS_DIFF_PEAK = VFS_DIFF_RMS * 1.41421356237f;
    return (float)code / CODE_FS * VFS_DIFF_PEAK;
}

bool PCNTSampler::service()
{
    // Fold PCNT into producedFrames before we make any decisions
    updateFromPCNT();

    // --- handle new trigger ---
    if (triggerRequested) {
        triggerRequested = false;
        triggerFrame     = producedFrames;
        state            = ALIGNING;
        captureDone      = false;
        captureCount     = 0;

#ifdef SAMPLER_DEBUG
        Serial.print("[Sampler::service] TRIGGER at frame ");
        Serial.println((unsigned long long)triggerFrame);
#endif
    }

    // --- IDLE: just flush to keep lag small ---
    if (state == IDLE) {
        uint64_t pending = producedFrames - consumedFrames;
        if (pending > 0) {
            uint64_t chunk = pending;
            if (chunk > MAX_FRAMES_PER_READ) chunk = MAX_FRAMES_PER_READ;

            uint8_t buf[MAX_FRAMES_PER_READ * BYTES_PER_FRAME];
            size_t rd = readFrames((int)chunk, buf, 0);
            consumedFrames += rd;
        }
        return false;
    }

    // --- ALIGNING: discard until consumedFrames == triggerFrame ---
    if (state == ALIGNING) {
        int64_t toTrigger = (int64_t)triggerFrame - (int64_t)consumedFrames;
        if (toTrigger <= 0) {
            state = CAPTURING;
#ifdef SAMPLER_DEBUG
            Serial.println("[Sampler::service] aligned to trigger, starting capture");
#endif
            return false;
        }

        uint64_t pending = producedFrames - consumedFrames;
        if (pending == 0) {
            return false; // nothing new yet
        }

        uint64_t chunk = (uint64_t)toTrigger;
        if (chunk > pending) chunk = pending;
        if (chunk > MAX_FRAMES_PER_READ) chunk = MAX_FRAMES_PER_READ;

        uint8_t buf[MAX_FRAMES_PER_READ * BYTES_PER_FRAME];
        size_t rd = readFrames((int)chunk, buf, 0);
        if (rd > 0) {
            consumedFrames += rd;
        }
        return false;
    }

    // --- CAPTURING: store captureFrames frames into captureBuffer ---
    if (state == CAPTURING) {
        if (captureCount >= captureFrames) {
            state       = IDLE;
            captureDone = true;
#ifdef SAMPLER_DEBUG
            Serial.println("[Sampler::service] capture complete");
#endif
            return true;
        }

        uint64_t pending = producedFrames - consumedFrames;
        if (pending == 0) {
            return false; // wait for more data
        }

        uint64_t remaining = (uint64_t)captureFrames - (uint64_t)captureCount;
        uint64_t chunk = remaining;
        if (chunk > pending) chunk = pending;
        if (chunk > MAX_FRAMES_PER_READ) chunk = MAX_FRAMES_PER_READ;

        uint8_t buf[MAX_FRAMES_PER_READ * BYTES_PER_FRAME];
        size_t rd = readFrames((int)chunk, buf, 0);
        if (rd == 0) {
            return false;
        }

        int32_t* codes = (int32_t*)buf;

        for (size_t i = 0; i < rd; ++i) {
            int32_t L = codes[i * 2 + 0];
            int32_t R = codes[i * 2 + 1];

            captureBuffer[(captureCount + i) * 2 + 0] = codeToVoltage(L);
            captureBuffer[(captureCount + i) * 2 + 1] = codeToVoltage(R);
        }

        captureCount   += rd;
        consumedFrames += rd;

        if (captureCount >= captureFrames) {
            state       = IDLE;
            captureDone = true;
#ifdef SAMPLER_DEBUG
            Serial.println("[Sampler::service] capture complete");
#endif
            return true;
        }

        return false;
    }

    return false;
}
